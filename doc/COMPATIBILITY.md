# Compatibility and Limitations

For a concise feature and design summary, read [the overview](OVERVIEW.md).

Sim4000 preserves the operation of the supplied monitor and system disk. It
is not a cycle-exact reconstruction of physical hardware, a general RC8000
emulator, or a source-compatible port of the DOS program. The original emulator
was designed and programmed by Henrik A. Jacobsen; this native implementation
retains his system environment as its principal behavioral reference.

Use the [User Manual](usermanual/README.md) for operating instructions. This report
describes the current supported baseline, intentional simplifications, and
known coverage gaps. A tested workload is evidence for that workload, not a
promise that every historical installation or peripheral will work.

## Supported system and verification

- Development and testing use Linux on x86-64. The runtime uses C17, POSIX
  interfaces and GNU Readline; no DOS, VCPI, Bochs, handwritten assembly or
  JIT is required. Other hosts, including macOS, have not been validated;
  platform-specific setup and testing are left to interested contributors.
  A 32-bit build is not a validated release target.
- `MONITOR` is the supplied emulator-adapted monitor. Included sources in
  `monsrc/` rebuild it byte for byte, but that does not identify it as an
  unmodified original vendor release. Its lineage includes RC4000/RC8000
  software and later emulator-specific changes.
- Native `UNIT0` replaces the historical `fromdos`/`todos` programs with
  `fromhost`/`tohost`. `UNIT0.original` preserves the historical image; select
  it explicitly if those original entries are needed. The guest process name
  `dos` remains inside the monitor for binary compatibility.
- Verified operation includes boot/date initialization, process control,
  independent local and remote sessions, Algol and SLANG compilation, simple
  text editing, catalog listings, host transfers, and retained guest files
  with RC433X persistence. The numerical `regnetest` checkpoint reproduces
  the original emulator's two reported sums and one successful iteration.
- A bounded same-console checkpoint also creates/removes/recreates two idle
  ordinary FPs with MONITOR/RC433X. It uses explicit attention/reselection,
  short replies, output settling and stopped-core descriptor checks. It does
  not certify rapid command pasting, long shared-console montest reports or
  the same workflow on STDMON/DFC403; separate terminals remain preferable
  for independent interactive work.
- RC8000-related program names and monitor source do not establish support
  for RC8000 process relocation, memory management, or arbitrary RC8000 systems.
  The CPU implementation targets the RC4000 instruction model used by these
  fixtures.

See the manual's [software inventory](usermanual/08-software-and-files-in-the-supplied-disk-image.tex)
for the distinction between tested programs and entries that have only been
identified. The default disk has no `soscat` and is not a ready-to-run MIPS/TS
environment. The separate UNIT0.mipsts/UNIT1.mipsts pair and matching
RC4000.mipsts.cfg provide automatic TEM/PRIMO/SOS startup and one demonstration
account. Two boots, TEM-mediated login/FP commands/logout and retained-file
readback pass with the compatibility monitor and protection off. Actual
PRIMO-to-RC610 disk-text transports also pass repeated byte-exact jobs over two
boots, with signup/signoff and continued TEM/SOS login/logout. This uses an
administrative FP, not an unrestricted print facility in the small demo account.
Recovery of a transport submitted with output detached is checked through
PRIMO waiting/start and append reattachment. Partial host-write errors, mid-job
paper loss and arbitrary queue capacity remain unverified. Two independent
outstanding transports pass exact FIFO output and a same-instance followup,
using one blocking waiter and one status-polling client. A second concurrent
blocking wait receives reply 3 (missing resources) in the checked session;
this is not two successful simultaneous waits. Arbitrary
accounts, applications, peripheral workflows and protected MIPS/TS operation
are not certified; see the MIPS/TS chapter of the User Manual.

