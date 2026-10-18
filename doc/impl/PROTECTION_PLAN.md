# Protection and Monitor Integration Plan

## Objective and status

Add architecture-backed storage protection and a matching monitor variant
without losing the working compatibility system. Continue with the maintained,
source-reproducible monitor. The binary-only V2 monitor is excluded from this
workstream; its missing source prevents the intended maintainable integration.

Planning and baseline milestone M0 are complete. Overall workstream progress is
approximately **100%**; M0--M5 are complete for the bounded optional profile.
The normal registered suite and sanitizer acceptance checks pass, including
instrumented protected guest workflows. Both profiles and the refreshed manual
are integrated into main; compatibility remains the default. Broader installed-
software coverage is follow-up work, not part of this completed bounded gate.
The optional CPU/core protection
profile has focused native and CLI coverage, including boundaries, register
aliases, partial effects, interrupt return and host/device access separation.
M3 has produced a separate candidate and reached protected startup. Rebuilt
experimental transfers using the older `tofrom` now pass text/binary checks
with protection enabled; the shipped utilities still break in `tofrom1`.
Stop/start/remove/recreation and full-area rekeying now pass a bounded local
checkpoint. Two independently consoled siblings now pass bounded write/jump/
privilege-fault isolation and recovery. Live loopback Telnet now passes separate
FPs, remote removal/recreation and reconnection on the derived system profile.
Nested guest processes now pass short/block/remainder areas, delegation denial,
subtree stop/resume and parent-initiated child stop/removal. Protected guest
Algol compilation/execution and newly
compiled transfers pass. A reproducible private system-scoped older-tofrom
profile eliminates the `oldalgollib bases` diagnostic seen with a temporary
binding. This operational diagnosis is not the historical diagnostic definition
or general software-profile acceptance.
Progress percentages are approximate and refer
to this workstream, not to overall emulator completeness. Check off milestones
only after their acceptance checks pass; record evidence in the existing
[Test Fixtures](TEST_FIXTURES.md) and behavior in
[Architecture](ARCHITECTURE.md), rather than appending a session log here.

## Stable baseline and recovery contract

The stable starting revision is `a5de176`. It includes the refreshed manual PDF
and the existing default MONITOR/RC433X/native UNIT0 environment.

- Before implementation, verify a clean runtime/media baseline, resolve the full baseline
  commit ID and create a non-moving annotated checkpoint tag, proposed name
  `baseline-before-protection`. If that name exists, verify its target; never
  overwrite an unrelated tag.
- Planning-only documentation edits may be carried onto the development branch
  without committing them; do not carry unrelated source or media changes.
- Create a dedicated development branch, proposed name `feature/protection-monitor`,
  from the baseline. Leave the stable branch at the checkpoint until acceptance.
- Keep `src/monitor/MONITOR`, existing monitor sources, the default configuration
  and shipped disk images unchanged during experiments. Store candidate sources,
  monitor binaries and configurations separately. Use ignored build directories
  for generated candidates and logs; version the necessary source changes and
  reproduction instructions before accepting a candidate.
- Each guest experiment uses a verified disposable disk copy or overlay. Never
  use an existing personal working disk or reuse an overlay across incompatible
  monitor profiles. No experimental command may promote media automatically.
- Keep protection disabled in the existing compatibility profile. The protected
  profile explicitly selects protection and its matching monitor. Do not silently
  change existing configuration meanings or infer protection from a filename.
- Retain both profiles as supported selectable alternatives after protected
  integration succeeds, with separate configuration files and matching monitor/
  software artifacts. Promoting a protected profile must not remove the working
  compatibility profile or require editing its configuration to recover it.
- Commit independently reviewable milestones. Do not merge an unsuccessful
  experiment or switch the default profile merely because a monitor boots.

If a milestone fails, retain its diagnostics and correct it on the experimental
branch. If the approach is abandoned, preserve that branch, return to the stable
branch/checkpoint and configure a fresh build directory. Verify the original
monitor/disk hashes and a bounded default boot. Do not use a destructive reset
or delete unrelated work. Git restores tracked source and media, not external
working disks or ignored build outputs; this is why experiments must be isolated.

## Milestones and acceptance gates

