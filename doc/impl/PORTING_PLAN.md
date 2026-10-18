# Native Emulator Roadmap

## Objective

Preserve operation of the supplied RC system on modern hosts: monitor startup,
disk-resident software, guest files and useful operator control. The native C17
implementation is maintained as a system emulator, not as a source-level port
of its original basis. Development and testing use Linux/x86-64; other host
platforms have not been validated.

[Progress](PROGRESS.md) records implemented capabilities and verification.
[Architecture](ARCHITECTURE.md) specifies current behavior.
This roadmap contains design constraints and remaining work, not a build log.
It is the consolidated continuation plan after the compatibility MIPS/TS
demonstration. Detailed historical experiments stay in their specialized
documents; their earlier failing controls are not automatically new blockers.

## Design constraints

- Use portable C and fixed-width types for 12-bit bytes, 24-bit words and
  architectural addresses. No host assembly or JIT is required.
- Keep CPU/core and device models independent of terminal shortcuts, Readline,
  Telnet framing and host paths.
- Separate deterministic instruction/device stepping from interactive wall-clock
  pacing. Approximate original-speed pacing is not cycle-accurate hardware timing.
- Preserve the default monitor, CLASSIC terminal protocol, Ex21/low-precision
  behavior and existing guest workflows when adding optional capabilities.
- IX is a separately selectable compatibility instruction, enabled by default.
  Its historical hardware-model assignment is unresolved; it is not a complete
  RC8000 profile.
- Never experiment on the bundled base media. Use volatile overlays or verified
  complete working copies; promote a changed artifact only after reproduction
  and bounded functional checks.
- Confine host-file paths, reject unintended overwrites and validate core/media
  ranges before I/O. Guest resource claims do not currently provide protection.
- Keep the ordinary build and binary distribution free of Python and TeX
  requirements. Project-specific user utilities belong in C; optional developer
  orchestration may use Python or CMake.
- Keep normal builds free of tests. Enable developer tests explicitly.
  Rebuild the versioned manual PDF only through its explicit target.

## Current compatibility baseline

The default profile uses MONITOR, RC433X and the native UNIT0. The optional
STDMON/DFC403/RC4819 profile has bounded boot and guest-workflow coverage.
The separately selectable MONITOR.protected/UNIT0.protected profile has
bounded protection and guest-workflow coverage. The compatibility-based
UNIT0.mipsts/UNIT1.mipsts profile automatically starts TEM/PRIMO/SOS and
passes two-boot Telnet login/logout and retained-text checks. Manual
manufacturing from private native inputs is separately documented and tested.
These successes do not certify every installed program or peripheral.
Monitor source reproduction, host-file image generation and diagnostic fixtures
are developer workflows, not runtime prerequisites.

Test success establishes only the operations covered by those tests. Consult
[Compatibility and Limitations](../COMPATIBILITY.md) for the product boundary
and [Test Fixtures](TEST_FIXTURES.md) for the evidence behind individual claims.

## Ordered continuation plan

Unchecked items below are future work, not authorization to execute all of
it immediately. P1 is complete for the agreed printer scope; its extra checks
are deferred. The P3 bounded protected runtime assessment is consolidated.
MIPS/TS continues to use the unprotected compatibility profile; protected
candidate promotion/integration is explicitly deferred. The next active work
is the independent P2 operating recipes and documentation upkeep. P4--P6
are independent optional or release-time work. No optional feature must be
enabled by default merely to make its test pass.

### P0. Finish the documentation handoff

- [x] Describe operation of the finished MIPS/TS environment in its own User
  Manual chapter, including contexts, quotas, persistence and known limits.
- [x] Keep manufacturing outside the User Manual: [MIPS-TS.md](../MIPS-TS.md)
  gives the complete manual guest/host sequence and safe copy boundaries.
- [x] Verify that manual recipe independently of the generator, on new private
  inputs, with exact catalog/startup checks and two-boot Telnet acceptance.
- [x] Review the intended documentation and explicitly requested rebuilt PDF;
  the old PDF-only modification is superseded by the reviewed current build.
  Commit only the reviewed source/documentation/test artifacts when requested.
- [x] Refresh the tracked manual on request and review the new chapter's
  pagination, tables, index and resolved cross-references. Future PDF builds
  remain explicit, not part of ordinary builds or documentation edits.