Separate developer assessments now pass TEM/PRIMO/SOS startup and demo-account
login/FP commands/logout with the maintained protected monitor. They also pass
two local PRIMO printer jobs, a remote-created retained file across two clean
emulator starts, and one forbidden monitor-store fault with child removal and
later subsystem/login recovery. These are private, bounded experiments, not a
shipped protected MIPS/TS profile or a general isolation guarantee. The installed
protected upsoscat still breaks on task-mode pl w3 34 in linked tofrom1
(IRQ 0, saved-next IC, PR bf/Key 1), so the assessment manufactures its
verified catalog in an explicit separate compatibility run. Protection stays
enabled throughout the protected runtime; no subsystem/CPU bypass is used.
An experimental private candidate also passes two automatic boots with terminal/
retained-file checks and four local PRIMO jobs using independent overlays and
unchanged clean bases. It has not been promoted into the ordinary build or
binary distribution, and does not extend the supported profile selection yet.
Continue to use the shipped compatibility MIPS/TS pair for documented operation.

## Differences from physical RC4000 hardware

### CPU, storage, and protection

The interpreter implements the assigned instruction set, register aliases,
24-bit address arithmetic, interrupt entry, integer arithmetic, and
floating-point arithmetic. It is a functional model, not an electrical or
microcycle simulation. In particular:

- The default profile does not enforce storage protection. An experimental
  opt-in (`PROTECTION MODE=RC4000`) now checks instruction fetch, CPU writes and
  privileged operations, with separate storage keys and PR state. The CLI can
  inspect PR and keys. A separately source-built monitor and matching software
  disk are supplied through RC4000.protected.cfg, with bounded process, isolation,
  compiler, host-transfer and Telnet checks. Compatibility remains independently
  selectable and is the default. There is no parity array; this functional
  protection model is not a host-security sandbox or an exhaustive security
  certification of every monitor service or installed program.
  In protected FP sessions use rcin/rcout: the retained original fromhost/tohost
  contain incompatible newer-machine instructions. PROFILES.txt explains the
  separate artifacts, older tofrom binding and remaining software boundary.
  Follow-up protected frontend checks also pass one freshly compiled regnetest
  iteration and command-driven edit replacement/insertion/deletion/truncation/
  reopening with byte-exact exports. This does not establish full-screen editor
  support, every numerical-library path or unlimited repeated execution.
  Both profiles also retain text/binary files and complete user-scope lookup
  metadata across normal FP exit and clean simulator restart using separate
  overlays. Cross-profile overlay reuse is rejected without changing either
  sidecar. This is not crash-durability or concurrent-writer certification.
  Protected catstat/cata agree with a fresh packed catalog export; bare show/
  claim correctly report tested allocation/reclamation deltas. Bootstrap can
  replace temporary catalog records, so a base-image listing is not an exact
  oracle for every active-session temporary entry.
  Both profiles pass catsort/search temporary, user and system scope checks,
  including promotion, exported reports and cleanup. Ordinary system listings
  contain 414 compatibility or 416 protected entries; the special catalog
  descriptor is separate. Temporary search exports can include scratch entries
  that have disappeared by the next command; these are separate snapshots.
  Bounded FP command-file checks also pass in both profiles: nested EOF/output
  return, missing-program continuation, conditional lookup/groups and guarded
  SLANG binary export, with full test-file cleanup. These do not establish
  stop-on-error behavior, automatic rollback or general unattended batch safety.
  The SLANG list.yes command-file example also passes in both profiles, with
  separate ordered text listing and binary-code exports, preserved source,
  restored console input/output and complete temporary-resource cleanup.
  An explicit opt-in run of the valid integer SLANG sample also reaches the
  expected 444 sum and stable self-loop in both profiles. Protected task mode,
  PR bf and code key 1 are observed. Guest proc break/start interrupts that
  loop and reinitializes FP, after which later jobs/cleanup work. This is not
  a general recovery guarantee for arbitrary machine code.
  Text display/copy/rename/concatenation also pass in both profiles. Installed
  `cmp` passes the compatibility identical/replacement examples, but is not
  usable in the protected profile: the tested identical-text call pauses in
  `tofrom1`. An opt-in negative check verifies this specific break and FP
  reinitialization with preserved inputs; it is not successful comparison or
  evidence of general fault recovery. An instruction-level probe localizes the
  fault to task-mode `pl w3 34` (`730022`), IRQ 0 with saved-next PC, PR bf/key 1.
  The binary embeds newer tofrom1/tofrom2 modules; changing a catalog binding
  cannot relink it. A source-based comparator rebuild remains deferred.
  The host fallback is checked with cmp/diff on exported text: equal, different
  and missing-file statuses remain distinct. A colliding export is rejected
  without changing the existing file's bytes or identity/size/mode/mtime, and
  FP remains usable afterward in both profiles.
