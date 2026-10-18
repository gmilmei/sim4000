# Console and Operator Input Policy

This policy defines the portable input contract for the native emulator. It
does not require XTerm, a particular desktop, or key combinations commonly
reserved by terminal emulators or window managers.

This is a design policy, not a claim that every requirement below is already
implemented. Interactive local guest input is ASCII-only: the frontend consumes
common ANSI sequences and bracketed-paste markers and visibly rejects high-bit
bytes. It does not translate UTF-8 or provide guest line editing. Telnet consumes
supported ANSI sequences and discards high-bit input; it does not decode UTF-8.
Readline editing applies only
at the operator prompt. See [Compatibility and Limitations](../COMPATIBILITY.md)
for the implemented baseline and known boundaries.

## Separation of input paths

The emulator has two logically separate input paths:

1. The RC console data path carries characters to the selected emulated
   console device.
2. The operator control path carries commands to the emulator itself.

The machine core and device implementations must not decode host escape
sequences or terminal shortcuts. Frontends perform that work and invoke a
UI-independent machine-control API. A local terminal frontend may multiplex
both paths on one terminal, while a Telnet console is a data-only connection
by default. A future local control socket or dedicated operator frontend can
provide the control path without reserving any console character.

## Operator interface

The operator frontend is a SIMH-style, line-oriented simulator
interface. It adopts the interaction model, not strict SIMH command syntax or
implementation compatibility. GNU Readline supplies line editing, session-local
history, and contextual Tab completion. The historical TUI is not required.
Emulator output wraps at 80 columns; guest terminal data is not reformatted.

Readline is active only while editing operator commands. At this prompt,
Control-E has its normal Readline end-of-line meaning (with default Emacs
bindings); during guest execution it remains the configured operator escape.
Device definitions are selected with a configuration file before startup, not
edited through a general runtime configuration UI.

When the machine is stopped, the terminal displays an emulator command prompt.
`run` or `continue` resumes execution and gives the terminal to the selected RC
console. Control-E stops execution and returns to the emulator prompt. This is
different from `attention`, which delivers the RC operator interrupt without
otherwise redefining machine state.

Commands must work with ordinary line input and must not depend on cursor
addressing or a full-screen terminal library. Short aliases may be supplied,
but documentation and command files use the complete command names.

## Required console input

Every character-oriented frontend must support:

- Printable 7-bit ASCII characters from `0x20` through `0x7e`.
- Return from either host CR or LF, normalized to RC Return (`0x0d`).
- Tab (`0x09`).
- Host Backspace (`0x08`) and Delete (`0x7f`), with a documented frontend
  mapping suitable for the selected RC terminal device.
- Arbitrary 7-bit input through an explicit `send` command even when a byte is
  reserved by the local frontend.

UTF-8 text outside 7-bit ASCII has no implicit mapping. A frontend must reject
it visibly or apply an explicitly selected character-set conversion. It must
not silently feed UTF-8 continuation bytes to an RC device.

Pasted input follows the same rules as typed input. A frontend that enables
bracketed paste must consume the host framing sequences rather than forwarding
them to the RC console.

## Portable local control entry

The default local terminal control prefix is Control-E (`0x05`), following the
established SIMH convention. It is configured with
`OPERATOR ESCAPE=CTRL-E` in `RC4000.cfg`. `CTRL-@` through `CTRL-_` are valid
values, and `OPERATOR ESCAPE=NONE` disables in-band control when a separate
control endpoint is available.

Control-E enters a line-oriented operator command prompt. Required command
names use printable ASCII and include no mandatory shortcut keys. The implemented command families are:

- `help`
- `run`, `continue`, `stop`, `step`, and `trace`
- `examine` and `deposit` for registers and core
- `break`, `watch`, and their removal commands
- `reset`, `start`, `autoload`, and `attention`
- `interrupts on` and `interrupts off`
- `console NUMBER`
- `show` and `set` for machine, device, and configuration state
- `attach` and `detach` for media
- `send BYTE...`
- `send-date` for translated host-date insertion
- `quit`

Only commands backed by implemented machine-control operations are exposed.
Implemented commands may provide documented, conflict-free abbreviations.
These are explicit aliases rather than arbitrary prefixes, so scripts do not
change meaning when the command vocabulary grows. The documented aliases are
`h` for `help`, `r` for `run`, `c` for `continue`, `st` for `stop`, `s` for
`step`, `t` for `trace`, `sh` for `show`, `x` for `examine`, `d` for
`deposit`, `b` for
`break`, `nb` for `nobreak`, `res` for `reset`, `sta` for `start`, `att` for
`attention`, `al` for `autoload`, `int` for `interrupts`, `se` for `send`,
`sd` for `send-date`, `w` for `watch`, `nw` for `nowatch`, and `q` for
`quit`.

The control-panel commands follow the hardware state rules. `reset` stores the
interrupted instruction counter in word 10 when it enters reset state.
`start` and `autoload` have no effect unless the machine is in reset state.
`start` loads the instruction counter from word 14; `autoload` reads the first
bootstrap instruction through device 0 and begins at address 0.

Command mode provides Return to execute, Backspace/Delete to edit, and
Control-C or Control-E to cancel. The `send` command is the portable way to
transmit Control-E or another locally reserved byte to the emulated console.

