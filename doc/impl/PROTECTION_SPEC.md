# Protection Specification

Architecture contract for [Protection and Monitor Integration](PROTECTION_PLAN.md),
established in M1 and implemented as the optional M2 profile. The compatibility
stubs remain unchanged when protection is disabled. Implementation coverage and
bounded M3 integration evidence are summarized below; the contract alone is not
a claim of exhaustive verification. Choices
not specified by the manual are identified as emulator policy, not hardware facts.

## Confirmed architecture rules

Source: *RC 4000 Reference Manual*, June 1969, sections 6.1--6.3, 14.4--14.6
and protection instruction definitions in 14.8. The available text is OCR;
cross-check pseudocode and explanatory prose wherever they disagree.
RC bit numbering starts at the most significant bit.

- Each 24-bit storage word has a separate three-bit key. Keys do not travel
  with data during ordinary loads/stores.
- PR is an eight-bit protection mask, not a process key or enable flag. A set
  bit means protected. PR bit 0 is always set: `0x80` in a host `uint8_t`;
  key `k` selects `0x80 >> k`.
- Task mode prohibits entering protected code, writing protected storage and
  executing privileged instructions. Ordinary data reads and indirect-address
  reads are not automatically prohibited by storage keys. This is not a
  read/write/execute page-permission model.
- Interrupt entry selects monitor mode and disables interrupts. A task fetching
  protected code raises instruction exception. Otherwise the fetched word's
  protection status determines monitor/task mode. Fetching unprotected code
  leaves monitor mode; a privileged branch alone does not preserve privilege.
- Fetch pseudocode increments IC before the protection test. Protection faults
  must follow the specified interrupt path, not become host execution errors.
  Instruction exception requests interrupt 0; IM bit 0 is specified as always
  enabled. Check fault ordering and saved IC against the current implementation.

## Protection instructions

| Instruction | Intended operation | Privileged |
| --- | --- | --- |
| `kl` | Load the word's key into W's low three bits, clearing the rest of W. | No |
| `ks` | Store W's low three bits as the word's key; preserve W. | Yes |
| `pl` | Load PR from the addressed byte's low eight bits, forcing PR bit 0. | Yes |
| `ps` | Store PR as a zero-extended 12-bit byte, subject to write protection. | No |
| `sp` | Skip if the addressed word's selected PR bit is zero. | No |

All five instructions validate effective addresses. `kl`, `ks` and `sp` use
the containing word's key; `pl` and `ps` select the addressed halfword.
`sp` depends on PR and the key, not the current monitor/task mode.

Other privileged operations include mask loading, interrupt-bit clearing,
interrupt-enabled/disabled jumps, I/O and autoload. Check each instruction's
pseudocode for test order before changing registers or device state.

## Access-path inventory and ordering

The CPU implementation is concentrated in `src/machine.c`; `src/floating.c`
computes register results and does not itself write guest memory.

| Path / instructions | Protected-profile contract |
| --- | --- |
| `fetch_instruction`, including each `am` continuation | Read instruction and key, increment IC, test task entry, then set mode from PR/key before effective-address calculation. No interrupt service between `am` and its continuation. |
| `effective_address` indirect read; ordinary arithmetic/load operands | Address validation only; no key-based read denial. Preserve existing low-core register alias ordering. |
| `hs`, `rs`, `xs`, `ms`, `is`, `ps` | Validate destination and test protection before changing its data. Halfword writes use the containing word's key. |
| `rx` | Save source W internally; validate/read target into W; test protection; write saved source to target. On denied write, loaded W survives, but target data is unchanged. |
| `ds` | Validate/check/write the addressed word first, then check/write the preceding word. A second-word fault retains the first write. Preserve register-alias effects between the two writes. |
| `ns`, `nd` | Normalize W or the register pair first; then validate/check/store the count. A denied store retains normalized registers. |
| `jl` | Validate/check target before changing link or IC. A task jumping to protected code faults at the branch, not only at the subsequent fetch. |
| `jd`, `je` | Check privilege first, then target address, then interrupt state/link/IC changes. Target fetch applies mode rules. |
| `ml`, `ic`, `io`, `aw`, `pl`, `ks` | Check privilege before opcode-specific reads or side effects; indirect addressing still occurs during decode, before opcode execution. |
| Interrupt entry | Store vector number and saved IC, read handler entry, set monitor mode and disable interrupts. These control-unit accesses bypass task write checks. |
| `aw` / panel autoload | On a successful four-character transfer, store data and set the destination key to zero. Panel autoload targets W0 and its key. |
| IX extension/bounds | Retain the defined optional IX extension/bounds operand reads as data reads; do not invent an undocumented RC4000 instruction-fetch rule for its inline data word. |