### M0: Secure and characterize the baseline (approximately 5%)

- [x] Create and verify the checkpoint tag and development branch.
- [x] Record the baseline commit and monitor/disk hashes using the existing
  fixture definitions; confirm that the default configuration still selects
  the stable artifacts.
- [x] Run focused default boot/date/process checks on disposable media and
  verify unchanged monitor-source reproduction using the existing workflow.

Gate: the baseline is recoverable and its guest build pipeline is usable.
This is not a reason to rerun the complete release or sanitizer suites.
The tag resolves to `a5de1763382bd7611507e35a398ac7e27f026c25`; `main` remains
at that revision during the isolated experiments on `feature/protection-monitor`.
M5 records the subsequent accepted integration; the tag remains unchanged.
See [baseline evidence](TEST_FIXTURES.md#protection-development-baseline).

### M1: Establish the protection contract (approximately 20%)

Initial findings and unresolved questions are in
[Protection Specification](PROTECTION_SPEC.md). This is a design document,
not a claim of implemented protection.

- [x] Read the architecture manual's protection, privilege and interrupt rules,
  checking OCR ambiguities against independent evidence where necessary.
- [x] Define key granularity and encoding, key-zero behavior, `kl`/`ks`/`pl`/
  `ps`/`sp`, register and mode transitions, initial/reset key state, and relevant
  privilege checks. Do not treat current compatibility stubs as specifications.
- [x] Enumerate fetch, indirect addressing, operand reads/writes and device/core
  access paths. Specify which are protected, which are monitor/host operations,
  and what state changes are permitted before each protection fault.
- [x] Trace `e22` in `monsrc/montxt10` and both start/stop callers in
  `monsrc/montxt11`. Confirm process key `a25`, bounds `a17/a18`, and the
  early return marked `94-06-01 ... HJ`. Identify any other bypasses needed
  for this monitor variant instead of assuming this is the only patch.
- [x] Define independently derived positive and negative CPU test vectors and
  the explicit configuration boundary between compatibility and protected modes.

Gate: document expected behavior and unresolved questions before implementation.
Resolve blocking architectural uncertainties rather than guessing semantics.

### M2: Implement optional CPU/core protection (approximately 45%)

- [x] Add key storage and centralized access checks, with explicit lifecycle
  handling for core allocation, reset and host-side core loading.
  - [x] Add key storage, PR state and mask predicate; test data/key separation,
    key boundaries and panel/full reset behavior.
  - [x] Add CPU-only enforcement helpers and successful-autoload rekeying.
  - [x] Verify core-file loading and unsuccessful-autoload lifecycle cases.
- [x] Implement the specified protection instructions, privilege/mode changes
  and fault delivery without changing the compatibility profile's behavior.
  - [x] Implement machine-API opt-in, real key/PR instructions, fetch/write checks,
    privilege checks and IRQ-0 fault entry; retain compatibility defaults.
  - [x] Expose explicit profile configuration and bounded state inspection.
- [x] Cover allowed/denied accesses, key and range boundaries, fetch/indirect/
  operand cases, privileged operations, fault state and interrupt return.
- [x] Review device transfers and debugger/loader access against the M1 contract;
  host inspection must remain usable without bypassing guest checks accidentally.
- [x] Run focused CPU/core tests in both profiles and bounded legacy guest boot.

Gate: independent vectors pass; existing guest execution remains unchanged when
protection is disabled. CPU correctness alone does not certify monitor isolation.
The protection test covers the P01--P18 vector groups with representative
native cases; this is not exhaustive verification of every operand combination
or hardware timing. Broader integration/sanitizer runs remain scheduled at M3.

### M3: Rebuild and exercise a protected monitor (approximately 70%)

- [x] Prepare a separate source variant restoring `e22` key setting and any
  additional confirmed protection-related adaptations. Preserve the original
  source/binary reproduction path.
- [x] Rebuild through the existing guest SLANG pipeline on disposable media;
  preserve source/options hashes, assembly results and candidate identity.
  The guarded `--protected` preparation produces candidate SHA-256
  `baf14abb99d582ad845478034167265087e1aec9d3785da2bd88cdc78298047b`
  (37,632 bytes). This is not an accepted replacement.
- [x] Boot the candidate under explicit protection. Verify date initialization,
  attention, system commands, process creation/start/stop/removal/recreation,
  parent/child transitions and keys across complete process areas.
  - [x] Reach date initialization, guest attention and creation/start of a user FP.
  - [x] Run `claim` and confirm keyed task execution before the host-import failure.
  - [x] Stop/resume/remove/recreate a local user FP; inspect every word's key
    in its claim-reported area and adjacent boundary keys. Cover 50,000 words
    and a 50,003-word remainder case. Verify resumed text/binary content and
    newly imported text after recreation. Clean temporary files explicitly in
    FP; direct process removal is not a verified catalog-cleanup mechanism.
  - [x] Run a key-2/PR-9f parent with a key-1/PR-bf guest child for 1, 10 and
    13-word areas. Reject child privilege widening; check full parent/child
    areas and outer boundaries across subtree stop/resume, parent-initiated
    child stop, stop-answer release and recursive removal.
  - [x] Exercise separately rebuilt transfers using the older `tofrom` binding
    on disposable media, without relaxing CPU checks or replacing installed tools.
    Text/binary round trips pass under protection; original catalog bindings
    are restored after compilation. RC8000 opcode 28 is GG, not RC4000 PL;
    register 34 still lacks a verified contract. This bounded checkpoint does
    not establish general compatibility of all installed runtime paths.
  - [x] Select and verify a reproducible protected-software profile before
    accepting the complete workflow. Keep the original transfer programs and
    default disk unchanged; do not alias `nsystem` to `system` merely because
    their parameter descriptions match. That trial fails in `system2` even
    with protection off and IX on.
    Any adapted library must be separately reproducible on derived
    media; do not enable arbitrary task PL or silently patch the shipped UNIT0.
    The bounded profile is candidate `baf14abb...`, derived system-binding disk
    `ebc56434...`, IX enabled and explicitly selected rcin/rcout utilities. It
    does not certify arbitrary installed programs or change any default media.
- [x] Exercise independent local/Telnet processes, Algol/SLANG and host-file
  exchange. Check legitimate sharing and device buffers where applicable.
  - [x] Run two simultaneous sibling FPs on separate console devices with
    distinct keys and disjoint core areas; assemble probes with protected SLANG
    and verify host transfer from the unaffected sibling after every fault.
  - [x] Compile and execute hello/arithmetic/procedure programs and rebuild the
    unchanged transfer sources under protection; verify the rebuilt binaries'
    module layout and actual text/binary round trips.
  - [x] Exercise live loopback Telnet with independent local/remote FPs,
    protected descriptor/key inspection, remote removal/recreation, unaffected
    local exports and reconnect without losing the surviving remote FP's file.
    This covers one console endpoint, not arbitrary networking or all protocols.
  - [x] Compare `9. oldalgollib bases` with temporary and matching system-scoped
    bindings. Reproduce two messages in the former and none in the latter;
    validate both catalog layers and repeat protected compilation/transfers.
    Keep the derived profile private; the historical diagnostic definition
    remains unverified and general catalog/library compatibility is not implied.
- [x] Deliberately attempt cross-process access and privileged operations;
  verify the specified guest fault/recovery behavior and continued operation
  of an unaffected process. A successful boot is not sufficient evidence.
  Four assembled probes cover allowed cross-key reads followed by denied RS,
  JL, PL and KS. Every expected IRQ-0 snapshot is checked, the complete sibling
  data/key area remains intact, and the faulting FP is restarted each time.
  These are bounded checks, not exhaustive protection/security verification.

Gate: protected process operation and fault handling work together. This is the
first major integration milestone: run the full registered regression suite and
sanitizer checks here, including the unchanged compatibility profile. Keep the
new profile optional even if this gate succeeds.

- [x] Complete the full registered regression and sanitizer acceptance runs
  after the bounded functional milestone; audit the selected profile, evidence
  and compatibility baseline before declaring M3 complete.
  - [x] Complete the normal registered suite (29/29) and verify stable media,
    source fixtures and checkpoint identity.
  - [x] Rebuild the protected candidate with sanitizers and repeat protected
    Algol/system-binding, nested, isolation, lifecycle and live Telnet checks
    with leak detection enabled in a permitted environment.
  - [x] Complete the registered sanitizer suite, including targeted repetition
    of the two manual-related tests after correcting their wall-clock budgets.

M3 accepts only the identified optional profile and bounded assertions. The
normal suite passes 29/29. The sanitizer suite initially passes 27/29, with
both remaining tests timing out in the same manual helper; after correcting
finite wall budgets, both pass serial repetition without sanitizer findings.
No content assertion or guest instruction limit is relaxed. Stable fixture
hashes and checkpoint identity are verified, the architecture/specification
now reflect the implemented boundary, and the default remains unchanged.
See [instrumented evidence](TEST_FIXTURES.md#instrumented-protected-profile-checks).

### M4: Prepare selectable maintained-monitor profiles (approximately 90%)

- [x] Prepare separate ready-to-use compatibility and protected configurations
  and matching monitor/disk artifacts, retaining the verified M3 identities.
- [x] Make profile preparation repeatable and refuse unknown inputs or existing
  destinations. Do not overwrite standard media or personal working disks.
- [x] Verify startup and profile selection from a separate working directory;
  keep CLASSIC terminals in both profiles. V2 monitor integration, interrupt
  expanders and V2-only host/terminal requirements are not prerequisites.
- [x] Document protected `rcin`/`rcout`, the older system-scoped tofrom binding,
  known untested software paths, persistence choices and recovery.
- [x] Decide artifact/distribution inclusion separately, preserving the ability
  to select either profile without changing the other profile's files.

Gate: both maintained-monitor profiles are reproducibly prepared, independently
selectable and honestly documented. Do not claim general RC8000 compatibility
or protection support for every installed program.
Both are now included in binary-dist and ordinary build trees from verified
prebuilt source artifacts. The compatibility default is unchanged. Focused
archive checks pass both explicit configurations and the default alias, without
Python, media regeneration or PDF rebuilding; missing/changed protected inputs
are rejected without replacing an existing complete archive.

### M5: Acceptance, documentation and default decision (approximately 100%)

- [x] Review the complete M3 registered/sanitizer acceptance and monitor/media
  reproduction, then run focused final CPU, source, profile and archive checks.
  Subsequent profile packaging/documentation changes do not alter CPU runtime
  semantics; repeat broad suites only if such implementation changes require it.
- [x] Update compatibility limits, configuration reference, operating procedures,
  packaging decisions and fixture assertions. Record what remains unimplemented;
  do not claim guest protection as a host-security sandbox.
- [x] Retain an explicit way to run the stable compatibility profile. Decide
  separately whether to promote a new profile, based on the evidence and an
  explicit default-change decision; acceptance does not require promotion.
- [x] Provide separate ready-to-use compatibility and protected configurations
  with their matching artifacts and documented selection. Verify both profiles;
  neither is removed merely because the protected profile becomes usable.
- [x] Rebuild/review the versioned manual PDF only when explicitly requested.
- [x] Merge only accepted work and verify the stable checkpoint remains available.

Final pre-merge checks pass the native protection target, manual source/rebuild
guards and normal Release binary-package checks. The explicitly rebuilt PDF
has 82 pages, embedded Libertinus/DejaVu fonts, resolved references and no
overfull/underfull boxes. Changed pages, contents and index are visually checked;
the archive is rebuilt and tested with that PDF. No redundant full sanitizer
rerun is claimed: broad acceptance is the recorded M3 run plus its successful
targeted repetitions and protected guest checks.

The accepted implementation/artifacts commit `8c28198` is fast-forwarded into
`main`. The fully merged `feature/protection-monitor` branch is deleted; its
commits remain in main's history. `baseline-before-protection` still resolves to
`a5de1763382bd7611507e35a398ac7e27f026c25`. The original compatibility monitor,
disk and configuration are unchanged, and no remote publication is performed.

## Deliberate exclusions

This work does not include the binary-only V2 monitor, new archived software
installation, a general RC8000
machine profile, Unicode conversion, additional host-platform validation or
publication-rights decisions. Normal builds remain independent of developer
tests, Python and TeX. Use small relevant checks within each milestone; reserve
broad testing for M3 and M5 as described above.