- [ ] At that documentation/release milestone, rebuild the archive from the
  reviewed PDF and check extracted contents. Packaging never refreshes the PDF.

Acceptance: manufacturing remains standalone, runtime instructions remain
usable without Python, and no stale PDF is described as containing new text.
There is no PDF or broad-suite prerequisite for recording this plan.

### P1. Implement an optional file-backed RC610 line printer

Purpose: give the guest a real emulated printer endpoint and let PRIMO send
output to it. The existing console-backed PRIMO transfer is a useful control,
not completion of this milestone. The first host backend is a file, not a
physical printer, CUPS integration or a PDF renderer.

#### P1a. Establish the device contract before coding

- [x] Derive sense/control/output operations, modifier interpretation, status
  bits, character packing, line/page controls, ready/busy handling and interrupt
  acknowledgement from the driver and available hardware documentation.
  Start with monsrc/montxt4's RC610 driver, montxt3's kind-14 dispatch,
  montxt9's type-18 description and mon8mops's printer entries. Record exact
  evidence and unresolved behavior; do not guess another model's bit layout.
- [x] Inspect the actual supplied monitor's external printer descriptors and
  configured driver, not only source options. Confirm guest name, device number
  and interrupt level; identify whether any separately rebuilt monitor would
  be necessary. Do not silently patch the baseline monitor to add a device.
- [x] Resolve number/interrupt conflicts with optional RC851 consoles, tapes
  and other devices. Source MOPS printer numbers are evidence, not permission
  to replace an existing configured device. Keep one explicit test config.
- [x] Define a minimal supported printer contract and independent vectors;
  list unsupported hardware functions and timing simplifications explicitly.

The contract checkpoint is in [RC610](RC610.md), using RCSL 55-D54
(archive 30007708) and live peripheral-table checks. The pre-printer baseline
had no printer. All maintained monitors now include printer on device 7,
interrupt 10, preserving terminals 5/6 and the other observed mappings.
They boot and agree with the verified mapping. The native module passes focused
checks and a repeated Guest-s transport smoke test. Bounded P1a is about 100%.

#### P1b. Implement the native model and host output boundary

- [x] Implement the agreed device state machine in C through the existing
  device I/O, advance and interrupt-poll interfaces. Keep host file handling
  separate from guest controller semantics. No assembly or BIOS interface.
- [x] Add an explicit configuration type (RC610, finalized by
  P1a), path handling, startup validation, device lifecycle/reset and useful
  show/status counters. Reject duplicate/conflicting definitions safely.
- [x] Specify output-file creation and job/session boundaries before exposing
  them: no implicit overwrite, no accidental truncation on reset, no invented
  per-job splitting without a real guest job boundary. Use ordinary files in
  the first backend; no shell command execution or automatic host printing.
- [x] Define supported character encoding and handling of CR/LF, form feed,
  tabs, unsupported control codes and page boundaries. Keep exact byte-level
  behavior testable; do not claim Unicode or historically exact typography.
- [x] Make disconnected/offline, busy, invalid operation and host write-error
  behavior explicit. Bound queues/resources and propagate errors through the
  guest status/completion path rather than silently dropping printed text.
- [x] Integrate close/flush and reset/error cleanup. Do not promise crash-safe
  spooling or mechanical timing; decide deterministic timing deliberately.

#### P1c. Validate printer I/O and actual PRIMO service

The agreed printer block is complete. Remaining unchecked P1 items below are
explicitly deferred extensions, not prerequisites for the accepted printer
support. No further printer implementation/testing is currently scheduled.

- [x] Add focused native tests for operations, status transitions, interrupts,
  character packing, controls/page breaks, reset and output errors, using
  independent expected values rather than the model as its own oracle.
- [ ] Exercise guest printer reservation, output and release on disposable
  media. Require exact file contents and normal guest completion; a created
  host file or a PRIMO ready reply alone is insufficient.
- [ ] Independently verify Guest-s print on a bounded known memory range, its
  formatted output and release. Printing must also work outside MIPS/TS.
- [x] Ship printer-capable compatibility/protected/DFC403 monitors, runtime
  integration and automatic append-mode configs. Keep historical inputs intact.
- [x] Print an installed Algol source through a guest program and compare the
  entire result with an independent guest export; document the accepted recipe.
- [x] Verify that same source-print example with protected libraries and actual
  keyed task execution, rcin/rcout transfers, exact output and clean FP exit.