Section 3.3 explicitly gives W0--W3 their own keys. The key slots for byte
addresses 0--7 therefore belong to the register aliases exposed by
`rc_machine_read_*`/`write_*`; normal result-register assignment does not itself
perform a storage-protection check. An instruction executing in a working
register is subject to that register word's fetch key. Normal memory transfers
must not copy keys along with data.

`rc_machine_read_*`/`write_*` are also called by `src/core_image.c`, host-file
service, RC433X, DFC403 and RC749. They must remain host/control/device accessors,
not become implicit task-mode access checks. Add explicit CPU-only checked
write/target helpers for the instruction paths above. Keep existing write
observer behavior: denied writes emit no write record; successful partial
writes remain observable. Use the same centralized key lookup for checks,
`kl`/`ks`/`sp`, and instruction fetch.

Section 9 states that high-speed I/O does not change storage keys. Device DMA
therefore preserves keys and is not gated by the interrupted task's PR; I/O
initiation is privileged. Host-file transfers are an emulator service using
the same explicit device-access boundary, not a new protection mechanism.
Debugger reads/writes, monitor loading and packed core data loading bypass
guest protection intentionally. Protection is not a sandbox against the host.

## Interrupt and lifecycle decisions

- Instruction exception uses existing interrupt 0 entry without executing a
  handler instruction in the same faulting step. A decoded instruction fault
  saves the already advanced IC. A fetch-address failure occurs before that
  advance. `jl` denial must not store a link or redirect IC first.
- Interrupt 0 must remain enabled in the protected profile even if host control
  APIs supply an IM value with bit 0 clear. Other pending interrupts retain
  architectural priority. Preserve existing disabled-interrupt treatment and
  non-protection exception behavior in the compatibility profile.
- Key storage is separate from packed 24-bit data, with one three-bit value
  per word, including the four register aliases. Core allocation initializes
  keys to zero; failed allocation must not leak the data allocation. CPU PR
  initializes to `0x80`. These deterministic initial values are emulator policy:
  section 14.4 does not initialize PR or all storage keys explicitly.
- Full `rc_machine_reset` clears data/keys and resets CPU/PR. The panel reset
  operation preserves data, keys and PR while selecting monitor/reset state.
  Starting from panel reset preserves PR/keys. Successful autoload sets only
  the loaded word's key to zero, as required by sections 11.5 and 14.8.
- Packed core files remain data-only, not protection or session snapshots.
  Loading into an existing machine preserves keys; a fresh machine or full
  reset provides zero-key bootstrap storage. Failed data loads must not change
  keys. The monitor loader writes into freshly initialized storage in normal
  startup; it must not implicitly rekey unrelated memory.
- The explicit profile setting is `PROTECTION MODE=OFF` (default) or
  `PROTECTION MODE=RC4000`, on a separate configuration line, implemented in M2.
  Duplicate/unknown values are rejected. It does not
  overload `CPU IX` or infer behavior from monitor names. No live-mode switch
  is required: use another configuration for another profile.
- Propagate the profile during startup, preserve it over reset, and expose
  mode/PR through bounded CLI diagnostics plus read-only key inspection.
  Preserve existing trace version-1 columns; any future protection-aware trace
  extension must be separately specified rather than silently changing v1.

## Monitor integration findings

In `monsrc/montxt10`, `e22` loads the process's first core bound and returns
through `x2`. This return, marked `94-06-01 21:03 HJ`, bypasses the retained
`ks` loops and range checks. The loops cover ten words at a time and then the
remainder of the process area.

The maintained protected entry is monsrc/sim4000-protected.mops. Its historical
guest-editor commands remove the recognized HJ jump only in temporary ptxt10,
then invoke the shared sim4000.mops build and restore the guest original name.
Python checks the normalized input identity, unique bypass and exported guest
edit/restoration; it no longer rewrites montxt10 during preparation. The two
FP input levels are wrapper and shared build, with no third command file.

