# Sim4000

Sim4000 is a native RC4000 computer emulator.
It preserves the operation of the historical monitor and system disk while
providing a terminal-based, SIMH-like operator interface.

All supplied monitor profiles include a file-backed RC610 line printer.
Output starts automatically and appends to a profile-specific text file,
with completed prints made visible immediately.

Development and testing use Linux on x86-64. The runtime uses C17, POSIX
interfaces and GNU Readline. Other hosts, including macOS, have not been
validated; platform-specific setup and testing are left to interested contributors.

## Historical background

The RC4000 was a 24-bit computer developed by Denmark's Regnecentralen in
the 1960s. Its origins were in industrial process control, but it became a
general-purpose system designed to run several programs concurrently.
This placed it at the intersection of real-time computing and the emerging
ideas of multiprogramming.
See the [history of Regnecentralen](https://lex.dk/Regnecentralen) and
Brinch Hansen's [1967 hardware paper](https://link.springer.com/article/10.1007/BF01939260).

[Per Brinch Hansen](https://lex.dk/Per_Brinch_Hansen), a Danish pioneer of
operating systems and concurrent programming, was a principal architect of
the RC4000 and its system software. The multiprogramming system was running
by spring 1969. Its central idea was a small **nucleus**: a kernel providing
process creation and control, a hierarchy of parent and child processes, and
message-based communication, including communication with I/O devices.
Different operating systems could be built above this common foundation,
rather than fixing one mode of operation into the kernel.
His [1970 nucleus paper](https://homepage.divms.uiowa.edu/~ochipara/classes/advancedos/papers/historical/p238-hansen.pdf)
explains the design; his later
[historical account](https://cs.ucf.edu/courses/cop4600/sum2007/files/2001b.pdf)
identifies its separation of mechanism from policy as a lasting contribution
to operating-system design.

Sim4000 makes this history executable, not just readable: you can operate the
monitor, run preserved programs, compile Algol, and inspect instructions and
memory on a modern host computer. Its purpose is to preserve access to a
working software environment and make experimentation possible without the
original hardware. The supplied monitor includes later adaptations; this is
not a pristine reconstruction of a 1969 installation. Storage protection is
available through a separate source-built monitor/software profile; the
default retains compatibility behavior.
For more historical material, see the
[RC4000 page at Datamuseum](https://datamuseum.dk/wiki/RC4000) and the
Bitstore keyword indexes for
[RC4000](https://datamuseum.dk/wiki/Bits:Keyword/RC/RC4000) and
[RC8000](https://datamuseum.dk/wiki/Bits:Keyword/RC/RC8000).

## About this emulator

The original RC4000 emulator for MS-DOS was designed and programmed by
**Henrik A. Jacobsen**. His emulator, monitor adaptations, and preserved system
environment are the foundation and behavioral reference for this project.
Sim4000 is a new implementation, not his original program; this credit does
not imply his authorship or endorsement of the new implementation.

Jacobsen reports that his emulator was also sold commercially: one of its
two licensed installations replaced an RC8000 at the Technical University
of Denmark and ran absence administration for several years. His account
illustrates that this preserved environment was used for real workloads,
not only demonstrations.

This is a reimplementation in C17, not a strict port of the original
Pascal program. It needs no DOS environment, Bochs, handwritten assembly, or
JIT compiler. This directory is self-contained: the historical source tree
outside it is not required to build or run Sim4000.

The bundled historical sources/media and adapted Algol utilities are not
wholly new project code. Jacobsen has permitted use of his own work with
appropriate credit. New native-project contributions are offered under the
MIT terms in LICENSE, with explicit scope exceptions. This does not grant
redistribution rights to historical media or override third-party terms,
including those of GNU Readline. Those publication questions remain separate.
Contributor documentation starts at `doc/impl/README.md` in a source checkout.

## Features

- Run the supplied monitor and disk-resident programs, including Algol and SLANG.
- Control execution, inspect registers and memory, and set breakpoints.
- Connect additional ANSI/VT100 terminals through Telnet.
- Exchange text and binary files with the host through `fromhost` and `tohost`.
- Select compatibility or protected maintained-monitor operation independently.
- Keep disk changes disposable by default, or explicitly retain them.
- Read optional RC749 magnetic-tape images in tagged TAP format.
- Use either the default RC433X interface or the optional historical
  DFC403/RC4819 controller interface.

The native `UNIT0` image contains the host-file utilities. The unchanged
historical disk remains available as `UNIT0.original` in the source distribution.

## Quick start

Requirements: a C17 compiler, CMake 3.20 or newer, GNU Readline development
headers/library, and a POSIX host with a terminal. The executable needs the
Readline runtime library.
Neither Python nor TeX is required for the ordinary build or for running a
binary distribution.
Run these commands from this repository's root directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build
./sim4000 -a
```

At `initialize date:`, type a historical date such as
`86 09 30 08 00 00` and press Return, or use the optional Alt-D shortcut.
Once `pause startproc finis fp` appears, press Escape, then type `s` and Return
to reach the monitor command interpreter.

Control-E (the configurable default escape key) suspends the emulator and opens the `sim4000>` prompt;
`continue` resumes execution and `quit` exits. Disk changes are discarded
on exit unless you explicitly select a persistence mode.
The operator prompt supports Readline editing, session-local history, and Tab
completion. Emulator output wraps at 80 columns; guest output is unchanged.
The default interactive CPU budget is 50% of one host CPU. Use `set throttle`
at the operator prompt to choose a CPU budget, instruction rate, rough
original-speed preset, or unthrottled execution.

No installation is required: copy `sim4000`, `RC4000.cfg`, `MONITOR`, and `UNIT0`
to your own working directory and create `exchange/` for host-file transfers.
Preserve the executable permission on `sim4000`. The binary archive supplies
all three profiles, their configurations and disks, plus the
tutorial's `examples/` directory. Compatibility remains the default; explicitly
select it with `./sim4000 -a -c RC4000.compat.cfg`. For storage protection use
`./sim4000 -a -c RC4000.protected.cfg`, keeping `MONITOR.protected` and
`UNIT0.protected` alongside that configuration. In protected FP sessions use `rcin`/`rcout` instead of
`fromhost`/`tohost`; not every installed program is compatible with protection.
`PROFILES.txt` explains the software boundary, file layout and separate overlay
choices. For MIPS/TS use `./sim4000 -a -c RC4000.mipsts.cfg`; it starts
TEM, PRIMO and SOS with `UNIT0.mipsts` and `UNIT1.mipsts`, using the
compatibility monitor. Connect with `telnet 127.0.0.1 4004`, press Ctrl-A,
select `sos`, then enter `run test pass test`. This is a demonstration
account, not a secure multiuser service; protection is off and the boot date
is fixed. MIPS/TS disk changes persist in its own two overlay files.
None of these profiles replaces another. The alternative `STDMON`/`RC4000.dfc403.cfg`
profile and `UNIT0.original` are available only in the source distribution.
Keep the manual with your copied distribution.

The emulator is under active development. The optional hardware-interface
controller models commands and interrupts, not physical seek/rotation timing.
Telnet has no encryption or authentication; bind it to loopback unless you
provide a trusted external access boundary.

## Documentation

The ready-to-read User Manual is supplied as `doc/usermanual.pdf`.
It covers startup, configuration, keyboards, simulator commands, disk handling,
Telnet, host-file exchange and operation of the supplied guest software.
No documentation build is needed.
