# RC4000 Architecture Notes

This document records architectural facts used by the native interpreter. Its
primary source is the historical collection's
`doc/RC_4000_Reference_Manual_Jun69-ocr-1up.txt` (outside this self-contained
native repository), checked
against the native decoder, guest software and focused tests where OCR makes
a table ambiguous.

## Interactive execution pacing

Execution throttle is a host scheduling facility, not an architectural CPU
change. `src/throttle.c` provides a setting parser and a deterministic governor
driven by monotonic wall time, process CPU time and actual instruction counts.
The CLI services real-time devices and network/input while waiting in intervals
of at most 5 ms. Every new run starts a fresh budget; windows reaching 100 ms
without pacing debt are rebased to bound catch-up credit after host stalls.
Single steps, traces and bounded deterministic runners are not paced.

The `original` preset uses a fixed nominal 4 microseconds per instruction
(250000 IPS), including AM prefixes. This is based on the typical instruction
time in the historical *RC 4000 Software: Multiprogramming System*, general
description and section 9.4. It is only an average-speed approximation: the
[June 1969 reference manual](https://www.math.utah.edu/~beebe/RC-4000/RC_4000_Reference_Manual_Jun69-ocr-1up.pdf),
appendix A.3, gives per-opcode times and addressing overheads, with notably
longer multiply/divide and floating-point operations. Neither those variable
costs nor physical peripheral latency are modeled by the preset. Workload
throughput must not be presented as hardware-calibrated RC4000 performance.

## Storage and registers

- A byte is 12 bits and a word is 24 bits.
- All architectural addresses are byte addresses.
- A word access ignores address bit 23, the least significant address bit.
- W0, W1, W2, and W3 are aliases for words at byte addresses 0, 2, 4, and 6.
- The 24-bit instruction counter advances by two byte addresses per ordinary
  instruction.

The June 1969 reference manual, sections 3.2-3.5, defines unsigned storage
addresses and a maximum of 16,777,216 RC bytes. The core/CPU boundary tests
allocate that complete address space (32 MiB of host halfword storage) and
verify the final byte and aligned word, indirect pointers above the signed
boundary, final-word instruction fetch, skips, and AM chains through zero.
They also check word/double-word transfers at the upper boundary and rejection
of missing storage without accidental access to W0. Host accessors reject
out-of-range addresses rather than truncating them; guest address arithmetic
wraps explicitly to 24 bits.

This architectural test capacity is not the configured runtime capacity.
The native `CORESIZE` policy raises its minimum to 16,384 RC bytes and caps
its maximum at 8,388,607.
An odd-sized store may end with an unpaired byte; byte access succeeds there,
but word access fails instead of wrapping to the start of the store. The
boundary checkpoint verifies missing operand and instruction storage,
interrupt-zero entry, and the saved IC, including a missing second instruction
byte. It does not implement storage protection or validate booting the supplied
monitor with the full architectural address space.

## Instruction word

The 24-bit instruction consists of the following fields, from most to least
significant bit:

```text
23            18 17  16 15 14 13  12 11                    0
+---------------+------+--+--+------+------------------------+
| opcode (F)    | W    | R| I| X    | displacement (D)       |
+---------------+------+--+--+------+------------------------+
       6 bits     2 bits 1  1  2 bits        12 bits
```

`D` is sign-extended from 12 to 24 bits. `X = 0` means no indexing; the other
values select W1 through W3. `R` selects relative addressing and `I` selects
one level of indirection. Therefore:

```text
R I  Effective address
0 0  X + D
0 1  word(X + D)
1 0  instruction address + X + D
1 1  word(instruction address + X + D)
```

All additions wrap to 24 bits. Relative addressing uses the address from which
the current instruction was fetched, not the already incremented instruction
counter. Indirect word reads ignore the low address bit.

The native decoder defines the architectural opcode table. Opcodes 0 through
57 are assigned;
58 through 63 are unassigned in the baseline RC4000 opcode table. The normal
runtime enables the compatibility instruction IX at opcode 61 by default;
`CPU IX=NO` or `--no-ix` restore the baseline treatment of that slot. The other
slots remain unassigned. Baseline decode APIs retain the architectural table;
capability-aware decoding is used for runtime inspection and tracing.

## Optional index instruction

IX consumes an inline word after the instruction. Its signed high 12 bits
give a displacement from the effective address to the lower bound; the upper
bound is the preceding word, and the effective-address word supplies the
origin. Addition wraps at 24 bits before word alignment. The signed low
12 bits give the type: positive values shift the selected register left by
the count modulo 32, truncating to 24 bits; nonpositive values do not shift.
Strict signed bounds require lower < scaled index < upper.

On success, the register becomes origin plus scaled index modulo 24 bits,
and Ex22/Ex23 clear. On failure, Ex22/Ex23 set. If the floating mask is set,
the original register is preserved and a floating interrupt request is raised;
otherwise it becomes origin plus upper bound without a new interrupt request.
Existing pending requests remain intact. Ex21 is unchanged. The next IC
skips the inline word in either case. Interrupt delivery uses the normal next
instruction boundary and global enable; the mask selects request versus fallback.
Operand reads occur before register/flag changes, including register aliases.
Missing installed storage takes the normal instruction exception instead.
This is a narrowly defined compatibility instruction, not a full RC8000
profile. Its precise historical hardware-model assignment remains unverified.

## Modify-next-address

Opcode 9 (`am`) computes its effective address normally, fetches the next
instruction, and adds that 24-bit effective address to the sign-extended `D`
value before the next instruction's relative, index, and indirect stages. An
`am` chain repeats this process. Interrupt recognition is deferred until the
modified instruction has completed.

## Interrupt numbering

The interrupt and mask registers each contain 24 bits, with bit 0 at the left
and highest priority. Historical configuration `INTNO` values are interrupt
bit positions from 3 through 23. On interrupt entry the CPU stores twice the
selected bit position at byte address 8, so the architecturally visible
interrupt numbers are 0, 2, 4, ... 46. Bit 0 is the non-disableable instruction
exception, bit 1 is integer overflow, and bit 2 is floating-point overflow.

## Floating-point representation

An RC4000 floating-point value occupies two 24-bit words. The first word and
the high halfword of the second form a signed 36-bit two's-complement
fraction; the low halfword of the second word is a signed 12-bit
two's-complement exponent. Its value is `fraction * 2^(exponent - 35)`. The
operand address and instruction W field identify the second word; the first
word is the preceding memory word or working register, wrapping W0 to W3.

A normalized positive fraction begins with `01`, while a normalized negative
fraction begins with `10`. Zero has fraction zero and exponent -2048. Floating
add, subtract, multiply, and divide retain two integer guard bits, normalize,
round by the architectural bit-36 rule, and optionally copy fraction bit 33
into bits 34 and 35 in low-precision mode. Integer conversion uses the
effective address as a signed binary scaling exponent; conversion to integer
rounds by adding the first discarded bit, as specified by the manual. None of
these operations use host floating-point arithmetic.

The native compatibility contract for `fa` and `fs` retains
the left-adjusted 64-bit alignment residue through the add or subtract and
reducing it to the architectural guard bits afterwards. This differs from
discarding every bit outside the formal 38-bit temporary register before the
operation. The distinction affects `fs` when a discarded positive residue
must borrow from the retained result. It is implemented with portable unsigned
integer operations and does not depend on host floating-point behavior.

Floating exponents outside -2048 through 2047 are stored modulo 4096 after
setting exception bit 22 and requesting interrupt 4. Division by zero leaves
the register operand unchanged and raises the same exception and interrupt.
An overflowing `cf` instead leaves its W register unchanged and requests the
integer-overflow interrupt 2.

### Floating-point workload checkpoint

The opt-in `regnetest` fixture performs one deterministic iteration after a
normal monitor boot and user-process creation. It reaches the result after
151,147,050 interpreter calls and executes more than nine million `fa`, `fs`,
`fm`, and `fd` instructions without selecting low-precision mode. Its
CPU/register section and both numerical self-checks pass. It produces
`360.700643456` and `1855.98980864`, then reports one successful run and
zero failed runs.

An independent signed-integer lattice model now checks FA/FS helpers and CPU
instructions without duplicating the native wrapped-register/sign-bit logic.
It retains 28 trailing alignment bits, uses mathematical floor division and
range-based overflow detection, then reduces to guard bits before rational
normalization/rounding. A separate literal 38-bit model records differences;
the workload borrow anchor produces `77dec9 9c6001` under compatibility and
`77dec9 9c7001` under the literal model. This validates the chosen compatibility
contract, not full physical-hardware conformance. See
[the FP oracle](TEST_FIXTURES.md#floating-point-oracle) for coverage and limits.

Non-normalized operand checks additionally quantize exact FM/FD fractions onto
the 38-bit guard lattice before normalization. Discarded bits cannot be recovered
by later normalization: a unit-field positive product vanishes, while a negative
one floors to a negative guard unit. FA/FS dominance shortcuts can return a
non-normalized operand unchanged. These are explicit compatibility expectations,
not an assertion that arbitrary non-normalized arithmetic follows ideal real
values or physical hardware. For FD fraction quotients of magnitude at least
two, the denominator is scaled by powers of two, with an exponent correction,
before guard generation. With 36-bit architectural fractions this bounds the
denominator by `2^35`, the guard quotient below `2^38` and the doubled remainder
below `2^36`. This fixes host-width loss and results outside the normalizer's
single-right-shift range, without changing normalized-operand results or
small-quotient truncation. The independent rational model rescales the exact
quotient instead, and tests large ratios around the former sign/wrap boundaries.
This is a native compatibility rule, not a reconstruction of the manual's
non-restoring algorithm for arbitrary non-normalized operands.

The multiply and divide implementations have been compared directly with the
formal 38-bit algorithms in the June 1969 manual, including signed products,
remainders, guard bits, and rounding.

## Current interpreter coverage

The current execution slice implements `aw`, `io`, `am`, `al`, `ac`, `bl`, `ba`,
`bs`, `bz`,
`hl`, `hs`, `rl`, `rs`, `rx`, `dl`, `ds`, `la`, `lo`, `lx`, `wa`, `ws`,
`wm`, `wd`, `ci`, `cf`, `fa`, `fs`, `fm`, `fd`, `aa`, `ss`, `jl`, `jd`, `je`,
`as`, `ad`, `ls`, `ld`, `ns`,
`nd`, the six basic comparison/bit skips, `sx`, `ic`, `ml`, `ms`, `is`, `xl`,
and `xs`. It includes
register/core aliasing and all address modes. The six architecturally
unassigned opcodes take the illegal-instruction path.

`aw` synchronously reads four six-bit characters from device 0. A busy result
is retried, end-of-buffer status restarts the current character read, and a
disconnect or any other input status error places the CPU in reset state. The
four characters are assembled from left to right into the addressed 24-bit
word. With the experimental protection API opt-in, successful autoload also
sets the loaded word's key to zero; compatibility behavior remains unchanged.

With the current instruction and device set, the supplied monitor mounts the
copy-on-write disk, accepts an operator login, displays its `ready` prompt, and
settles in the dummy internal process's enabled self-loop at byte address
`0x001b34` after the startup floating-point multiply and divide operations.

## Device dispatch and deterministic clock

The six low effective-address bits of `io` select the four basic commands and
their modifier. Bits above them select one of 64 device numbers. A device
callback returns accepted, busy, or disconnected; the interpreter exposes the
latter two results in exception bits 23 and 22 as specified by the manual.

The configured `CLOCK` device uses an explicitly advanced deterministic time
source. One device tick represents a 25.6 ms coarse timer interval. Advancing time increments the clock counter and latches the configured
interrupt request. Sense returns the low six counter bits shifted left by
eight, so each coarse tick represents 256 increments of the physical 14-bit,
100-microsecond interval timer, encoded in the native right-aligned word.

Devices expose optional `advance` and `poll` operations. Advancing time never
depends on host wall-clock time, and polling transfers latched device requests
to the CPU interrupt register at an instruction boundary. The bounded runner
advances one device tick per 10,000 interpreter calls. This
ratio is a deterministic bootstrap policy, not yet a cycle-accurate CPU timing
model.

Interactive execution instead advances devices from monotonic host time at
39 ticks per second, including throttle waits. Neither policy models the
physical timer's full resolution or cycle-dependent instruction timing; see
[Compatibility and Limitations](../COMPATIBILITY.md).

The basic `CONSOLE` device writes the low seven bits
and requests the I/O interrupt. Reads consume a 31-character FIFO or wait for
input, sense returns the last character or status, and sensed input is echoed.
An unanswered read times out after the historical one-second timer interval
and returns status bit 2. Escape requests the separate operator interrupt when
no read is pending. Console output is supplied through a callback; input is
submitted through a host-facing function, keeping terminal handling outside
the emulated device.

The bounded runner connects standard input to the first configured
basic console. It polls without blocking at deterministic device-time
boundaries. A terminal is placed in character-at-a-time, no-echo mode so that
Escape can generate the operator interrupt immediately; piped LF is translated
to RC Return. Host terminal mode remains a front-end concern and does not
change emulated console semantics.

Host date insertion queues six two-digit fields in year, month, day, hour,
minute, second order, followed by Return. The supplied monitor accepts years
82 through 99. The frontend exposes `send-date` and a configurable shortcut.

Remote `CONSOLE` and `RC851` endpoints keep Telnet framing and ANSI terminal
translation outside the device model. Their attention policy is likewise a
host-boundary function: a configured control character is intercepted before
RC input delivery, connect mode requests attention at session establishment,
and Telnet `BRK` requests the same console operator interrupt independently of
the selected policy.

### Optional V2 guest terminal protocol

Each CONSOLE/RC851 configuration may select PROTOCOL=CLASSIC (implicit default)
or V2. `terminal_v2.c` handles only the opt-in guest protocol, sharing the
existing FIFO, timeout and output transport. Classic I/O takes its original
path; selecting V2 does not select a different CPU, monitor or Telnet codec.
The following is the contract implemented by `src/terminal_v2.c` and its
synthetic protocol tests, not a claim of complete guest-monitor integration.

Control modifier 0 sets and modifier 1 gets an eight-word attribute block at
the W-register's RC halfword address (low alignment bit ignored). In native
right-aligned 24-bit words, word index 1 holds type in bits 0..9; word index 3
holds the prompt byte in bits 8..15. GET returns zero in other words and adds
bit 11 (`echo.yes`) to the type word. Only types 0, 1 and 2 are supported;
initial values are type 1 and prompt 7. Invalid type or pending input gives
Ex23; an invalid complete core range gives Ex22. Validate before mutation,
do not wrap addresses, and generate no attribute-completion interrupt.

Read modifier bit 0 disables echo; bit 1 disables backspace echo only. Input
tables 0/1/2 correspond to console/data-entry/editor. Read status adds terminate
bit 20, blind-character bit 19, backspace bit 18, cancel bit 17 and attention
bit 16 to the converted character. A literal NUL is a blind character; DEL
selects entry zero. Echo bytes are captured when input completes and emitted
once on sense, so later attribute changes cannot reinterpret a completed read.
Write modifier 1 emits the prompt rather than W; other writes emit W's low
seven bits. NUL output is suppressed and a write cancels a pending read/echo.

Existing direct I/O/operator interrupts, idle Escape attention, FIFO and coarse
timeouts remain. Host DEL mapping, ANSI filtering and Telnet attention may
consume/convert bytes before guest-table handling; these are not table errors.
This is not a complete serial-terminal emulation: no UART/transmit timing,
disconnect controls or expanders are implemented in this block. Unsupported
controls report disconnected rather than appearing to implement those features.
The source's PC composite-key translation is not adopted. No guest screen
editor or V2 monitor boot has been verified.

## RC2000 paper-tape reader

The RC2000 reader accepts a raw sequence of 8-bit frames including the
physical parity bit. Zero frames are blank leader or feed and are skipped.
Read modifier 0 checks odd parity and returns seven data bits; modifier 1
checks even parity and returns seven data bits; modifier 2 returns all eight
bits using the general-input parity convention. Parity
failure sets architectural status bit 1, represented by bit 22 of the
right-aligned word. End of file sets status bit 5, represented by bit 18.

Reads complete synchronously and latch
the configured device interrupt. Mount and rewind also latch it. Sense returns
the last character and status immediately. This is sufficient for the supplied
monitor's interrupt-driven buffered reader process without introducing host
timing into deterministic execution.

Control-panel autoload and the `aw` instruction use the same device callback.
They request odd-parity input, discard the parity bit, and combine the lower
six bits of four nonblank frames from left to right. Bad parity or end of paper
returns the CPU to reset state. The shipped configuration defines the physical
reader as device 0 but leaves it empty until a tape is attached.

The RC150 punch on device 1 is the inverse raw-frame path. Write modifier 0
adds odd parity to seven data bits, modifier 1 adds even parity, and modifier 2
writes the low eight bits unchanged. Successful output latches the configured
interrupt. Sense reports status bit 2 (right-aligned bit 21) when no output
tape is mounted; a host write failure reports the historical low-supply status
bit 5 (right-aligned bit 18). Output is flushed and synchronized before an
operator detach. The native frontend refuses to
truncate an existing host file unless the operator explicitly selects
`replace`.

## Historical DFC403/RC4819 interface

`src/dfc403.c` implements the controller protocol separately from the
simplified RC433X monitor-message protocol.
Controls select a unit, set the first cylinder/head/sector and transfer size,
read/write directly to core, read/write address marks, return to cylinder zero,
seek forward/reverse, and reset selection. Sense modifiers 0 and 8 expose
the ordinary and technical status registers. Separate data/head interrupts
are latched until polled. Status errors include wrong second index, invalid
core/physical address, absent-unit local state, and seek errors. Load-core-
address commands clear the historical controller error subset.

Geometry is 406 cylinders, 20 heads, 9 sectors per track, 256 24-bit words per
sector. Up to eight selectors are represented. File-backed drives
reuse only the packed-sector/COW storage primitives, never RC433X messages.
Base files stay read-only and need not be expanded or rounded to cylinders.
Reads beyond available file sectors leave remaining core unchanged. RAM
drives are zero-initialized and reject transfers beyond their fixed capacity.
All full-transfer core and physical bounds are checked before data movement.

Intentional limitations: operations complete synchronously; there is no busy
interval, rotation delay, concurrent transfer, or pending-head-over-busy state.
Repeated invalid seeks are bounded without host-width wrapping. Input-address
returns the packed address; output-address and set-mode are no-ops. The undocumented
controller mode register is not expanded into a full electrical model.
Persistent overlays, write-through, and runtime media replacement currently
remain RC433X-only features.

`dev/python/make_standard_monitor.py` exports the existing disk-resident
`stdmonitor` using the current monitor and native guest `tohost`, with volatile
disk writes. The 39,168-byte result is verified by a fresh DFC403 boot, date
initialization, RC attention/FP startup, text/binary imports and exports, and
nonzero read/write/seek counters. SHA-256 of the exported fixture is
`acd1f238c18357bf5b9dfa8ae9338e3775845956d7ccbd53b552d300f0652be8`.
The verified result is now supplied as src/monitor/STDMON and copied by the
ordinary build. The optional developer standard-monitor target regenerates
a candidate under build/generated/ for comparison, not automatic promotion.
RC4000.dfc403.cfg selects the supplied copy without a Python requirement.
The native and original UNIT0 fixtures remain byte-identical to their recorded
hashes; default RC433X boot remains unchanged.

## Emulator-specific RC433X disk

The shipped monitor uses a synchronous emulator-specific disk
protocol instead of reproducing the physical RC433 controller. A modifier-zero
control command receives the address of an eight-word transformed monitor
message. The fields used by the device are:

| Word | Meaning |
|---:|---|
| 0 | Operation in the high 12 bits: 3 reads and 5 writes. |
| 1 | First core byte address. |
| 3 | First disk sector. |
| 6 | Sector count in the high 12 bits. |

Each image sector is 768 host bytes and contains 256 RC words. Within each
word the least-significant byte is stored first. One sector therefore occupies
512 RC byte addresses in core. The controller validates the complete core
range before copying and rejects malformed image sizes when opening a file.
Reads beyond the physically present final sector leave the remaining core
area unchanged.

The safe controller modes open their base image with `rb`. Read-only mode
returns the historical parity/error status bit for a monitor write.
Copy-on-write mode stores every complete written sector in a sparse, volatile
in-memory overlay.
Later reads consult the overlay first and otherwise read the base image. The
overlay may contain sectors beyond the physical end of the base image, while
an unwritten read beyond that end still leaves the corresponding core area
unchanged. Rewriting a sector replaces its one overlay entry. Destroying a
volatile device discards the overlay, so neither read-only nor volatile mode
changes or expands the image.

Persistent mode stores the same sparse sectors in a separate versioned
sidecar. Its 48-byte little-endian header contains an eight-byte magic value,
format version, header and sector sizes, base-image byte size, base-image
FNV-1a checksum, and record count. Each sorted record contains a 64-bit sector
number, 768 sector bytes, and an FNV-1a checksum over the encoded sector number
and data. Exact file size, header fields, base identity, record checksums, and
duplicate sectors are validated before any record becomes visible. Updates
use a same-directory temporary file, `fsync`, and atomic `rename`; the base
image remains open read-only. Sparse sectors beyond its end remain supported.

An explicitly selected write-through mode opens a complete disposable image
with `r+b`, writes and flushes each 768-byte sector directly, and extends the
file when a sector beyond its current end is written. It allocates no overlay
entries. Command-line selection requires a separate `--disk` path; the
configuration and operator forms likewise use the conspicuous `WRITABLE=YES`
and `writable` keywords. This mode is intended for verified working copies,
never the checked-in `src/disks/UNIT0` fixture.

The standard RC4000 backing-store catalog is a layer above that sector
mapping. The chain-table head begins in sector zero of the supplied image and
describes the slice length, device slice range, catalog size, first catalog
slice, and the slice chain containing the chain table itself. A chain element
is a signed 12-bit relative link; zero terminates a chain. The supplied
`UNIT0` catalog uses three sectors per slice and contains 36 catalog segments.

Each catalog segment occupies one RC433X sector. It contains 15 entries of 34
RC halfwords (17 words), leaving the final word for the monitor's segment
metadata. The fields decoded by the native read-only catalog parser are the
first slice, name and permission keys, lower and upper bases, 12-character
entry name, signed size, document name, short clock, file, block, contents,
entry, and final tail word. A positive size denotes an area; zero and negative
sizes are retained as non-area entries without guessing a narrower type. The
parser follows both chain-table and catalog slice links, validates image and
device bounds, and rejects premature or cyclic chains. `UNIT0` currently
contains 415 occupied entries in the historical `UNIT0.original` fixture.

This device-level catalog is not the monitor's main catalog. `MON8INIT` defines
the main name as `catalog`; `MONTXT10` distinguishes its pseudo chain head from
auxiliary/device catalogs. The supplied main area starts at slice 964 and spans
72 sectors, using the same 17-word layout and 15 slots per sector. Its 418
pristine entries contain all device names plus `c`, `v`, `catdisc`; area document
fields differ between layers. `rc_catalog_read_main` locates that area through
the device catalog, validates positive size and the same document, then follows
its slice links with the same range/premature/cycle checks. Arbitrary main names
and validation of the descriptors' backing allocations are not implemented.

`cata` counts main-catalog slots: 72 * 15 = 1080. The integration checkpoint
compares its free-slot report with empty markers in a guest-exported binary
`catalog`, not the device catalog's 540 slots. The working image also shows
stored temporary `gone` remains in the main area after device reclamation.
This establishes a stale-descriptor boundary, not its exact cleanup/cache
mechanism. Offline listing is not the live monitor's filtered namespace or a
complete consistency checker.

## Protection compatibility

The default profile does not enforce storage protection. Process claims are not a security
boundary. Protection-related operations still check installed core addresses:
`kl` returns key zero, `ks` and `pl` have no architectural effect,
`ps` stores 0x080 in the addressed byte, and `sp` always skips for an
existing address. These compatibility stubs do not establish key enforcement.

Core now stores one zero-initialized three-bit key per word (including the
four working-register address slots). Both halfwords select the same key;
an odd final halfword has its own containing key slot. The host key API rejects
out-of-range addresses and values above 7. Ordinary data writes preserve keys.
Allocation failure cleans up data and keys; full core reset clears both.
CPU PR initializes to `0x80`; its setter forces bit 0 and its key predicate
uses RC MSB-first numbering. Panel reset preserves PR and keys; full machine
reset clears keys and restores PR.

The machine API's `protection_enabled` flag now explicitly enables experimental
fetch, CPU write and privileged-opcode checks. It defaults to false and survives
reset. `PROTECTION MODE=RC4000` selects it at startup; OFF or an omitted
directive preserves compatibility behavior. `--check-config` reports the
selection, `show cpu` displays it with PR, and read-only `show keys ADDRESS
[COUNT]` displays up to 256 word keys and their PR mask bits. Decimal/0x addresses
match `examine`; odd addresses select the containing word. A displayed PR bit
does not imply active protection or a task-mode access denial. In this mode,
`kl`/`ks`/`pl`/`ps`/`sp` operate on real keys and PR, IRQ 0 cannot be masked,
and fetch updates monitor/task mode from the instruction word's key. Ordinary
data reads are not denied by keys. `jl` checks its target before setting a link;
`jd`/`je` require monitor mode, with target mode determined at the next fetch.

CPU-only write helpers preserve the specified `rx` loaded-register effect,
`ds` partial stores and normalization-before-count-store ordering on faults.
Interrupt entry and host/device/debugger accesses bypass those helpers and
preserve keys. Successful protected-profile autoload explicitly rekeys the
loaded word to zero. Bounded protected guest workflows now pass with a separate
rebuilt monitor and derived software profile; this does not certify arbitrary
programs on the shipped image. See the
[protection contract and CPU coverage](PROTECTION_SPEC.md) and the
[integration plan](PROTECTION_PLAN.md) for acceptance status.

On failed autoload in the protected profile, the machine enters panel reset,
preserving PR/keys and loaded data; an instruction failure saves its already
advanced IC. A panel-autoload failure already in reset does not overwrite the
previous reset's saved IC. Compatibility-profile error handling is unchanged.
Packed core-file loading is a host operation that preserves keys, including on
rejected malformed or out-of-range input; it is not a protection-state restore.

### Monitor patch required before enabling protection

The maintained monitor source has an early return in
[monsrc/montxt10](../../monsrc/montxt10), procedure e22 (set pk), before
the retained key-setting loops g0/g1 and range checks g2.

[monsrc/montxt11](../../monsrc/montxt11) calls e22 from the start-internal-process
path using the process key a25, and from the stop path using the parent key.
Process first/top bounds are a17/a18. The skipped helper affects both paths,
not just one start call.

The developer rebuild workflow's guarded `--protected` variant restores this
loop in a separate prepared source copy. The maintained source and current
monitor fixture remain unchanged. Bounded tests cover complete process-area
keys, sibling isolation, nested process delegation and subtree stop/resume;
removing one early return alone is not protection correctness.

The selected experimental software profile rebuilds separate `rcin`/`rcout`
utilities against the installed older `tofrom` module and gives that module a
matching system-scoped catalog binding on a private disk copy. The shipped
`fromhost`/`tohost` binaries still contain newer RC8000 GG instructions at the
RC4000 PL opcode. Enabling protection with the unchanged default software is
therefore not a supported migration. No GG semantics or implicit RC8000 CPU
profile are inferred from this conflict. The protected profile retains IX and
the newer `nsystem`; bounded test paths do not execute its known GG probe.
This evidence does not establish compatibility with another monitor variant
or every installed library path. Reproduction and fixture identities are in
[Development](DEVELOPMENT.md) and [Test Fixtures](TEST_FIXTURES.md).

## Control-panel state transitions

The native machine-control API follows sections 11.3 through 11.5 and 14.4 of
the reference manual rather than treating the operator commands as arbitrary
debugger actions. Entering reset from execution stores the current instruction
counter in core word 10, disables interrupts, selects monitor mode, and leaves
core and attached devices intact. Repeating reset while already in reset state
does not overwrite word 10.

Start and autoload are accepted only in reset state. Start loads the instruction
counter from word 14, clears its sign bit and byte-address alignment bit, and
leaves reset state. Autoload reads four six-bit characters through device 0
into W0, then begins instruction execution at address 0. A disconnected reader
or input error leaves the CPU in reset state. The public control API also
allows a frontend to enable or disable interrupts and to edit IM, IR, and the
three exception fields with validation, without depending on terminal code.

## Core-write observation

The machine exposes an optional write observer at its core-store boundary.
Successful word and halfword stores report the aligned word address together
with the previous and resulting 24-bit values. Register aliases W0 through W3
are deliberately excluded. Because CPU instructions and device DMA both use
this boundary, the operator frontend can implement core-write breakpoints
without placing debugger policy in the interpreter or individual devices.
Observation occurs after the store; the frontend therefore stops at the next
safe instruction boundary rather than interrupting a partially completed
instruction or transfer.

The RC433X image stores each 24-bit word least-significant host byte first.
Reversing those three bytes restores the monitor's character order. Structural
catalog traversal of the pristine `UNIT0` locates `catalog`, `startarea`,
`slang`, and `slangxref`. After interactive device time was tied to monotonic
host time, the previously observed `key trouble` during user-process creation
disappeared. The monitor now creates `myproc`, loads the `geo` utility from
`UNIT0`, and reaches its `ARE YOU READY?` dialogue. A deterministic fixture
also verifies this complete path with instruction-count-based device time.

The disk-resident `slang` entry is executable, not merely catalog metadata. A
native integration checkpoint boots the system, initializes the date, creates
the `sltest` user process, and submits `slang missing` to FP. The assembler is
loaded from `UNIT0` and responds with `source unknown` followed by
`***slang sorry 1 0/0/0`. This wording differs from the later OCR manual's
`connect <source file>` description and is treated as version-specific. The
checkpoint completes after 2,759,551 interpreter calls with 239 disk reads and
501 writes in the volatile overlay; the pristine image remains unchanged.

## Monitor source and assembler availability

The files under monsrc/ are SLANG 3 monitor sources, using named expressions,
local/global blocks, conditional assembly, text constants, repetition,
segmentation and source inclusion. The supplied disk contains the full guest
assembler; a host-side instruction decoder is not a replacement for it.

MONITOR is a ready-made compatibility artifact; building from sources is not
required for startup. Guest SLANG reproduces it byte for byte with the maintained
generation files. See [the rebuild workflow](DEVELOPMENT.md#rebuild-the-historical-monitor).
A separate C host assembler is only a possible future convenience.

The native host-file service is called `HOSTFILES` in host-facing
configuration and documentation. Both the native `fromhost`/`tohost` and the
historical `fromdos`/`todos` utilities require the guest-visible process name
`dos` and device number 63. Keeping this compatibility name inside
the emulated machine avoids changing the supplied monitor. Host paths remain
POSIX paths confined to the configured exchange root.

`src/hostfiles.c` implements the host-file control protocol: modifiers 1/2
set conversion mode and transfer size; 3/4 input/output packed characters;
11/12 open input/output; 13 closes the current file. Sense modifiers 0/1
return status and character count. Transfers pack three 8-bit bytes in each
24-bit word at an even halfword address and cap each operation at 6144 bytes.
Open, transfer, and close request the configured completion interrupt (21 in
the supplied monitor). Status bits 23, 18, and 14 mean intervention, end of
document, and fault respectively. Size/mode controls do not raise interrupts.

The native implementation pins the configured root directory and resolves
guest paths component by component with `openat` and `O_NOFOLLOW`. Output uses
`O_EXCL` for non-overwrite output creation. Only regular files are
accepted. All core transfer ranges are checked before performing I/O.
Text conversion deliberately uses LF and preserves character bytes without
code-page translation; mode 0 is binary. EOF appends one text end
marker (25), and short input blocks are padded to complete RC words. Export
stops at marker 25 and removes NUL/CR. Text encoding beyond ASCII remains
future work.

The native monitor checkpoint imports `hfinput` with `fromhost` into an RC
file, exports it with `tohost`, and compares the LF text byte for byte. It also
verifies that the pristine `UNIT0` image has not changed. Its transcript is
available through `sim4000_tests --hosttest`.

The `--slangbuild` checkpoint extends that path with the maintained LF/ASCII
source `tests/fixtures/slang_integer.sl`. FP invokes `hfprog=slang hfsource`
and the delivered assembler reports `slang ok 1/12/1`. Binary export through
`tohost` in binary mode produces one 768-byte allocation segment. Its 18-byte prefix
contains two reserved words and the independently derived instructions
`2c007b 2d0141 1c0002 348000`; trailing allocation bytes are not zero-filled
by the supplied tools and are excluded from the code comparison. Loading
`hfprog` through FP executes those instructions, produces W0=444 and W1=321,
and enters a relative self-loop. The checkpoint checks registers and IC at
that boundary rather than relying on a host debugger or a console transcript
alone. It verifies the source/import/assembly/export/load/execute workflow,
not equivalence of the historical monitor source snapshot and `MONITOR`.

The native `UNIT0` snapshot rebuilds the file-transfer tools from the
maintained Algol variants under `src/guest/`. The generation session imports
both sources before renaming `fromdos.fromhost` and `todos.tohost` with the
guest `rename` utility. Compilation overwrites those existing global entries,
preserving permission key 3 and base interval `800001..7ffffd`. This requires
an `all` process; a regular `new` process cannot rename these protected
entries. A bounded temporary disk claim prevents the compiler's work area
from extending to the whole disk.

The Algol compiler reports `algol end 55` for the importer and `algol end 47`
for the exporter. The native sources change diagnostics to host terminology,
default to byte-preserving text mode 2, explicitly initialize conversion
parameter state, and retain the `dos` process lookup for binary-monitor
compatibility. They do not retain `conv.dos`/`conv.dostext` aliases.
Generation validates new catalog names and executable descriptors, removes
temporary build entries, and reboots the derived image under copy-on-write
before testing both programs in a normal user process. The original fixture
is not modified. The selected native snapshot is version-controlled alongside
`UNIT0.original`; sources and the build procedure reproduce it independently.
The `native-image` target produces a separate `UNIT0.host` candidate, leaving
both shipped snapshots intact. Normal configuration selects `UNIT0`, enables
`HOSTFILES`, and uses the CMake-created `exchange` directory. Historical
disk-layout/boot reference fixtures explicitly select `UNIT0.original`;
current host-file, SLANG and workload checkpoints use the native default.

### Source provenance

The maintained monsrc/ snapshot is historical RC monitor source with later
maintenance and emulator-specific modifications. It identifies Monitor 3
version 10.11.77; the current generation options select the corresponding
disk/host drivers.

All monitor fragments plus the maintained options file reproduce MONITOR
through the disk-resident assembler. Files use LF and lowercase names.
Two non-ASCII Danish comment characters are mapped to historical seven-bit
positions only in disposable build copies. This establishes a matching
source/build pair, not a pristine vendor release, original Monitor 1 source
or redistribution clearance. See [Provenance](PROVENANCE_REVIEW.md).

### Independent monitor listing from Datamuseum

[BitArchive artifact 30003039](https://datamuseum.dk/wiki/Bits:30003039),
labelled "RC 8000 Monitor Kildetekst", is a public SIMH tape image. The
verified image is 2,055,818 bytes with SHA-256
`262a4ceeccbf92b023313a1e7050117a72be3385557528b034db1d143a64d82c`.

Its first complete `monitor8000` dump contains `mon8part1` through
`mon8part6`, plus `mon8list`, a print command file. Despite the archive's
"source" description, the six large files are assembler listings: numbered
source lines with assembly addresses, generated messages, and identifier
cross-references. They are not directly usable replacements for `monsrc/`.
The archive's decoded entry dates are 1982-06-01; the embedded generation
summary says release `08.00`, version `82.06.15 12.00.00`, options
`79.03.27 19.00.00`, and `slang ok 11/42354/83`. These dates describe
different metadata fields and must not be conflated into a proven build date.

A local extraction of all seven allocated areas agrees with the sizes and
artifact hash prefixes in [Datamuseum's decoded namespace](https://datamuseum.dk/aa/rc489k/9a/9ab0fc1ed.html).
The full SHA-256 of `mon8part1` was also independently checked against the
archive's downloadable extracted binary:
`6bbed22891763d10824320e4d098ff780553a15beac4ab39cfba913183088475`.
Later tape files include incomplete/unrelated dumps and are outside this
inspection's scope.

After stripping only numbered listing columns and normalizing whitespace,
comparison with the local source files finds:

| Local file | Shared nonempty line occurrences | Longest identical block |
|---|---:|---:|
| `mon8s` | 3896 / 4037 | 736 lines |
| `mon8init` | 2341 / 2398 | 403 lines |
| `montxt11` | 3986 / 4146 | 638 lines |
| `montxt10` | 1280 / 1324 | 202 lines |
| `montxt3` | 425 / 2040 | 7 lines |

Comments are included, repeated local lines count repeatedly, and the
archive comparison corpus concatenates all six numbered-line derivatives.
These are textual comparison measures, not percentages of equivalent code.
The derivatives omit unnumbered output and may include inactive conditional
source; they have not been validated as assemblable input.

The archive uses modules such as `mondef`, `moncentral`, `monfpaline`,
`monhost`, `montabinit`, `monprocfnc1/2`, `mons1/2`, and `moncatinit`.
Its first global declaration is `b.a800,b200 w.`, unlike local `a199,b136`.
It includes RC8000/FPA-specific drivers and conditional RC4000 paths. The
local source identifies Monitor 3 version 10.11.77 and includes later
emulator-specific disk and host-device changes. The long common blocks are
independent evidence of substantial shared historical code, especially in
the higher-level system and catalog machinery. They do not establish direct
descent from this particular tape, a pristine vendor release, or the
original 1969 Monitor 1 source. No archive file is installed into `monsrc/`
or used to replace the shipped monitor.