In `monsrc/montxt11`, the start path supplies the process key from `a25`;
the stop path supplies the parent's key. Both calls are conditional on a
generation expression involving `a128`. The maintained default is `a128=2.1`
(binary 1); `(a128 >> 2) & 1` is zero, so its negation is zero. SLANG section
3.8 assembles a conditional body for a nonnegative expression: these two calls
are included. The maintained `sim4000.mops` build edits (initially inherited
from historical `hjmops.new`) do not replace that option or those calls.
Verify their generated candidate positions in M3 rather than relying only on
source interpretation.

Descriptor offsets are `a17=22` (first), `a18=24` (top), `a24=32` (PR) and
`a25=33` (key). The `e22` loops start at first and cover words strictly below
top: the single-word tail tests `i+2 <= top` before writing `i`. Candidate tests
must include short areas, exact ten-word blocks and remainders, and verify that
the adjacent words retain their keys.

`montxt2` selects a process by loading its PR and setting all four working
register keys from its descriptor. This is another essential integration point,
not an optimization to remove. `mon8s` contains RC4000 key allocation and
absolute-protection controls; some system descriptors intentionally retain PR
`1<7` and key zero. Do not blindly rekey privileged system processes. M3 must
prove that ordinary child processes receive distinct usable keys and return
through the intended monitor paths. A key-zero process running successfully
does not demonstrate isolation.

## Independent M2 acceptance vectors

Unless stated otherwise, use code at RC byte `0x100`, next IC `0x102`, data at
`0x180` and a valid interrupt-0 handler at `0x300`. Task code has key 1 with
PR `0x80` (key 1 allowed). Handler code has key 0. Install data/key fixtures
through explicit host setup, not through the instructions under test. Expected
values below derive from the manual, not from comparison with the compatibility
stubs. On a serviced fault, vector location 8 contains zero, saved IC at 10
contains the specified fault IC, execution points at `0x300` in monitor mode,
and interrupts are disabled.

| Vector | Setup / expected result |
| --- | --- |
| P01: mask direction | For keys 1--7, PR `0x80` allows a task store; PR `0x80 \| (0x80 >> k)` denies it. Key 0 is always denied in task mode. |
| P02: fetch transition | Monitor fetch of key-1 code with PR `0x80` enters task mode. Task sequential fetch of key-0 code faults with saved IC equal to fetched address plus 2. |
| P03: protected reads | `rl` and indirect-pointer reads from key-0 words succeed; task mode remains governed by the instruction key, not the operand key. |
| P04: scalar writes | Denied `rs`, both halves of `hs`, `xs`, `ms`, `is` and `ps` preserve target data and EX; save IC `0x102`. Allowed writes change data, never keys. |
| P05: exchange fault | W2=`0x123456`, target=`0xabcdef`, target key 0: denied `rx w2` leaves W2=`0xabcdef`, target=`0xabcdef`; save IC `0x102`. |
| P06: partial double store | W2=`0x111111`, W3=`0x222222`; `ds w3 0x182` allowed at `0x182`, denied at `0x180`: first word becomes `0x222222`, preceding word is unchanged; save IC `0x102`. Reverse protection denies both writes at the first check. |
| P07: normalize fault | W2=`0x100000`; denied `ns w2` count store leaves W2=`0x400000`, target unchanged and EX unchanged. Repeat with a register-pair `nd` vector. |
| P08: direct branch fault | `jl w2 0x180`, protected target: W2 unchanged, saved IC `0x102`, not `0x180`. |
| P09: privileged branch | Task `jd`/`je` changes neither link nor enable state before the fault; monitor branch to key-1 code remains monitor until that target is fetched, then enters task mode. |
| P10: privilege coverage | Task `ml`, `ic`, `io`, `aw`, `pl`, `ks` faults without operand/key/mask/device side effects. Test address-error precedence and an indirect pointer that aliases W. |
| P11: PR instruction | Monitor `pl` from byte `0x07f` produces PR=`0xff`; from zero produces PR=`0x80`. `ps` stores `0x080` for PR=`0x80`, not a process key. |
| P12: keys and skip | `ks` of W=`0xabcdef` sets key 7, preserves W and data; `kl` returns exactly 7. `sp` skips for an allowed key and does not skip for a protected key in both modes. |
| P13: register words | Test `ks`/`kl` for addresses 0--7, both halfword aliases, denied indirect stores to W, instruction fetch in W, and direct arithmetic result assignment unaffected by the register's storage key. |
| P14: address bounds | First/last complete word, odd word aliases, invalid halfword/word addresses, preceding-word wrap at zero, and all eight keys. No out-of-range key lookup. |
| P15: modify chain | Allowed `am` at `0x100`, protected continuation at `0x102`: fault saves `0x104`; no continuation operand effects. Test monitor-to-task transition within the chain. |
| P16: control lifecycle | Fresh keys zero/PR=`0x80`; panel reset preserves modified keys/PR, full machine reset clears them; successful `aw` and panel autoload zero only the loaded word's key. |
| P17: bypass boundaries | Device/host writes to protected data succeed without rekeying; equivalent task stores fail. Denied writes produce no observer record; P06 produces only its successful first write. |
| P18: compatibility | Protection OFF retains existing `kl=0`, `ps=0x080`, no-op `ks`/`pl` and unconditional valid-address `sp`, including existing IRQ/mode behavior. |