`send-date` queues a translated host date without depending on a keyboard shortcut.
It inserts the six two-digit fields `YY MM DD HH MM SS` followed by RC Return
into the selected console input queue. It obtains the date from the host clock.
Explicit timestamp arguments are not implemented; deterministic recipes can
enter a historical date as ordinary guest input. The supplied monitor accepts years 82 through
99, so a host date
outside that range must produce a clear error rather than silently wrapping or
substituting a different year.

Automatic host-date insertion may apply the configured full-year offset:

```text
emulated year = host year + configured year offset
```

The configuration form is `DATE YEAROFFSET=NUMBER`; for example,
`DATE YEAROFFSET=-40` maps host year 2026 to emulated year 1986. The offset
applies only when `send-date` obtains the date from the host clock. A future
explicit timestamp passed to `send-date` will already be an emulated timestamp
and bypass the offset. A command-line override may be added using
`--date-year-offset NUMBER`.

The translated date must be a real calendar date and its year must be in the
monitor's accepted 1982-1999 range. An out-of-range year or an invalid result
such as February 29 in a non-leap target year produces a diagnostic. It is not
clamped or silently changed. The diagnostic includes the host date and
configured offset. The offset affects date insertion only; it does not alter
device timing or the RC clock after initialization.

Control-C outside command mode requests a graceful emulator stop when the
local terminal uses normal POSIX signal handling. Sending ETX (`0x03`) to the
RC console therefore also uses `send 03` or a data-only frontend.

## Escape and special keys

No essential operation may require:

- Alt or Meta combinations;
- Super, Windows, or Command keys;
- Ctrl-Alt combinations;
- function keys;
- cursor, navigation, or editing keys represented by terminal-specific escape
  sequences;
- shortcuts commonly consumed by a window manager or terminal emulator.

A frontend may offer such keys as optional aliases after identifying them
through its terminal library. The documented command name remains the
authoritative interface.

The local terminal frontend implements the `OPERATOR DATE=ALT-D` setting as an optional alias for
`send-date`; another Alt-letter or `NONE` can be configured. It is not
required because terminal emulators and window managers may consume it.

The historical bare Escape operator key may remain as an optional `attention`
alias. It is not the only way to request attention because Escape is also the
prefix of many terminal sequences. A terminal frontend that supports this
alias must distinguish a bare Escape from recognized sequences and must offer
a way to disable the alias. `attention` in command mode is always
unambiguous.

Unknown escape sequences must not accidentally invoke machine control. They
are rejected or passed according to an explicit frontend mode.

## Telnet input

Telnet is the preferred first interface for additional configured console and
RC851 devices; additional local virtual terminals are not required. The service must not assume the escape key used by a particular
Telnet client. Its default connection is an RC console data path and handles
Telnet negotiation, IAC escaping, and NVT Return normalization at the network
boundary. Emulator control is provided through the local operator interface
or a separately configured control endpoint.

The service is disabled unless configured and should bind to loopback by
default. Connections are data-only and select one configured emulated
console endpoint; exposing emulator control over the network is a separate
feature with separate security requirements.

The client-facing terminal contract is an ANSI/VT100-compatible character
terminal carried over Telnet/NVT. RC device bytes are not exposed as an
undocumented raw terminal protocol. A basic `CONSOLE` needs only the small
printable-character, Return, Line Feed, Backspace, and Bell subset. An
`RC851` endpoint translates its cursor-addressing, clear, erase, tab, home,
and cursor-motion operations into ANSI/VT100 escape sequences while retaining
RC851 state and semantics inside the emulator. Input performs the reverse
boundary translation for supported keys; unknown host escape sequences must
not become RC console data accidentally.

The input implementation accepts printable bytes and ordinary control
characters, normalizes NVT Return, maps ANSI Delete through the configured
DEL policy, and consumes CSI/SS3 cursor and function-key sequences. Cursor and
function keys are not assigned RC byte values until a workload requires and
documents a specific mapping. ANSI OSC and other string controls are consumed
through their terminator rather than forwarded as console text.
Each endpoint has a configurable attention policy. `ATTENTION=CTRL-A` is the
default and intercepts that control byte before console delivery.
`ATTENTION=CONNECT` requests the operator interrupt on every accepted client
connection, while `ATTENTION=NONE` disables the in-band shortcut. Telnet
`BRK` requests attention in every mode and remains a protocol-level fallback.
A raw ANSI Escape prefix is not used for this purpose.

The service should allow one active client per configured console endpoint
and must not impose a smaller terminal-count limit of its own. In the current
machine model, device numbers range from 0 through 63, but interrupt allocation
is the practical limit: levels 3 through 23 are configurable and every
`CONSOLE` or `RC851` consumes two unique levels. At most ten such terminals
can therefore be configured when no other interrupt-driven devices consume
those levels. The shipped configuration currently defines two `CONSOLE`
devices; consoles, clock, reader, punch, and host exchange use eight unique
levels, leaving thirteen. Six additional terminal pairs fit in the parser;
monitor-side device descriptions must also match. The runtime-device checkpoint
verifies that allocation and rejects a seventh pair reusing an occupied level.

Telnet connections remain console-data endpoints: the attention byte requests
the RC operator interrupt but does not expose the emulator command prompt.

## Automation

Piped console input is treated as console data, not as an operator command
stream. Automated machine control uses command-line options, a future control
file descriptor/socket, or a test API. This avoids interpreting an input file
containing the configured escape byte as an emulator command.

All operator commands must eventually be available through the same
UI-independent control API so terminal, Telnet, graphical, and test frontends
do not develop different machine semantics.
