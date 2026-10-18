# Sim4000: Features and Implementation

Sim4000 runs a preserved RC4000 monitor and software environment on a modern
POSIX host. It is a functional reimplementation in C17, not a cycle-exact
hardware simulation or a general RC8000 emulator. **Henrik A. Jacobsen** wrote
the original emulator and adapted the monitor for it. His emulator and the
environment he preserved are the foundation and behavioral reference for this
project. The historical monitor and guest software include work originating
with Regnecentralen, RC/ICL and other authors; they are not solely Jacobsen's work.

For commands and complete operating procedures, read the ready-built
[User Manual](usermanual.pdf). This overview requires no knowledge of the
source tree or internal development documentation.

## Main features

- **Historical software:** monitor process control, FP commands, catalogs,
  retained files, text editing, Algol and SLANG. Later RC8000/RC9000-era
  components are installed, but not every program or option is verified.
- **Operator interface:** SIMH-like CLI with GNU Readline editing, session-local
  history, Tab completion and command abbreviations. Execution controls,
  registers, memory, breakpoints, watchpoints, core files and traces support
  exploration. CLI output wraps at 80 columns; guest output is unchanged.
- **Terminals:** local console and independently configured Telnet endpoints;
  RC851 screen controls are translated to ANSI/VT100. Attention, date insertion
  and DEL-to-Backspace mapping are configurable. A listener accepts one client
  at a time; separate guest terminals use separate ports.
- **Disk handling:** offline catalog listing, integrated `--mkdisc`, disposable
  writes, persistent overlays or explicit write-through to a working copy.
  The normal RC433X disk interface and optional RC433 and DFC403/RC4819 models
  represent device commands and interrupts, not mechanical timing.
- **Other devices:** interval clock, paper-tape reader and punch, read-only
  RC749 tagged-TAP magnetic tape, host-file exchange and an RC610 line printer.
- **Host exchange and printing:** text/binary exchange with non-overwriting
  exports. Printer output appends by default; each completed print is flushed
  immediately, without waiting for a full buffer or emulator shutdown.
- **Pacing:** configurable host-CPU budget or instruction rate. The default
  interactive budget is 50% of one host CPU; `original` approximates 250,000
  instructions per second, not hardware-calibrated performance.

## Supplied operating profiles

| Profile | Configuration | Purpose and persistence defaults |
| --- | --- | --- |
| Compatibility | `RC4000.cfg` or `RC4000.compat.cfg` | `MONITOR` and `UNIT0`; broad compatibility with the preserved software. Disk writes are discarded at exit. |
| Protected | `RC4000.protected.cfg` | `MONITOR.protected` and `UNIT0.protected`; storage keys and privileged-operation checks enabled. Disk writes are discarded at exit. |
| MIPS/TS | `RC4000.mipsts.cfg` | `MONITOR`, `UNIT0.mipsts` and `UNIT1.mipsts`; automatic TEM/PRIMO/SOS startup, fixed historical date, one `test/test` account and two separate persistent overlays. Protection is off. |

All three profiles include the printer. Never mix their matching media or
overlays. The optional DFC403 profile and unchanged
`UNIT0.original` are source-distribution extras, not binary-archive defaults.

MIPS/TS supports checked file, batch and administrative PRIMO-printing recipes.
Idle TCP reconnection and known echoed partial-line correction preserve the
existing FP job. Larger accounts and two simultaneous sessions have separate
private verification, but are not additional ready-made distribution profiles.
Protected MIPS/TS is not a supplied operating profile.

## Important implementation choices

The CPU is a portable interpreter with explicit 24-bit arithmetic and address
handling. An RC word is 24 bits; an RC byte is a 12-bit halfword, not a host
eight-bit byte. Guest floating-point arithmetic uses integer-based fraction,
exponent and rounding logic rather than the host floating-point format.
The optional IX instruction is enabled for software compatibility; it does
not select a complete RC8000 architecture.

CPU, storage, device and host-I/O components are separate. The protected monitor
is rebuilt from maintained guest sources with its key-setting code restored.
The historical guest software, not a replacement host-side operating system,
still implements catalogs and process behavior.

RC433X persistence records modified sectors in a separate overlay rather than
rewriting the base image. Explicit write-through instead modifies its selected
working copy immediately. Neither mode saves CPU state. Host exports and
printer files persist independently.

Interactive devices follow monotonic host time with a coarse 39 Hz clock.
The bounded runner uses deterministic instruction-count-based device timing
for repeatable execution. Throttling controls host scheduling; it does not
model individual instruction latency, disk seeks or overlapped physical I/O.

## Boundaries and practical requirements

Development and testing use **Linux on x86-64**; other hosts and 32-bit builds
are not validated release targets. Building requires CMake, C17 and GNU Readline
development files; a copied binary needs its linked Readline runtime library.
No installation, DOS, handwritten assembly, JIT, Python or TeX is required
to run the distribution.

Storage protection is a guest compatibility feature, **not a host-security
sandbox**. In protected FP sessions use `rcin`/`rcout`; the original
`fromhost`/`tohost` and installed guest `cmp` are not protection-compatible.
Telnet is unencrypted and has no host authentication. Bind to loopback:
reconnecting can resume an existing job without a new guest password request.

Coverage is bounded to documented workflows, not arbitrary terminal errors,
interrupted output, crash durability or all protected guest software. Stop the
emulator before copying a profile; preserve both MIPS/TS overlays with their
matching bases. Devices require suitable guest configuration and software.

The new native contributions use MIT terms. Historical software/media and
linked dependencies have separate conditions; see [LICENSE](../LICENSE).
Availability in the emulator does not establish unrestricted redistribution
rights to all included historical material.