For P01, choose an allowed instruction key different from the target key under
test; otherwise the test would fault at fetch before reaching its store. For
P10, distinguish forbidden opcode side effects from the mandatory interrupt-entry
changes to IC, IR, mode and enable state.

The dedicated native C protection target is explicitly enabled with developer
tests. Configuration/CLI tests cover default OFF, explicit modes, invalid
values, reset retention and bounded inspection. The M2 coverage summary below
maps executed cases to these vector groups; the table itself is a contract,
not an exhaustive test report. Broad regression and sanitizer runs belong at
the M3 integration gate rather than each specification edit.

## Monitor integration boundary (M3)

Check candidate assembly and descriptor state for other protection-related
monitor adaptations, initial key assignment, legitimate shared buffers and
fault recovery. The `e22` finding is not proof that it is the only bypass or
that the monitor is ready for protection. The binary-only V2 monitor is excluded;
M4 prepares selectable profiles from the maintained monitor instead.

The source-level M1 review found no second explicit key-setting bypass among
the maintained `montxt1`--`montxt11` protection sites. The tagged message/general
copy adaptations retain software address checks and are not additional `e22`
early returns; this is not a security audit of every monitor service.
Process creation in `montxt11` retains PR/key validation, and the bootstrap in
`montxt9` probes the core limit using `ks` and its address exception. The
separately rebuilt candidate boots with enforcement active. Nested-process
tests reject privilege widening before creating a valid child, then check
delegation, full-area rekeying and subtree lifecycle transitions. These are
bounded integration checks, not a complete audit of monitor services.

## Runtime opcode conflict and older library candidate

The *RC 8000 Computer Reference Manual*, January 1976, RCSL 31-D383,
section 10, page 10-3 (PDF page 44), assigns opcode 28 to `GG`, General
Register Get. Its instruction-characteristic pattern does not mark GG as
privileged. This differs from RC4000 PL at the same opcode; it is not a reason
to allow RC4000 task-mode PL to modify PR.

The manual's section 7 (PDF page 31), section 11 (page 45), and appendix A1
(page 74) explicitly leave relevant details unfinished. Section 11 appends
RC4000 instruction definitions for information; those reproduced PL semantics
are not a definition of RC8000 GG. This edition does not establish the meaning
of general register 34 or later machine-identification values. The 60--79
comparison in the observed library remains a feature-probe interpretation,
not a documented register-34 contract.

Read-only inspection of existing UNIT0 libraries supplies a narrower software
candidate without installing archived software or changing the base image:

- The current `tofrom` catalog entry names `algollib`. Its exported binary has
  `tofrom1` and `tofrom2` module labels in zero-based blocks 17 and 18.
- The same GG/PL-plus-comparison sequence occurs twice, in `tofrom1` and
  `system0`. Fixing only one call site is not evidence of runtime compatibility.
- The already installed `oldalgollib` has a single `tofrom` module in block 14
  and lacks that exact word-aligned probe sequence throughout the exported area.
  Absence of that sequence does not prove the old routine's entire instruction
  set, calling convention or protected-mode behavior compatible.

The installed transfer programs contain already-linked tofrom1/tofrom2 modules.
Changing the library catalog entry after compilation does not replace those
copies. The older module's relocation header and parameter-entry prologue
provide a bounded reconstruction of its descriptor, exercised through guest
FP `set` and Algol rather than host-side byte patching:

```text
tofrom=set bs oldalgollib 2048.22 83.2665 0 46.14 1.0
```

