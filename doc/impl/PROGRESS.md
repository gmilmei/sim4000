# Implementation Status

This is the current capability checklist, not a chronological work log.
Checked items have implementation and bounded verification. Unchecked items
remain incomplete, deferred or outside the validated release scope.

## Next work

Supported-operation MIPS/TS source documentation is complete (100%): startup,
contexts, files, batch, printer, status, preservation/backup and explicit limits
have end-to-end instructions. Known echoed partial input now survives TCP
reconnect with exact continuation or five-character Ctrl-H correction, on
repeated fresh private pairs with preserved file bytes. Unknown input and
in-flight failures remain unsupported, not silently documented as recoverable.
The ready-built PDF is explicitly refreshed to include the completed source
instructions; later edits still do not rebuild it automatically. See the
[documentation acceptance](MIPS_TS_PLAN.md#user-documentation-follow-up).

The optional larger unprotected SOS account now passes a bounded staged-input
Algol workload over two emulator boots, with the original demo record and
base images unchanged. Direct SOS host transfers now also pass after the
documented explicit `include 63` command for enrolled jobs; before inclusion,
reservation still fails with result 2, including after restart. See the
[account checkpoint](MIPS_TS_PLAN.md#optional-larger-account). It is private
developer evidence, not an added distribution profile.

Two simultaneous unprotected SOS sessions now pass on private device-4/5
loopback endpoints. The fixture adds distinct account bases and two SOS child
slots, verifies temporary/user visibility and a common project input, tests
different quotas, and exercises independent logout/relogin. Swapped jobs may
reuse the same core slot; this is not a protected-memory isolation claim.
The shipped single-account profile is unchanged. See the
[parallel-session checkpoint](MIPS_TS_PLAN.md#simultaneous-sos-sessions).

The shipped unprotected single-account profile also preserves an idle FP
through two TCP disconnect/reconnect cycles without guest logout. Direct
resumption needs no fresh SOS login; temporary and retained text survive,
verified by independent exact exports. This is a local transport reconnect,
not authenticated reattachment or recovery from interrupted I/O, hard terminal
errors, emulator exit or crashes. See the
[idle reconnect checkpoint](MIPS_TS_PLAN.md#idle-fp-tcp-reconnection).

The ordered continuation plan is [Roadmap](PORTING_PLAN.md). Its priorities
now move from the completed RC610/PRIMO printer block (P1) and consolidated
bounded protected-runtime assessment (P3) to documentation upkeep and MIPS/TS
operating recipes (P2). MIPS/TS remains unprotected for normal operation;
protected candidate promotion/integration is explicitly deferred. Software/media, publication and
host-portability work (P4--P6) is optional or explicitly deferred.

- [x] Record the consolidated plan, dependencies, acceptance gates and scope.
- [x] P0: Review the documentation and explicitly requested rebuilt PDF, including
  MIPS/TS pagination, tables, references and index. Commit the reviewed artifacts
  as requested; later PDF rebuilds remain explicit.
- [x] P1a: Establish hardware command/status semantics from RCSL 55-D54 and
  confirm the compiled baseline lacks a printer; preserve independent vectors.
- [x] P1a: Build/boot a private source candidate with kind-14 printer on device 7,
  interrupt 10, verified by montest and packed live descriptor/vector checks.
  Devices 5/6 and other existing assignments are unchanged. Candidates were
  subsequently validated and promoted for the permanent printer profiles.
- [x] P1a/P1b: Specify raw print-operation stream, encoding, hardware clear,
  error and simplified timing policies. Contract: [RC610](RC610.md).
- [x] P1b: Implement the native C device/file API and focused tests, including
  per-print flushing and immediate visibility from a separate file reader.
- [x] P1c: Establish a bounded repeated Guest-s print transport smoke check
  with the private source-built monitor and volatile disk writes.
- [x] Use maintained monsrc/sim4000.mops for all future monitor builds; keep
  hjmops* unchanged as historical references and remove --variant selection.
  Initial normalized build-command equivalence is checked without media changes.
- [x] Move the e22 correction into sim4000-protected.mops and the historical
  guest editor. Reuse the shared build at FP depth two, preserve original input
  segments and validate the exported edit/restoration instead of patching in
  Python. Protected and compatibility monitor bytes remain unchanged.
  The focused refactor is complete (approximately 100%): eight pure guards,
  private regeneration/boot checks, protected printing, live editor/assembler
  negative controls and before/after historical/source/media hashes pass.
- [x] P1c: Build a private protected printer monitor from sim4000.mops and
  repeat Guest-s printing with CPU protection enabled and observed keyed task
  execution. Normal compatibility mode still passes; a mismatched monitor/
  protection-on run fails rather than falling back to unprotected operation.
- [x] P1b: Implement C RC610 I/O and a safe file backend with explicit
  output lifetime, errors, cleanup and configuration/status integration.
- [x] Include the printer in all maintained monitors/configs and the normal
  build/binary distribution. Automatic append output preserves existing text
  across restart; explicit exclusive output remains available.
- [x] Check the packaged profiles and the new monitor's two-boot MIPS/TS
  login/logout/persistence path; preserve all base disk hashes. Refresh the
  native fixed-image regression checkpoints for the generated monitor layout.
- [x] Print cpuregtest through a guest Algol physical-device zone, with an
  independent whole-file export comparison and a documented source example.
- [x] Repeat the unchanged source example under the protected monitor/libraries:
  rcin import, normal compile/run, 630 prints/17309 exact bytes versus rcout,
  keyed task execution during printing, normal finis and subsequent s list.
- [x] P1c: Verify actual PRIMO-to-RC610 signup, four byte-exact completed jobs
  across two starts, immediate host visibility, signoff and append lifetime.
  TEM/SOS login/logout and unchanged base-media hashes pass on both boots.
  Ship the tested administrative ALGOL example and document its command contexts.
- [x] P1c: Recover a transport submitted with output detached: require the
  specific printer-end-document notice, waiting state, no premature completion,
  unchanged output and healthy subsystems; append reattachment/start then
  produces exactly one payload and normal completion, surviving restart.
- [x] P1c: Define two outstanding independent transports, verify distinct IDs,
  exact FIFO output, two normal completions, client cleanup and a subsequent
  same-instance transport. Record the second blocking wait's reply 3 and the
  explicit bounded action-4 polling fallback; do not claim two successful waits.
- [x] Diagnose the bounded waiter refusal: exported primotrim specifies
  waitops=1, distinct from waittrans=300. A private recreation with buf 11 still
  refuses the second wait, while printing/polling/followup/TEM checks pass.
  Preserve the generated-binary limit; no source, binary or default-claim patch.
- [ ] P1c: Verify broader host-error recovery and queue resource exhaustion/
  exhaustive reclamation. Deferred beyond the agreed completed printer scope.
- [ ] P2: Add accepted remote-file/batch/account/concurrency/recovery recipes.
- [x] P2 remote-file recipe: use the shipped unprotected MIPS/TS profile, compact
  seed preparation in explicit 0/0 intervals and actual test-account retention.
  Four normal Telnet sessions across two automatic boots verify later readback,
  exact host exports and targeted cleanup. User Manual command blocks match
  the checked dialogue; source-only TeX checks pass. No PDF refresh is implied.
- [x] P2 batch recipe: prepare a compact retained jobfile with explicit demo-
  matching intervals, submit locally with a password question and independently
  with inline pass test, require actual message output/normal removal, then
  verify later interactive login, exact jobfile export and deliberate cleanup.
  Eight manual blocks match the dialogue; the focused live check and 48 pure
  MIPS/TS guards pass. Other account/concurrency/recovery recipes remain open.
- [x] Review MIPS/TS reader navigation after adding the file/batch exercises:
  add task/context/page-reference entry points, staged file headings and targeted
  index entries; place troubleshooting after the operating recipes and clarify
  non-destructive checks for expected errors, quota/retention/export/job issues.
  Literal command-block guards and TeX source checks pass without a PDF rebuild.
- [x] Explicitly requested PDF refresh after the file/batch/navigation updates:
  104 pages, with MIPS/TS on printed pages 53--65. Visually review all chapter
  pages, contents and index; compact the workflow table onto one page and remove
  the nearly empty chapter-ending spill page. No overfull boxes, unresolved
  references or missing-glyph warnings remain. No runtime/profile changes.
- [ ] P3: Assess protected MIPS/TS without weakening protection or using V2.
- [x] P3 initial checkpoint: independently start unchanged TEM/PRIMO with the
  protected monitor and matching disk, preserve live descriptor/core/key state,
  and check PRIMO display/unknown-command/display. Stop counts remain zero;
  PK=1 code and Key-0 monitor descriptors are independently observed. This is
  startup/operator evidence only, not protected MIPS/TS user-service acceptance.

- [x] P3 SOS startup checkpoint: initialize a new private second disk under
  protection, import a verified empty guest-generated catalog, preserve original
  startsos and use a private disc1 startup. SOS 5.0 waits with stop count 0,
  PR=8f/PK=3 in matching keyed memory; its 1722-segment swap area is on disc1.
  Protected upsoscat itself breaks in tofrom1, so catalog manufacture is an
  explicit separate compatibility stage, not a hidden protection fallback.

- [x] P3 combined terminal checkpoint: import a separately manufactured,
  byte-verified demo catalog; run TEM/PRIMO/SOS together with protection enabled.
  Reject the wrong password, enroll test/test, execute message and lookup fp,
  log out with finis and verify the child disappears. Preserve active packed
  core and actual subsystem keys, including the stopped-by-parent child state.

- [x] P3 protected PRIMO spool checkpoint: compile the unchanged example with
  guest Algol, submit two sequential local FP jobs to real RC610/dev7, check
  completion and exact 4538-byte output visible before shutdown. Signup/signoff
  and subsequent protected TEM/SOS login/commands/logout pass.

- [x] P3 protected file/restart checkpoint: the actual SOS test/test child copies
  a shared seed to keepsake and retains it as user. A second emulator process
  reuses the independent private disk overlays, mounts the existing disc1 and
  starts the subsystems manually. Fresh login, descriptor/scope and exact text
  pass without recreating the file; independent rcout exports match both boots.
  CPU protection/IX, sidecar identities and unchanged base hashes are checked.

- [x] P3 bounded fault/isolation checkpoint: execute a deliberate monitor-memory
  store from the remote SOS child. Live task PR=e7/code Key 3 and target Key 0,
  operands, exact loaded RS code and reported next IC are checked. The target
  word is unchanged; SOS removes only the failed child. Fresh login/FP commands/
  logout and PRIMO operator recovery pass. No CPU/protection semantics change.

- [x] Consolidate protected runtime coverage, exact preparation limitations and
  evidence boundaries. Separate completed bounded tests from optional extended
  coverage and future clean-profile/autostart/distribution integration.

- [x] Localize protected upsoscat independently: task pl w3 34/730022, IRQ 0,
  next IC=PC+2, PR bf/Key 1, embedded tofrom1/tofrom2. FP reinitialization
  preserves the maxp-4 input. Negative controls reject wrong context/binary/
  duplicate observations. No instruction, library, catalog or CPU fix is applied.

- [x] Prepare an exclusive private protected MIPS/TS candidate with flat media/
  config names, fixed-date depth-two autostart and explicit compatibility-only
  catalog manufacture. Freeze clean bases before running any verification.
  Two automatic boots on copies pass remote retained-file/login/FP sessions
  and four local PRIMO jobs (9076 exact immediately visible append bytes).
  Source/candidate/runtime base hashes remain unchanged; sidecars stay separate.

Protected assessment is approximately 95% complete. Private candidate preparation
and its two-boot acceptance are complete (100% of that block); source promotion,
operating documentation and build/distribution integration are explicitly deferred.
Normal MIPS/TS continues with protection off; standalone protection is unchanged. Updater
incompatibility is diagnosed, not fixed.
Remote SOS-client printing and broader isolation/fault coverage are extensions,
not claims of current acceptance. Private persistent
diagnostic overlays are not a promoted protected image; compatibility remains
the supported release profile. No crash-recovery or autostart claim is made.

The agreed printer block is complete (100% of current scope). Native checks,
Guest-s prints in compatibility/protected/DFC403, and source printing through
guest Algol pass. Automatic output, visibility, reset and restart append are
tested through the real CLI in all three direct profiles. Full guest numeric/
reservation checks, unrestricted PRIMO queues and partial-write/disk-full
recovery are deferred extensions. Protected Algol source printing passes.
Sequential real PRIMO spooling
and pre-job detached-output recovery are checked. Two outstanding transports
work with one blocking waiter and one polling client; a second concurrent
blocking wait receives missing resources in this fixture.
All maintained monitor artifacts and configs include the printer; shipped disk
images and historical files are unchanged. Checkpoint documentation is about
95%; the PDF refresh remains explicit. Existing MIPS/TS function
coverage is about 90%, documentation about 95%, and the fixed manual
manufacturing recipe about 100%; optional new scope is not already complete.

## Supported baseline

- [x] Self-contained C17 emulator with fixed-width architectural
  state, explicit storage/address bounds and deterministic execution.
- [x] Linux/x86-64 development and test baseline documented; other POSIX hosts,
  including macOS, remain unvalidated rather than promised release targets.
- [x] Integer/control instructions and interrupt paths used by the supplied
  system; floating-point operations with independent oracle and guest checks.
- [x] Separately configurable IX instruction, enabled by default for the bundled
  software; historical hardware attribution remains unresolved.
- [x] Default MONITOR/RC433X and optional STDMON/DFC403/RC4819 profiles.
- [x] RC2000 reader, RC150 punch, console/RC851 translation, clock, host-file
  service and read-only RC749 tagged-tape support.
- [x] Volatile/persistent RC433X overlays, explicit writable working copies and
  runtime media controls, with malformed-media and non-overwrite checks.
- [x] SIMH-like operator CLI, Readline editing/completion, aliases, bounded
  output, execution control, core inspection, breakpoints/watchpoints and traces.
- [x] Configurable local escape/date shortcuts, DEL mapping, input buffering
  and recovery, date translation, pacing and orderly terminal restoration.
- [x] Data-only Telnet endpoints, configurable attention, separate guest
  processes, connection/rejection/reconnection and ANSI transport handling.
- [x] Bounded shared-console lifecycle for idle FPs on MONITOR/RC433X.
  Long shared-console reports, arbitrary tools and alternate-profile concurrency
  remain outside this check.
- [x] Experimental per-terminal V2 attribute/conversion subset, opt-in only.
  No end-to-end V2 monitor support is implied.

## Artifacts, build and documentation

The compatibility MIPS/TS demonstration is integrated into main; see
[MIPS/TS Plan](MIPS_TS_PLAN.md). Approximate compatibility milestone coverage: 90%.

- [x] Two complete compatibility-profile autostarts, TEM login/logout and exact
  retained-file readback pass with independent persistent overlays (M4).
- [x] Prepared clean UNIT0.mipsts/UNIT1.mipsts bases and commented configuration
  are included in ordinary builds and binary packaging without Python.
- [x] Flat MONITOR.protected/UNIT0.protected source and runtime layout, with
  consolidated per-directory file inventories and updated tools/tests.
- [x] Dedicated MIPS/TS User Manual chapter covers operation, credentials,
  persistence and limits. TeX source checked; PDF refresh remains on request.
- [x] Expanded MIPS/TS operating guidance: command contexts, go/run, jobfiles,
  account limits and import allocation, status, backups and error boundaries.
  The requested PDF review is complete; additional accepted file/batch recipes
  remain separate from the existing operating guidance.
- [x] Standalone doc/MIPS-TS.md manufacturing guide, separate from the User
  Manual, with all manual host/guest steps and document-driven acceptance on
  private copies: exact catalog/startup exports, two boots, Telnet login/logout
  and retained-text restart readback. Clean bases/source media are unchanged.
  Manufacturing coverage is about 100% for the fixed recipe; overall MIPS/TS
  documentation is about 95%; broader accepted operating recipes remain open.

- [x] Export startup/configuration material, subsystem binaries and descriptors
  without executing scripts; private copies and source media remain unchanged.
- [x] Repeat unchanged TEM/PRIMO creation through the real frontend with
  requested sizes, nonzero keys and zero stop counts, still active after a
  bounded advance. This is not terminal/spool service or SOS-login acceptance.
- [ ] Complete real peripheral/PRIMO service communication and separately
  assess protected MIPS/TS. Catalog creation, fixed-account login and bounded
  work are already verified; broader policies remain optional P2 work.
- [x] PRIMO display/unknown-command/display operator exchange completes with
  the expected distinct reply forms, followed by healthy component listings.
- [x] Guest upsoscat creates a byte-checked empty catalog. Administrative
  preparation plus a private 2000-segment/eight-entry startup reaches SOS 5.0
  started with active TEM/PRIMO/SOS. This is not an account or login check.
- [x] A fixed named-process catalog with size bounds and quoted dummy password
  passes exact packed-byte checks and repeated SOS startup. This does not yet
  establish complete account claims, terminal connection or password checking.
- [x] SOS local operator selection and bounded timestamped command exchanges
  work, with all three components still active afterward. A call with a missing
  account name also receives ready; this is explicitly not login acceptance.
- [x] Catalog diagnostics distinguish normal, rejected, incomplete and
  inconsistent completion; residual output after an abnormal end is never
  promoted. Named-process startup remains the successful control. Terminal
  grammar and the meaning of call's arguments remain unresolved.
- [x] A non-patching bounded debugger replay locates and verifies known updater
  native code before tracing it. The complete observed term order is name,
  quoted locid, quoted terminal key, then two numeric values.
- [x] A fixed account/terminal fixture completes normally, matches its packed
  1536-byte export exactly and starts SOS on two fresh private runs. All three
  services remain active. This is catalog/startup evidence, not an attached
  terminal or login: the operator start check still reports not connected.
- [x] Establish fixed user/process resources and terminal records, actual login,
  bounded jobs and TEM communication. Real printer/spool service remains P1;
  this does not imply every account or PRIMO device is accepted.
- [x] Integrated --mkdisc/operator mkdisc creation and overwrite safeguards.
  A private device-16 disk is labelled and mounted through all big/kitlabel/
  kiton devno.16, with 18225 free segments reported. Base media are unchanged.
- [x] A retained 207-segment numbered-text file on disc1 exports byte for byte
  before and after a full emulator restart/remount. Complete guest metadata is
  unchanged, including scope, bases and key. This exercises persistent overlay
  data beyond the starter's 180 sectors.
- [x] A private second-disk SOS startup keeps the original 2500-segment/200-entry
  claim, changing only its document to disc1. TEM/PRIMO/SOS remain active;
  administrative lookup confirms the 1722-segment temporary swpsos on disc1.
  This is resource allocation/startup evidence, not terminal login or job I/O.
- [x] Classify the plain batch-without-jobfile experiment as a bounded negative
  diagnostic, not an accepted unattended-job recipe. It can acknowledge
  enrollment while the child still waits for its parent. The separate jobfile
  path below completes normal jobs; reproducing the old negative control is
  not an unresolved requirement to establish basic batch operation.
- [x] The documented jobfile path completes a fixed finis-only batch job with
  the dummy test password. Exactly one enrollment and normal removal, no test
  child afterward and healthy TEM/PRIMO/SOS are required. This is a minimal
  lifecycle check, not useful computation or interactive login. The zero-quota
  copy failure is the negative control for the explicit resource checks below.
- [x] Fixed message and catalog-lookup batch workloads finish normally and
  require actual unique output. Read/copy and allocation remain failing
  diagnostics with the default zero-segment account. D662 confirms that this
  account has six disc entries but zero segments.
- [x] A private 30-segment temporary claim changes exactly the expected packed
  catalog word. Allocation and copy then finish normally; a temporary-copy
  readback checks matching character counts/checksums. This is not exact-byte
  export. Without persistent-scope quota, retention reports no resources.
- [x] A separate cumulative key1..3 quota enables user-scope retention after
  batch exit. An independent FP with matching bases exports the text byte for
  byte; binary exports confirm all logical bytes plus the end marker. Unused
  sector tails are excluded explicitly. One- and two-disk fixtures pass;
  the separate restart and interactive-session gates are verified above/below.
- [x] A separate loopback device-4 console completes authenticated interactive
  FP message/lookup and normal logout via go and TEM-mediated run. Wrong
  passwords are rejected, run requires the from tem header, and local-console
  listings verify child removal and healthy services. M3 is complete for the
  fixed compatibility fixture, not arbitrary applications or protection.

- [x] Immutable original disk plus maintained native host-utility image and
  reproducible developer regeneration.
- [x] Included LF/lowercase monitor sources reproduce MONITOR byte for byte.
  This is not proof of an unmodified vendor source release.
- [x] Ordinary builds exclude tests/developer tools and require neither Python
  nor TeX. Fresh default builds and native startup have been verified.
- [x] Scoped MIT terms for new contributions, retained attribution/third-party
  notices and explicit historical-media/Readline caveats.
- [x] Versioned ready-built manual PDF, standalone tutorial inputs, concise
  public README and explicit developer binary-dist target.
- [x] Binary-package checks cover content hashes, single archive root, permissions,
  all three profiles' startup, staging cleanup and failure handling.
  Compatibility remains the default; protected monitor/disk/configuration and
  PROFILES.txt are also included. UNIT0.original and STDMON/DFC403 stay only in
  the source distribution and are excluded by the package test.
- [x] Contributor documentation separates workflows, specifications, evidence,
  provenance and remaining work; no conversation or local-environment log is
  required to continue development.

## Verification status

- [x] Complete normal registered suite passes 29/29. All 29 sanitizer tests
  have successful coverage, including two targeted repetitions after correcting
  manual wall-clock limits; this is not an initially green single sanitizer run.
- [x] Focused final protection, manual/rebuild guards and Release archive checks
  pass with both profiles and the updated ready-built PDF.
- [x] One opt-in regnetest iteration completes in both compatibility and
  protected profiles with both numerical checks, one successful run and zero
  failed runs. Protected execution includes fresh guest compilation, CPU-mode
  selection and sampled process keys; this is not an arbitrary-workload soak.
- [x] Command-driven editor replacement, insertion/deletion, truncation and
  reopening pass in protected and compatibility frontend sessions. Eleven
  byte-exact exports verify results/originals, with unchanged base media and
  sampled protected process keys. Full-screen editing remains unverified.
- [x] Text display/copy/rename/concatenation pass in both profiles, including
  different logical allocation sizes, exact unchanged inputs/concatenation and
  restoration of disk/claims. Compatibility cmp and exported report pass.
- [x] Classify protected cmp as unsupported: reproducible tofrom1 break, with
  opt-in bounded FP reinitialization, preserved input exports and continued
  editing/cleanup. This is negative coverage, not a successful comparison.
- [x] Localize protected cmp to task-mode PL address 34 in linked tofrom1:
  IRQ-0 instruction/EA/PR/key/saved-PC evidence and independent binary module/
  probe checks pass. FP recovery preserves both inputs without a CPU bypass.
- [ ] Identify matching comparator source for a compatible rebuild. None has
  been identified locally; the older catalog binding cannot relink an existing
  binary. Export-and-compare on the host is the documented alternative.
- [x] Verify the host comparison alternative in both profiles: cmp/diff equal,
  different and missing-file statuses, exact edit hunk and unchanged exports
  after simulator exit. A colliding export is specifically rejected without
  changing the prior file's contents/inode/size/mode/mtime; FP/cleanup continue.
- [x] Both profiles retain text/binary contents and complete user-scope metadata
  across normal FP exit and simulator restart with separate overlays. Copied
  configuration paths work from another CWD; cross-profile overlay reuse is
  specifically rejected in both directions without changing source/copy media
  or sidecars. Crash durability and concurrent writers remain outside coverage.
- [x] Protected/compatibility catstat, cata, show and claim pass independent
  same-session packed-catalog/slot checks and temporary allocation/reclamation
  deltas. Boot-time temporary cleanup is distinguished from retained offline
  entries; this is not a full live-catalog consistency certification.
- [x] Both profiles pass catsort/search scope and exported-report checks against
  offline device-catalog names. Promotion, separate sorted groups, transient
  search scratch entries, special catalog lookup and complete cleanup are
  covered; 414/416 ordinary system entries remain profile-specific.
- [x] Source-only manual validation and its focused tests pass.
- [x] Protected/compatibility FP command files pass nested EOF/output return,
  direct/nested missing-program continuation, conditional lookup/groups and
  guarded SLANG export. Expected instruction words, no failed export, explicit
  cleanup and restored claims/disk are checked; general batch safety is not.
- [x] SLANG list.yes command-file workflow passes in both profiles: ordered
  listing/status, independent binary words, preserved source export, console
  restoration and full disk/claim cleanup. The default check does not execute it.
- [x] Opt-in execution of the valid SLANG sample passes in both profiles:
  W0=444/W1=321, exact loaded instructions and stable self-loop; protected
  task PR bf/key 1. Guest proc break produces interrupt 8 at the observed PC,
  then start reinitializes FP; later jobs and complete cleanup still pass.
- [x] The versioned 85-page PDF was explicitly rebuilt and visually reviewed,
  including catalog/profile differences, host comparison, SLANG execution and
  recovery, profile selection, contents and index. Fonts
  are embedded and final TeX output has no box overflow or unresolved references.
- [x] An isolated i386 build and bounded compatibility-profile startup work
  with both disk interfaces; this predates protected-profile acceptance.
  Full 32-bit regression and release support remain open.
- [ ] Verify the hosted CI workflow on a release revision.
- [ ] Complete the release-version and destination-host acceptance process.

For reproducible commands and limitations, use [Development](DEVELOPMENT.md),
[Test Fixtures](TEST_FIXTURES.md) and [Binary Distribution](BINARY_DISTRIBUTION.md).

## Deferred and incomplete capabilities

[Protection and Monitor Integration](PROTECTION_PLAN.md) is complete for its
bounded maintained-monitor profiles (approximately 100% of that workstream,
not of all possible emulator/software coverage). The stable checkpoint remains
`baseline-before-protection` at `a5de176`; accepted changes are integrated into
`main`, and the fully merged feature branch is deleted. Focused operator
acceptance and byte-identical monitor regeneration with candidate boot and host
transfers pass. M1 now specifies CPU access ordering, lifecycle policies,
host/device bypass boundaries and 18 acceptance-vector groups. M1 source review
is complete. M2 now has machine-API opt-in fetch/write/privilege enforcement and
real key/PR instructions, with focused native fault/partial-effect tests.
Configuration/CLI integration, packed-core loading and failed-autoload reset
cases have focused coverage. Boundary, register-alias, interrupt-return and
representative device-access vectors now pass; M2 is complete at the CPU/API
level. M3 now produces a separate guarded `e22` variant; protected startup,
date initialization and FP creation succeed, but `fromhost` breaks in `tofrom1`.
`claim` also completes with keyed task execution. The failing runtime sequence
executes privileged `pl w3 34` in task mode; a native regression reproduces its
compatibility-mode branch and required protected-mode fault. Its intended
later-machine register semantics remain open.
The RC8000 manual identifies opcode 28 as GG, distinct from RC4000 PL, but leaves
register-34 details unspecified. Library and linked-binary exports identify
an older tofrom in existing oldalgollib and a second probe in algollib's system0.
Experimental `rcin`/`rcout`, separately compiled with the older tofrom, now pass
text/binary transfers under protection (168,897 task steps; 50,000 keyed words).
The private image's original catalog binding is restored after compilation;
installed programs, both libraries and shipped media remain unchanged. An
nsystem-to-system alias trial fails even without protection and is not used.
The newer nsystem remains in the experimental binaries; these transfers do
not establish compatibility of all its paths.
The bounded local lifecycle check now passes stop/resume/remove/recreation,
full-area keys for 50,000 and 50,003 words, adjacent boundary preservation,
resumed text/binary transfers and a fresh text transfer after recreation.
Direct removal left temporary catalog entries visible in an exploratory run;
the passing checkpoint cleans them explicitly through FP, rather than claiming
automatic catalog reclamation from process/key behavior.
Two sibling FPs on separate console devices now have disjoint areas and keys
1/2. Guest SLANG builds four negative probes: cross-key data reads succeed,
while foreign writes/jumps and privileged PL/KS fault. The helper checks the
actual IRQ-0 instruction/state, all victim data/keys, a successful victim
export after each fault, and attacker FP restart. No host injection/rekeying
or CPU exception bypass is used to make a probe pass.
Protected guest Algol now compiles/executes hello and an arithmetic/procedure
sample, then rebuilds the unchanged transfer sources with the older tofrom
binding. Those newly built programs pass independent text/binary checks;
the visible tofrom binding is restored and remaining GG-probe executions are
zero. An explicit derived-system-binding comparison now eliminates the two
`9. oldalgollib bases` messages observed with a narrow temporary binding. The
helper checks matching system ranges/keys in both catalog layers. Protected
compilation, execution and transfers still pass on that private profile;
no original library or shipped artifact is replaced. The historical diagnostic
definition and general catalog/library compatibility are not established.
Live loopback Telnet now passes on that private system-binding profile: local
and remote FPs have distinct descriptors/core areas and keys 1/2, stored PRs
bf/df, and a common supervisor. Remote removal/recreation leaves the local FP
working; disconnect/reconnect retains the new remote FP and its file. Six
separate text exports and sampled key transitions pass, with unchanged media.
Nested guest parents/children now pass 1/10/13-word child areas. A parent with
PR 9f/key 2 rejects a privilege-widening child request, then creates and runs
PR bf/key 1 children. Whole-area keys and boundaries follow subtree stop/resume,
parent-initiated child stop returns key 2 and releases its answer buffer, and
recursive removal returns all keys to zero and clears both descriptors. Child
execution is observed in task mode; frozen counters verify actual stops.
M3 is complete for the identified bounded optional profile; the candidate is
not promoted to the default. The normal suite passes 29/29. All 29 registered sanitizer tests
are covered successfully after targeted repetition of two manual-related
timeouts with corrected finite host deadlines. Instrumented protected rebuild,
Algol/system-binding, nesting, isolation, lifecycle and live Telnet checks also
pass with leak detection enabled. Stable media/source/checkpoint identities
remain unchanged. The plan owns detailed completion state; general installed-
software compatibility remains outside this acceptance. The binary-only V2
monitor is excluded from the active workstream; continue with separately
selectable compatibility/protected profiles of the maintained monitor.
The developer profile preparer now creates both verified configurations and
separate runtime files in a new workspace. Both boot to the date prompt from
an unrelated working directory with unchanged media and volatile disk writes.
Both profiles now ship in the binary archive and ordinary build tree using
verified prebuilt artifacts. Explicit configurations, profile operating notes,
independent overlay choices and recovery are documented. Focused archive tests
pass both profiles and the compatibility default, including rejection of
missing/changed protected media. M4 and M5 are complete; the explicitly requested
PDF refresh is included. Commit `8c28198` integrates implementation/artifacts
into main by fast-forward, retaining the compatibility default and recovery
checkpoint. No remote publication is performed.

- [x] Implement optional storage keys, privileged checks, fetch/write protection
  and monitor/task transitions, with focused CPU/API verification.
- [x] Restore key setting in a protected monitor variant and test the combined
  monitor/CPU behavior.
- [x] Prepare and document separately selectable maintained-monitor profiles.
  The binary-only V2 monitor and its integration requirements are out of scope.
- [ ] Validate an actual archived software tape and installation workflow.
- [ ] Extend guest filesystem documentation and advanced catalog/batch recipes.
- [ ] Verify optional subsystem startup and inventory-only installed utilities.
- [ ] Define non-ASCII host/guest text conversion.
- [ ] Validate i386 comprehensively and correct ABI-sensitive package naming.
- [ ] Locate an independently verified original monitor source release.
- [ ] Decide whether a host-side SLANG assembler is necessary.
- [ ] Resolve inherited/third-party redistribution scope and release publication.

The design constraints and order of future work are in
[Roadmap](PORTING_PLAN.md). These gaps are not instructions to alter the
current compatibility fixtures or enable experimental behavior by default.