- [x] Configure PRIMO with the actual external printer, not the console-backed
  substitute; sign up, transfer known text, wait for completed transport and
  sign off. Check repeated sequential jobs, service health
  and file lifetime after orderly simulator shutdown.
- [x] Check two outstanding independent PRIMO transports, exact FIFO output,
  normal completions, client cleanup and a subsequent same-instance job. Use
  one blocking wait and one explicit status-polling fallback after reply 3.
- [x] Assess the second blocking-wait refusal against installed trimming and a
  private increased buffer claim. waitops=1 and unchanged refusal with buf 11
  strongly support a generated waiter limit; preserve the existing binary.
- [ ] Test queue/resource exhaustion and exhaustive reclamation; two transports
  are only bounded proof. More blocking waiters need source-backed PRIMO
  regeneration, not a host configuration workaround.
- [x] Test a representative error/offline path and subsequent recovery without
  destroying another job's output. Check ordinary console/disk workflows with
  the installed printer offline and with healthy output attached.
  Accepted scope: a pre-job detached endpoint, waiting notice, append reattach
  and start, healthy subsystem listings/TEM login and byte-exact restart output.
- [ ] Assess partial host-write/disk-full failures and mid-job paper recovery
  separately; the disconnected-endpoint check does not establish these cases.
- [x] Document the device configuration and PRIMO printing recipe. Update
  compatibility limits, manufacturing instructions only if their actual
  inputs change, and the package manifest if new required runtime files arise.

Acceptance: an opted-in RC610 receives a real guest/PRIMO print request and
produces the expected host bytes with correct guest completion and cleanup;
all existing profiles remain usable without it. This closes only the tested
printer/PRIMO path, not every optional PRIMO device. Integrate in a separate
feature branch from a committed stable checkpoint, with no default-media edits.

### P2. Extend practical MIPS/TS operating coverage

- [x] Verify a complete remote file lifecycle: prepare compact input within
  quota, copy/retain it in the actual demo account, logout/login, full restart,
  byte-exact export and deliberate cleanup. Administrative 0/0-base retention
  is already verified and is not a substitute for this user-level recipe.
  The shipped unprotected profile now passes four normal Telnet sessions over
  two automatic boots: administrative compact seed, actual remote copy/retain,
  later login/restart readback, exact independent exports and deliberate remote
  cleanup plus seed removal. The User Manual has the bounded accepted recipe.
- [x] Turn the existing fixed batch fixtures into an exact, repeatable manual
  jobfile exercise, including safe preparation, visibility, password dialogue,
  output, normal removal and cleanup. Verify inline-password batch syntax
  separately before calling it an accepted recipe.
  The shipped unprotected profile now passes both local-console password forms
  with the compact message/finis jobfile, exact message and normal removal,
  byte-preserved jobfile export, explicit cleanup and later interactive login.