- With protection disabled, protection instructions retain compatibility behavior:
  valid addresses are checked, `kl` returns zero, `ks`/`pl` have no protection
  effect, `ps` stores `0x080`, and `sp` skips. These are compatibility stubs,
  not hardware protection implementations.
- The architectural address space is 16,777,216 RC bytes, where each RC byte
  is a 12-bit halfword. Full-space CPU boundary tests pass, but configuration
  still caps memory at 8,388,607 RC bytes. Normal operation uses 1,048,576;
  booting the supplied monitor at full architectural capacity is not verified.
- Missing storage produces the instruction-exception path; it is not silently
  wrapped into a smaller installed store. Word accesses ignore the low address
  bit, and an unpaired final byte cannot be used as a complete word.
- `IX` (opcode 61) is a switchable extension enabled by default for the supplied
  software. `CPU IX=NO` or `--no-ix` disable it; `CPU IX=YES` or `--ix` enable
  it. Its semantics follow Henrik Jacobsen's upstream
  `UCODE/IX.ASM`, with independent vectors for scaling, bounds, addressing and
  floating-exception handling. This does not claim complete RC8000 or hardware
  conformance. Bare `catstat` completes with both monitor/controller variants
  and matches the stored catalog view when enabled. With IX disabled, the same
  binary still takes interrupt 0 and `break 0 line 18-19`; that negative
  diagnostic remains tested. Existing FP/Ex21 behavior is unchanged.
  Its assignment to the historical RC8000 hardware instruction set has not
  yet been verified against an original hardware manual; the newer emulator
  source and supplied guest binary establish the implemented compatibility
  behavior, not its first hardware model or introduction date.
- A single interpreter call can execute several `am` prefixes and the following
  instruction. A chain reaching 1,024 prefixes is stopped as unimplemented,
  a host-safety limit rather than an architectural AM-chain limit.
- Control-panel `start` currently clears both the sign and alignment bits of
  the address loaded from word 14. This retained behavior does not demonstrate
  high-half-address startup compatibility with the physical control panel.