On an exclusive disposable image, the unchanged transfer sources compile to
separate `rcin`/`rcout` binaries. Restore the original catalog binding after
linking, retain the installed utilities/libraries, and validate both text and
binary transfers first in compatibility mode and then with the protected
candidate. This bounded checkpoint now passes. It establishes the older
tofrom binding for these programs, not the complete old library's compatibility.
An explicit private system-binding profile instead retains the older descriptor
at system scope, with matching catalog ranges. It supports subsequent protected
Algol builds without the temporary-binding `oldalgollib bases` diagnostic.
This derived image is reproducible but does not replace the shipped UNIT0.

The newer nsystem remains linked, including its system0 GG probe; the tested
transfer path does not execute that probe. Do not interpret a transfer pass
as implementation of GG or coverage of every nsystem operation. A trial using
the old system descriptor for nsystem breaks in system2 already with protection
off and IX enabled. Its underlying incompatibility is not yet diagnosed;
matching external parameter descriptions is insufficient evidence for an alias.

Do not replace the complete library, patch every occurrence of an opcode, or
introduce an implicit RC8000 profile. CPU-family compatibility and library
adaptation are separate decisions from strict RC4000 protection.

## CPU acceptance coverage (M2)

Core key allocation/access and CPU PR setter/mask predicate are implemented.
The native protection test covers key/halfword aliases, all 256 PR inputs and
eight mask positions, invalid inputs, data/key independence and panel/full
reset behavior. It also verifies unchanged compatibility instruction stubs
when the new stored PR/keys are nonzero.

The explicit machine-API flag now enables instruction fetch/write/privilege
checks and real protection instructions. Focused execution vectors cover all
key/mask positions for stores, protected data reads, indirect reads, fetch and
AM faults, scalar denied stores, `rx`/`ds`/`ns` partial effects, branch/link
ordering, all privileged opcodes, `pl`/`ps`/`kl`/`ks`/`sp`, register key aliases,
IRQ-0 mask invariance, host write/observer boundaries and successful autoload
rekeying, including panel autoload. Existing native tests pass with opt-in off.

Configuration/CLI integration now includes default OFF, explicit RC4000,
duplicate/invalid-value rejection, `--check-config`, `show cpu`, `show keys`
and completion. The CLI test executes a literal `ks` instruction, inspects the
changed key and checks panel-reset retention, without booting the unchanged
monitor in protected mode. Display/usage output remains within 80 columns.
Core-file load tests preserve nonzero keys on both successful and rejected
loads; instruction/panel autoload failure tests preserve PR, keys and data.

Extended cases now verify last-word/odd-address aliases, invalid addresses across
the memory/key/branch operations, double-store first/second-word failures,
register aliasing and preceding-word wrap, `nd` partial effects, register-resident
instruction fetch, indirect register stores, IRQ priority and return to task
execution, and monitor-to-task transition within an AM chain. A device poll
callback writes protected storage while a task runs, preserving the key and
remaining distinct from CPU write checks.

| Native test functions | Principal vector groups |
| --- | --- |
| `core_keys`, `protection_register`, `machine_lifecycle` | P01, P13, P14, P16 state/mask boundaries |
| `fetch_and_reads`, `scalar_stores` | P01--P04, P15 fetch/read/write paths |
| `partial_effects`, `boundary_and_alias_vectors` | P05--P07, P13--P14 ordered side effects and boundaries |
| `branches_and_privilege` | P08--P10 direct/privileged branches and opcode checks |
| `protection_instructions` | P11--P12 key/PR instructions |
| `control_and_bypass`, `interrupt_return_and_device_boundary` | P02, P09, P13--P17 IRQ, register, autoload and device boundaries |
| `configuration`, `packed_core_loading`, `autoload_failure_lifecycle` | P16 configuration, loading and error reset |
| `compatibility_stubs` and existing native/CLI tests | P18 unchanged default profile |

M2's CPU/API gate is complete with these representative cases. It is not an
exhaustive test of every operand permutation, physical microcycle behavior or
every device under a protected monitor. The original host import still breaks
in `tofrom1`; see the
[failed checkpoint](TEST_FIXTURES.md#experimental-protected-monitor-checkpoint).
The separate derived profile now passes bounded process isolation, nested
lifecycle, Algol compilation, host exchange and live Telnet checks. These do
not remove the original runtime conflict or certify untested software. The
[integration plan](PROTECTION_PLAN.md) tracks broad acceptance separately.

Move accepted runtime contracts into [Architecture](ARCHITECTURE.md#protection-compatibility)
and assertions into [Test Fixtures](TEST_FIXTURES.md) as they are implemented.
Do not maintain a second, conflicting runtime reference here.