- [x] Define and test a larger optional account for administratively imported
  input and remote compilation, with bounded memory/disk claims, two boots
  and unchanged demo records. See the
  [optional-account checkpoint](MIPS_TS_PLAN.md#optional-larger-account).
- [x] Assess direct host transfers from SOS separately: the optional account
  receives reservation result 2 before explicit SOS `include 63`, then passes
  byte-exact import/export, compilation and two-boot readback after inclusion.
  Repeat inclusion after enrollment on each boot; no monitor rights are bypassed.
- [x] Add a second account/terminal with explicit catalog entries and verify
  independent simultaneous sessions, resource limits, scope visibility and
  logout. Private device-4/5 sessions pass distinct catalog bases, shared
  project input, independent temporary/user files, quota refusal and relogin.
  See [simultaneous SOS sessions](MIPS_TS_PLAN.md#simultaneous-sos-sessions).
  Actual multiterminal application attachment remains a separate investigation.
- [x] Verify TCP disconnect/reconnect of an idle FP in the shipped unprotected
  profile: two cycles preserve the same job and temporary/retained text, with
  independent exact exports, normal logout/relogin and unchanged base images.
  Document direct resumption and the lack of a fresh authentication boundary.
- [x] Verify fully echoed partial FP lines across TCP reconnect: complete a
  known prefix and replace a five-character suffix with Ctrl-H. Require exact
  command output, preserved files and normal cleanup; document unknown-input limits.
- [x] Finish supported-operation documentation: prerequisites, contexts, expected
  replies, safe cleanup, backup, diagnostic preservation and explicit unsupported
  cases. See [documentation acceptance](MIPS_TS_PLAN.md#user-documentation-follow-up).
- [ ] Investigate unacknowledged input, interrupted commands/output and actual terminal
  faults separately; do not extrapolate the bounded TCP recovery to these cases.
  Do not advertise untested stop/start/break/kill sequences or invisible passwords.
- [ ] After individual recipes pass, consider bounded longer-running workload
  and repeated-job checks. Broader account administration, full-screen editors
  and all TEM controls remain opt-in investigations, not startup requirements.

Acceptance: each new documented recipe has its own positive/negative evidence
on fresh private media. Update the operating chapter only with proven steps;
keep manufacturing instructions in [MIPS-TS.md](../MIPS-TS.md).

### P3. Assess protected MIPS/TS separately

The maintained protected monitor is already implemented. This work concerns
the installed MIPS/TS subsystem binaries and their interactions under it,
not reimplementing the completed CPU protection milestone.

- [ ] Inventory subsystem instructions, monitor calls, shared data and privilege
  assumptions. Identify available matching source before planning any rebuild.
- [x] Test bounded TEM, PRIMO and SOS startup with the source-built protected
  monitor on independent media, preserving fault PC/instruction, register/key
  state, process identity and exact failed operation.
  TEM and PRIMO startup checkpoints now pass, plus PRIMO operator status/error
  recovery. Saved live descriptors and core/key evidence are retained privately.
  SOS now starts independently with an imported empty guest-generated catalog,
  private disc1 startup and verified swap descriptor. Combined TEM/PRIMO/SOS
  now passes a bounded test/test Telnet login, FP message/lookup and normal
  logout with actual protection enabled. Active process/core/key evidence is
  retained. Two sequential protected local FP-to-PRIMO-to-RC610 jobs also pass
  exact output and completion checks. The actual remote SOS child now also
  creates/retains a user file; fresh login on a second full emulator boot reads
  it unchanged through private persistent overlays. Independent rcout exports
  and base hashes pass. A bounded forbidden monitor store now verifies exact
  task code/PR/keys/operands/fault IC, unchanged target, SOS child removal and
  healthy subsystems/fresh login. Broader isolation, remote printer submission
  and profile integration remain separate gates; restart does not establish crash recovery.
- [x] Classify observed boundaries as preparation, resource and runtime behavior
  and consolidate the current assessment in
  [MIPS/TS coverage and decision](MIPS_TS_PLAN.md#assessment-decision-and-coverage).
- [x] Diagnose the exact protected upsoscat instruction fault before planning
  any guest rebuild. Distinguish configuration, resource, guest-software and
  CPU/device behavior using evidence. Do not weaken protection or add a hidden
  compatibility bypass merely to boot a newer subsystem binary.
  Protected upsoscat breaks in tofrom1 during preparation; an explicit separate
  compatibility factory supplies verified catalog data for the protected SOS
  startup check. A separate instrumented run proves task pl w3 34 (730022),
  IRQ 0, saved-next IC, PR bf/Key 1 and embedded tofrom1/tofrom2. FP recovery
  and exact preserved input pass; no updater fix or catalog build is claimed.
- [x] Prepare and verify a separate explicitly experimental protected MIPS/TS candidate.
  Existing binaries pass bounded runtime checks; no rebuild is required by
  those results. Keep the explicit compatibility catalog factory unless a
  source-based protected updater is independently established. Derive clean
  bases and autostart/config/overlay integration separately. The private candidate
  now passes two automatic boots, login/retained-file sessions and four local
  PRIMO jobs with exact immediate append output on separate runtime copies.
  Keep the working compatibility release environment independently selectable.
- [ ] Deferred: review candidate promotion and integrate operating documentation plus
  build/distribution assets explicitly. Generated diagnostic overlays/user files
  must never replace clean bases. Existing source profiles remain unchanged.

Current decision: retain unprotected MIPS/TS for normal operation, build and
distribution. Preserve the protected candidate tooling/evidence as developer
work only. Successful experiments do not authorize promotion or a change of
defaults. The ordinary standalone protected profile is unaffected.
- [ ] Investigate protected cmp only if matching comparator/runtime source is
  found; retain the verified rcout plus host cmp/diff workaround meanwhile.

The binary-only V2 monitor remains excluded. Experimental V2 terminal tables
do not establish V2 monitor support and are not a prerequisite for printing.
Compatibility, protected and MIPS/TS configurations must remain selectable
independently, with matching images and separate overlays.

### P4. Optional media, software and historical coverage

- [ ] Identify archived installable software with the required monitor/device
  dependencies and provenance; install only on new derived images.
- [ ] Run an actual archived read-only RC749 software-tape workload and document
  record/mark/installation behavior. Tape writing is a separate decision, not
  part of the current read-only contract.
- [ ] Extend guest filesystem/catalog documentation where it enables a useful
  listing, extraction or administration procedure. No host-side mutation tool
  without a separate specification and recovery tests.
- [ ] Validate inventory-only tools, optional subsystems and remaining editor/
  compiler/numerical recipes, one bounded workflow at a time.
- [ ] Define non-ASCII host/guest conversion and Danish character mapping before
  changing text behavior. Preserve the existing ASCII/byte-transfer paths.
- [ ] Locate an independently verifiable original monitor source release if
  authenticity beyond the maintained source/binary pair is required.
- [ ] Reconsider a host-side SLANG assembler only if the working guest assembler
  becomes insufficient. It is not needed for the present build or release.

These are opportunities, not requirements to keep adding historical scope
before shipping the already working profiles. Physical timing fidelity, full
RC8000 emulation and further peripherals need separately agreed scope.

### P5. Release validation and publication, when resumed

- [ ] Explicitly agree the intended release scope and version; retain author
  credits, supported-platform statements and known-limit documentation.
- [ ] Resolve inherited-material and linked-dependency redistribution scope
  under LICENSE before actual publication. Creating an archive is not clearance.
- [ ] Validate hosted CI on the intended revision and run one coherent normal
  regression block plus representative compiled guest workload. Use sanitizer
  coverage at a significant native-code milestone, not after each prose edit.
- [ ] With explicit approval, build/review the final PDF, regenerate the binary
  archive and verify extraction, flat profile files, hashes, runtime-library
  needs, startup, Telnet and persistence on a compatible destination host.
- [ ] Confirm ordinary builds still neither build/run tests nor require Python
  or TeX. Exclude runtime overlays, logs, preparation/verification files and
  developer tools from the binary archive.
- [ ] Publish/tag only on explicit instruction. Package-manager installation
  work remains deferred; copying the runtime files is sufficient.

P1--P4 additions are not implicit release blockers. A release may document
them as deferred; unresolved publication rights are a separate publication
decision. No remote write or release operation is authorized by this plan.

### P6. Optional host portability

A 32-bit i386 build and bounded monitor startup have succeeded, but i386 is
not a validated release target. Linux/x86-64 remains the supported test baseline.

- [ ] If i386 support is pursued, run CPU/FP/device/CLI/guest regressions for
  all claimed profiles; review long-based offsets and large-file support.
- [ ] Derive package architecture from the executable ABI rather than only the
  host processor string, and document 32-bit library/size requirements.
- [ ] Let interested contributors validate other POSIX hosts (including macOS)
  with their own build/runtime evidence. Do not turn untested portability into
  a support claim or expand this block into a new platform port by default.

## Progress interpretation

Existing compatibility MIPS/TS coverage is approximately 90%. Source documentation
for the supported operating scope and the fixed manual manufacturing recipe is
100% complete; this does not promise instructions for unverified future features.
The last requested PDF is a reviewed snapshot, not automatically rebuilt after
source completion. Future PDF updates remain explicit.
These estimates refer to their existing bounded scopes, not to every optional
item above. The agreed printer block is complete (100% of its bounded scope),
including maintained monitor/runtime/distribution integration, immediate append
output, real PRIMO spooling, detached-output recovery and bounded FIFO checks.
Broader fault/stress work remains deferred, not an unfinished original gate.
The protected MIPS/TS assessment is approximately 95%: bounded runtime checks,
consolidation and precise updater diagnosis pass. A separately accepted release-
profile workflow remains open. These percentages are distinct
from documentation coverage and do not imply exhaustive hardware/software support.

## Change acceptance

Use the smallest relevant verification block during development. Add independent
vectors for CPU/device semantics and integration checks for guest workflows.
Check fixture hashes, narrow unsupported claims, and document destructive
operations. Reserve full integration and sanitizer suites for significant
milestones. Update this roadmap and the current checklist when scope or
supported behavior changes; do not append conversational or machine-local logs.