- Floating arithmetic uses explicit integer fractions, exponents, guard bits,
  normalization, and rounding rather than the host FPU. Low-precision packing
  is implemented and has focused tests, unlike the historical DOS release's
  documented omission. `fa`/`fs` retain the historical alignment residue to
  reproduce the supplied workload. An independent rational/signed-integer oracle
  checks all six FP instructions, including rounding, alignment, exponent/integer
  limits, signed scaling, both precision modes and all register pairs. A separate
  literal 38-bit FA/FS model identifies intentional compatibility differences.
  Non-normalized arithmetic checks include truncation before normalization and
  dominance shortcuts, rather than assuming ideal real arithmetic. Division
  rescales large fraction quotients before guard-bit generation to avoid host
  overflow, including normalized numerators divided by tiny non-normalized
  denominators; small-quotient truncation is retained. These checks validate the
  native arithmetic contract, not arbitrary non-normalized hardware results.
  Broader workload coverage remains incomplete. No claim of exhaustive FP
  conformance is made. See the [FP test scope](impl/TEST_FIXTURES.md#floating-point-oracle).

### Timing and I/O

There are two different time policies:

- Interactive execution advances the emulated devices from monotonic host
  time at 39 coarse ticks per second, including pacing waits. Operator prompt
  time is not continuous guest execution. Debug steps and traces are unpaced.
- Bounded `--run` advances one device tick per 10,000 interpreter calls for
  repeatable traces. Calls are not necessarily individual instructions because
  of AM chains. Do not compare its guest clock directly with interactive time.

The coarse clock approximates the historical emulator's 25.6 ms tick; it does
not model the physical interval timer's 100 microsecond resolution. The
`original` throttle preset is approximately 250,000 instructions per second,
not calibrated hardware speed. Instruction-dependent latency, channel timing,
physical peripheral delays, and disk/CPU overlap are not modeled. A monitor
CPU-time report is therefore not a physical RC4000 performance benchmark.

The I/O dispatcher selects device addresses modulo 64: it models a 64-number
installation rather than the full 18-bit device address field described by
the hardware manual. Configurable interrupt levels
are 3 through 23; console devices each require two unique levels. Theoretical
terminal capacity also depends on other devices and the monitor's device
descriptions, not merely the number of available TCP ports.

### Devices and media

| Device | Implemented behavior | Important boundary |
| --- | --- | --- |
| `CONSOLE` | Seven-bit character I/O, echo, timeout, I/O and operator interrupts | Small RC FIFO; local host buffering is separate. Not a physical teletype timing model. |
| `RC851` | Console semantics and translated screen controls through Telnet | ANSI/VT100 subset, not a complete terminal model or verified full-screen SOS editor. |
| `CLOCK` | Coarse counter and configured interrupts | Deterministic or interactive time policy, not a physical interval timer. |
| `READER` / `PUNCH` | RC2000/RC150 raw tape frames, parity, status, and autoload | File-backed synchronous completion; no paper transport timing. No historical boot tape is supplied. |
| `RC433X` | Adapted monitor messages and packed disk sectors | Emulator-specific interface, not a physical RC433 controller. Volatile/persistent overlays and explicit write-through are available. |
| `DFC403` / `RC4819` | Historical controller commands, status, data/head interrupts, geometry, and core transfers | Synchronous; no seek/rotation delay or concurrent transfer. File-backed disks use volatile COW only. |
| `RC4819RAM` | Fixed-capacity, zero-initialized drive on DFC403 | Volatile RAM medium; same timing/controller limitations. |
| `RC749` | Read-only tagged TAP records, marks, status, spacing, rewind/unload and confined optional guest mounting | Synchronous, maximum 7,680-byte transfer. Write/erase rejected. Synthetic tests only; no archived software tape or DOS binary comparison yet. |
| `HOSTFILES` | Native host exchange beneath a configured directory | Virtual emulator service, not RC hardware. |

`RC433XRAM` is recognized but rejected at runtime; volatile RC433X COW replaces
that nonpersistent workflow. Physical `RC433` is not a configuration type.
Other historical peripherals, including serial `COMTERM`, are not supplied
by a generic hardware emulation layer. `--check-config` validates syntax, not
successful media opening or complete runtime device support.

`CONSOLE`/`RC851` may opt into `PROTOCOL=V2` for guest type/prompt attributes,
three input tables and blind/backspace-echo modes. Classic remains the default
and the supplied monitor's supported protocol. The V2 option has synthetic
I/O/CLI coverage only, not a verified V2 monitor or editor workflow. It does
not enable expanders, connection controls or RC8000 CPU features. Host input
mapping and ANSI filtering are unchanged.

The optional DFC403 configuration requires generated `STDMON`, not the default
RC433X monitor. It has verified boot, basic user operation, and host transfers.
Its geometry/status tests do not establish every real controller mode.
Output-address and set-mode remain historical no-ops; asynchronous busy/head
overlap is not implemented. Persistence and runtime disk replacement remain
RC433X-only; runtime geometry editing is not offered.

## Differences from the DOS operator environment

- The full-screen TUI, local virtual-terminal windows, mouse interface,
  SysRq/Alt-X controls, and PC/BIOS integration are replaced by a Readline CLI.
  It is SIMH-like, not a SIMH command-language implementation. Emulator output
  wraps at 80 columns; guest output is not reformatted. History is session-local.
- Control-E normally suspends execution for emulator commands; `attention`
  requests the guest RC operator interrupt. These are different operations.
  Alt-D is optional and configurable; `send-date` avoids dependence on an
  Alt shortcut being passed by the desktop. The supplied date prompt accepts
  1982-1999; a configured year offset affects host-date insertion only.
- Local guest input supports ASCII, Return, Backspace/DEL mapping, pasted
  input, and explicit seven-bit `send` bytes. It is not the Readline prompt
  and does not provide guest line editing. The interactive frontend consumes
  common ANSI cursor/function/string controls and bracketed-paste markers;
  ANSI Delete follows the configured DEL mapping. Bare Escape requests guest
  attention after a 30 ms lookahead. Non-ASCII bytes are rejected, counted by
  `show input`, and reported on suspension, not converted from UTF-8.
  This filtering does not apply to data-only bounded `--run` input.
- Additional terminals use separately configured Telnet listeners instead of
  COM ports. Each listener has one active client and its own port. The primary
  local console cannot also be exposed through Telnet. Remote endpoints do not
  expose emulator commands or the local Control-E prompt.
- Telnet consumes common ANSI cursor/function/string controls rather than
  assigning RC key codes to them. Non-ASCII input bytes are discarded, not
  decoded as UTF-8. Input/output buffers are bounded and can drop bytes under
  overload; use `show telnet` to inspect counters. Telnet has no encryption
  or authentication and should remain on loopback or behind a trusted boundary.
- The shipped configuration defines two consoles and consumes eight interrupt
  levels across consoles, clock, reader, punch, and host exchange. Thirteen
  levels remain: six additional terminal pairs fit in the parser, not eight.
  Monitor-side definitions still need to match any added terminals.
- GNU-style options replace DOS switches. Default configuration is `RC4000.cfg`
  in the current directory, not `RC4000CFG` environment lookup or fallback to
  the executable directory. Config paths are relative to the config file;
  command-line overrides and runtime media paths are relative to the CWD.
- Native configuration files use LF and Unix paths. Core allocation requests
  the configured maximum directly; it does not reproduce DOS best-fit memory
  allocation or rounding to 2,048-halfword boundaries.
- Device definitions are selected at startup rather than edited in a runtime
  configuration window. Supported media attach/detach/rewind operations remain
  available. Core files hold words, not a complete machine/disk/device snapshot.

## Host files and persistence

Normal disk writes are volatile COW. Guest `scope user` does not persist host
disk changes by itself; explicitly select an RC433X persistent overlay or a
verified working copy with write-through to retain work across emulator exits.
There is no automatic whole-machine save/restore or guarantee of recovery from
uncatchable termination, host crashes, or storage failure. Follow the manual's
[disk persistence instructions](usermanual/18-configuration-and-disk-persistence.tex).

Host filenames are case-sensitive Unix-relative paths beneath `HOSTFILES ROOT`.
Traversal, symlinks, DOS drive paths, and nonregular files are rejected. Guest
exports create new mode-0600 files rather than truncating existing files;
punch replacement likewise requires explicit `replace`. Whitespace in operator
path arguments is not supported by the command parser.

Text transfer uses LF, removes NUL/CR, and treats byte 25 as the RC end marker.
There is no DOS code-page, Danish-character, or UTF-8 text conversion. Binary
mode preserves all transferred byte values but includes guest allocation
padding; it is not automatically trimmed to an original host file's length.
Use the manual's [binary transfer guidance](usermanual/09-host-file-exchange.tex).

## Deferred work and release status

Broader protected-software coverage, further archive software installation,
broader optional-subsystem examples, and complete
independent architectural vectors remain future work. Installation packages
are deferred; copying the documented runtime files is sufficient.

The source includes a Linux CI matrix, locally verified with GCC Debug,
GCC Release, and Clang sanitizers. A hosted GitHub run is still pending.
Technical compatibility and successful tests do not resolve historical media
redistribution rights or the Readline license review. This report is not a
redistribution license or a declaration of a completed public release.
