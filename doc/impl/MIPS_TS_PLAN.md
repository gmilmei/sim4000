# MIPS/TS Bring-up Plan

## Scope and recovery

For the consolidated remaining-work order, use
[the native roadmap](PORTING_PLAN.md#ordered-continuation-plan). The agreed
RC610/PRIMO printer scope (P1) is complete; extra fault/stress coverage is
deferred. The current decision is to keep MIPS/TS unprotected for normal use
and defer protected candidate promotion/integration. Active continuation is
documentation upkeep and account/file/batch recipes (P2). This document retains
bring-up evidence and negative controls; earlier failed experiments do not
override the later accepted compatibility environment. A console-backed
printer descriptor is not an implemented printer device.
The current printer checkpoint is [RC610](RC610.md): all maintained monitors
now include printer/dev7/IRQ10 and all configs open append-mode output.
Native, Guest-s and compatibility/protected Algol source output are checked. Actual
PRIMO-to-RC610 output now passes separately from the console demonstration:
four sequential transports across two boots, exact output visible before
shutdown, signup/signoff and healthy TEM/SOS login/logout. A transport submitted
with detached output now reports end document/waiting, preserves earlier output
and resumes exactly once after append reattachment and start printer. General
host-error recovery and unrestricted queues remain outside this acceptance.
Two outstanding transports from separate bounded new processes now pass FIFO
byte comparison, normal completions and a same-instance followup. The second
concurrent blocking wait receives reply 3; explicit bounded status queries
provide the checked alternative. Exported primotrim specifies waitops=1, and
private buf 11 recreation does not remove the refusal. This strongly supports
a generated waiter limit; see [RC610](RC610.md#primo-wait-resource-diagnosis).

Establish a reproducible MIPS/TS session using the installed TEM, PRIMO and SOS
software. Bring-up began on `feature/mips-ts`, starting from `120c204`;
the approved compatibility demonstration was integrated into main at 645b8c3.
The original compatibility and protected profiles remain independent.
Do not change the shipped monitor, disk, startup scripts or default configuration
to make an experiment pass. All media writes go to explicit private working
copies or separate overlays. Generated exports, transcripts and runtime copies
stay under ignored build directories. No automatic PDF refresh or full-suite
run is required for each experiment.

Begin with the compatibility profile, IX enabled and the maintained monitor.
The installed subsystem binaries now pass bounded protected runtime checks,
but their complete software environment is not established as protection-safe.
Strict protection remains a separate assessment, not a CPU exception bypass.

## Milestones

M2 is complete for the accepted terminal/printer scope; broader optional
PRIMO devices and extended printer stress/fault cases are deferred. M5 protection
assessment follows P3, not a switch to the binary-only V2 monitor. The manual
manufacturing/documentation block is complete for its fixed recipe and need
not be repeated merely to add a future optional device.

- [x] M1: Export and inspect startmips, individual start scripts, configuration
  material, program descriptors and binary identities without executing them.
  Identify required devices, storage claims, base intervals and missing files.
- [x] M2: Start TEM and PRIMO separately on private media. Verify process
  creation, continued execution and terminal/device communication; preserve
  independent failure diagnostics. Derive only necessary private adjustments.
- [x] M3: Establish the SOS user catalog and a minimal test account using
  verified guest tooling. Start SOS and demonstrate an actual login and a
  bounded user operation, not merely a process listed as running.
- [x] M4: Reproduce the combined start and a Telnet session. Verify controlled
  logout/shutdown, persistence/restart and isolation of test profiles from the
  normal runtime. Define required files and terminal configuration explicitly.
- [ ] M5: Assess protected-profile compatibility separately, add focused
  regression checks and document supported operation and remaining limits.
  Do not promote experimental artifacts or merge without review.

Progress percentages are approximate milestone coverage, not a certification
of the historical system. A boot banner alone is not completion.

Current coverage is approximately 90%: inventory and repeated TEM/PRIMO
creation pass; PRIMO operator status/error handling works. A guest-generated
empty SOS catalog and a private reduced-resource startup reach SOS 5.0 started.
A named process fixture with explicit size bounds, a quoted test password and
a terminal record also passes byte-level export checks and SOS startup.
Authenticated interactive FP sessions now pass on a separate Telnet console
via both direct SOS go and TEM-mediated run, including wrong-password rejection
and normal logout. This completes M3 for the fixed compatibility fixture.
The isolated autostart test profile now passes two complete boots with TEM
login/logout and persistent-file readback. M4 is complete for this bounded
compatibility fixture. Broader PRIMO device/spool I/O and account policies
remain deferred beyond the bounded M2 scope; M5 is not complete. The actual
RC610 transport path, repeated jobs, detached-output recovery and two-client
queue/polling checks close the agreed terminal/printer acceptance. Additional
peripherals are not implicit requirements for completing M2.
The additional-disk prerequisite now includes retained file I/O and a complete
emulator restart with remount. SOS also starts with its original permanent
claim moved to that disk, and its swap-area descriptor is verified there.
The fixed jobfile probe also completes an authenticated minimal finis job.
With an explicit temporary disk claim, allocation and copy workloads also
finish normally; a readback checks character counts and checksums, not an
independent byte export. A separate user-scope fixture now survives job removal
and exports its logical text byte for byte. Restart persistence is verified
for the administrative fixture; multiterminal
application attachment and protected operation remain open.

## Current protected assessment checkpoint

The protected assessment is approximately 95% complete, separate from the
approximately 90% accepted compatibility coverage and the completed supported-
operation documentation described below. These describe bounded milestones, not a protected-service
certification. The maintained monitor is used; the binary-only V2 monitor stays
excluded. No shipped media/configuration or protection semantics are changed.

- [x] Start TEM alone on a private copy of MONITOR.protected/UNIT0.protected
  with PROTECTION=RC4000, IX enabled and the unchanged starttem script.
- [x] Start PRIMO alone under the same conditions using unchanged startprimo.
  Verify display/unknown-command/display through its protected operator path.
- [x] Preserve CLI CPU state, live process bounds/PR/PK/registers/saved IC,
  instruction window, actual code/descriptor keys and a complete packed core.
  Match reciprocal live process links to reject subsystem copies of descriptors.
- [x] Prepare a blank private second disk with mkdisc, kitlabel and kiton under
  protection. Import a separately guest-generated empty SOS catalog through
  rcin, verify its exact packed contents and start SOS independently with the
  original 2500/200 claim moved to disc1 in a new private startup file.
- [x] Assess combined startup and actual TEM/SOS login: reject a wrong password,
  enroll test/test, run FP message and lookup fp, then finis and verify removal.
- [x] Verify two sequential protected PRIMO-to-RC610 transports from a local
  FP client, including exact output visible before shutdown and later Telnet health.
- [x] Create and retain a file in the actual remote SOS session; verify its
  one-segment user descriptor and exact bytes after a full emulator restart
  using independent private persistent overlays for both disks.
- [x] Execute a deliberate forbidden monitor store from the SOS child; verify
  the live task/code/target keys and exact fault IC, unchanged target word,
  child removal, healthy subsystems and a fresh login/commands/logout.
- [x] Consolidate verified runtime behavior, preparation failures, evidence
  requirements and the boundary between diagnostics and a supported profile.
- [x] Localize the installed upsoscat failure independently to task PL address
  34 in linked tofrom1, with IRQ-0 saved-next-IC and preserved-input FP recovery.
- [x] Build and verify a separate private protected MIPS/TS candidate, with
  explicit preparation limitations and its own config/media/overlays.
- [ ] Review promotion, operating documentation and build/distribution integration
  separately; no candidate is automatically installed into source assets.

### Assessment decision and coverage

The maintained protected monitor can run the installed TEM/PRIMO/SOS binaries
for the bounded operations below without disabling protection, changing CPU
semantics or replacing the subsystems. This is a successful experimental runtime
assessment, not a claim that the entire installed software is protection-safe.
The shipped RC4000.mipsts.cfg/disk pair remains the compatibility release profile.

| Area | Verified result | Boundary / retained evidence |
| --- | --- | --- |
| TEM/PRIMO/SOS | Independent startup and combined keyed execution; PRIMO operator recovery | Core, CPU/PR/PK/keys and live descriptors; not every subsystem path |
| SOS terminal | Wrong-password rejection, test/test login, FP message/lookup and logout | Raw Telnet transcript and active/finished listings; one demo account |
| PRIMO printer | Two diagnostic jobs plus four candidate jobs over two boots; exact immediate append output and signoff | Per-boot replies/output hashes; not remote SOS submission or general fault recovery |
| SOS files/restart | Remote-created user file survives two full emulator processes | Separate overlays, both byte-exact exports, media/CPU logs and unchanged bases; normal exit only |
| Forbidden monitor store | Task RS to Key 0 faults; child removal, unchanged target and fresh login | Pre-instruction core, exact code/operands/PR/keys, next IC and post-fault health; not general security isolation |
| SOS catalog creation | Exact empty/demo fixtures manufactured with guest upsoscat in a separate compatibility session | Factory manifest and packed-byte validation; protected updater still fails |

Each manifest describes its own workload. Initial printer, restart and fault
runs are separate experiments. The later private candidate combines terminal,
file and printer checks over two automatic boots; the forbidden-store check
remains separate. Private
cores/transcripts under ignored build trees are diagnostic evidence, not runtime
assets. Reproduction commands belong in [Development](DEVELOPMENT.md#protected-mipsts-startup-assessment);
guard assertions belong in [Test Fixtures](TEST_FIXTURES.md#separate-protected-subsystem-diagnostic).

### Remaining work and profile gate

- [x] Keep unprotected MIPS/TS as the current operating/build/distribution profile.
  Defer protected candidate promotion; preserve its developer tooling/evidence.
  This decision does not disable or change the standalone protected profile.
- [x] Derive clean private bases/startup with the source-built
  protected monitor, exact catalog provenance and no runtime verification files.
  Do not reuse the compatibility MIPS/TS pair by merely setting PROTECTION.
- [x] Prepare a separate candidate config, deterministic autostart (FP depth at most
  two), flat filenames and independent persistent overlays; verify two fresh
  boots of that finished candidate, including terminal/file and local printing.
- [ ] Deferred: update operating instructions and packaging for a protected profile
  only after a new explicit decision to promote it;
  promotion and PDF regeneration remain explicit operations.

The private candidate gate now passes via
dev/python/prepare_protected_mips_profile.py --verify. Candidate files are flat:
MONITOR.protected (unchanged maintained monitor), UNIT0.mipsts-protected,
UNIT1.mipsts-protected and RC4000.mipsts-protected.cfg. The explicit factory
produces only verified demo catalog data in compatibility mode; all disk-pair
preparation and verification use protection. startsos changes only its permanent
document to disc1. startarea mounts device 16, sets 1986-09-30 and directly reads
starttem/startprimo/startsos at depth two. Exact startup exports are required.
The intermediate psosboot entry is removed before freezing the clean bases.

Preparation writes through only exclusive private disk copies. The four
candidate artifacts are frozen before any verification, with hashes and catalog
provenance in manifest.json. Verification copies them into a separate runtime,
changes only its Telnet port and uses two independent persistent overlays.
Two automatic boots (no date prompt or manual subsystem reads), both remote
login/FP/file sessions, exact retained-file exports, four local PRIMO jobs and
9076 exact immediately visible output bytes pass. Append output survives both
normal exits. Candidate/runtime base hashes remain unchanged; verification's
user files and overlays never become candidate bases. This closes the private
candidate gate, not source promotion, packaging, public support or crash recovery.

Protected upsoscat now has its own instruction-level diagnosis, independent of
cmp: task pl w3 34 (730022), PC 54206/00d3be, saved-next IC 54208/00d3c0,
effective address 34, PR=bf and code Key 1. The installed 114-segment binary
contains linked tofrom1/tofrom2 and the probe sequence in tofrom1. This is the
same instruction-level incompatibility as cmp, now measured rather than inferred.
Protected PL is privileged; allowing that task instruction merely to build a
catalog would weaken the CPU model. A catalog rebinding does not relink the
embedded runtime. A source-based updater rebuild remains optional and needs
matching source/runtime identification first; no binary patch is applied.
The explicit separate compatibility factory is a documented data-preparation
workaround, never a mode switch inside the protected runtime. Protected rcin's
36-segment minimum is a resource constraint, not a CPU fault: use a small
retained seed for the existing 30-segment demo quota rather than inflating it.

Remote SOS printer submission, broader accounts/concurrency, batch workloads,
arbitrary sibling/ancestor faults and crash recovery remain optional extended
coverage, not reasons to redefine the accepted printer block as incomplete.
No V2 monitor, subsystem rebuild or hidden protection bypass is required by
the successful bounded runtime assessment.

### Detailed checkpoints and diagnostic interpretation

Both first subsystem checks reach their startup banners and continue waiting
with stop count 0, PR=bf and PK=1. Saved ICs are 45498 (TEM) and 45824 (PRIMO),
in Key-1 memory; their live descriptor is in Key-0 monitor memory. PRIMO's
operator test recovers normally from an unknown command. These observations
establish startup and one operator path, not terminal spooling, printing under
protected MIPS/TS, SOS enrollment or user-file persistence. In particular,
protected standalone Algol printing does not establish protected PRIMO support.

Use dev/python/probe_mips_protection.py with a new destination and --component
tem or primo. Evidence stays private; no new release profile is produced.

SOS also reaches release 5.0/started on its own: stop count 0, PR=8f, PK=3,
saved IC 47964 in Key-3 memory and a Key-0 live descriptor. Its swpsos entry is
independently checked as 1722 segments on disc1. The original startsos remains
unchanged; only private psosboot selects the second disk. Empty-catalog bytes
round-trip exactly, including resizing rcin's initial 36-segment area to one
segment with guest set before export/retention. No account is provisioned.

The preparation path has a separate incompatibility: protected upsoscat stops
with break 0 in tofrom1, called from lines 1094--1099. The separate updater
probe now identifies the task PL-address-34 fault above; this is not a failed SOS startup. Its
transcript and complete preparation-failed core are preserved. Rather than
turning protection off in that session, an explicit compatibility-only factory
runs guest upsoscat for maxp 4, verifies its one-segment output and records a
hash manifest. A fresh protected session imports that verified artifact, never
switching protection mode. Invalid/missing provenance or changed catalog bytes
are rejected. This does not claim a protected-compatible catalog updater.

The paired --prepare-sos-catalog / --component sos --catalog-from workflow is
documented in [Development](DEVELOPMENT.md#protected-mipsts-startup-assessment).
Writes are volatile by default; only --restart explicitly selects private
persistent overlays. These workspaces are evidence, not clean release images.
The factory now also accepts --demo-account,
producing the exact known two-segment terminal catalog with test/test, a
30208-halfword child and bounded disk resources. --component combined imports
that catalog, starts TEM/PRIMO/SOS and verifies a loopback Telnet session.
No protection switch occurs during the protected run. PRIMO operator recovery
and the disc1 swap descriptor remain checked alongside terminal operations.

An active packed-core snapshot records all three subsystem descriptors and the
SOS child after message/lookup. TEM/PRIMO/SOS code keys are 1/2/5. At the input
prompt the child has zero stop count but state 184 (stopped/parent/no-stop plus
waiting), PR=e7 and PK=3. Its saved PC lies in memory currently keyed 5, not 3.
This is a suspended child, not evidence of a failing instruction fetch: SOS
may reclaim its interval between interactions. The probe records this explicitly
and accepts the parent key only for that stopped-by-parent snapshot; live
subsystem code must remain accessible through its saved PR. PK is the store-key
register, not the instruction-access permission mask. Earlier stricter snapshot
assertions are retained as private negative evidence, not guest failures.

The --printer combined probe now compiles the unchanged primo_print.alg example
with installed Algol under protection, using rcin rather than the incompatible
newer fromhost transfer library. A local 150000-halfword FP client submits a
29-byte job and a 4509-byte job (161 lines). Both return define/wait result and
reply 0, state 5 and cause -1. All 4538 bytes match independent input exactly,
are visible before shutdown and remain unchanged after exit. Signup and signoff
use the real printer process/device 7; no console surrogate is involved. PRIMO
and TEM/SOS stay healthy and the subsequent protected Telnet session passes.
No monitor, startup, PRIMO binary or CPU/device semantics are changed.

The --restart probe now passes two complete emulator boots with independent
persistent overlays on the unchanged private UNIT0.protected/UNIT1.protected
bases. On boot 1, an administrative FP supplies only the small keepseed file.
The real SOS test/test child creates keepsake, retains it with scope user and
displays its text. After logout and full emulator exit, boot 2 mounts the
existing disc1 without relabelling it, restarts TEM/PRIMO/psosboot manually and
logs in again. It reads the existing keepsake without recreating it. Both
matching-base administrative rcout exports equal the independent 29-byte ASCII
fixture exactly. File scope remains user; both base hashes remain unchanged.
CPU protection/IX and both persistent media identities are explicitly checked.
Raw terminal transcripts, separate second-boot log and restart-result.json
preserve the evidence. Normal exits, not crash recovery or autostart, are tested.

Early harness failures exposed the historical system-command length limit,
an occupied process name mount, copy's metadata-only default output (c=copy
displays text), and CLI 80-column wrapping. Fresh private workspaces verify
the corrected sequence; no guest software or CPU behavior was changed.

The separate --isolation assessment now passes a deliberate RS w2 x1 from a
guest-SLANG-built badstore program, executed by the remote SOS child. At the
instruction breakpoint the CPU is a task (monitor=no), PR=e7, code Key 3,
W1=8192 and W2=1; address 8192 has Key 0 and contains zero. The exact loaded
instruction body and literal target are checked in a complete pre-fault core.
Continuing produces exactly ***break 0 79210 for the RS at 79208, followed by
test removed after break. The monitor word remains zero. TEM/PRIMO/SOS stay
healthy; a fresh test/test login, FP message/lookup, normal logout and PRIMO
display/error-recovery all pass. The private bases remain hash-identical.

FP initially visits the same predicted program address while loading. A bounded
64-visit breakpoint filter requires the expected live operands, PR/key and exact
code rather than treating the first address hit as the test instruction. FP's
two reserved words are excluded from the loaded-code comparison. SOS reclaims
the interval after removal, so the post-removal core is diagnostic only; it must
not be mistaken for the actual faulting program. No instruction, register, key
or protection mode is modified by the diagnostic breakpoint.

This checks one forbidden monitor write and subsystem recovery, not arbitrary
sibling/ancestor isolation, prevention of all monitor-call misuse, or a security
sandbox. The separate profile gate is now deferred; continue with unprotected
MIPS/TS operating recipes. Remote SOS-client printing is not established by the local FP spool
test. Bounded terminal/printing/restart/isolation checks are not a release profile.

The unchanged scripts request these allocations:

| Component | Core halfwords | Permanent segments/entries | Binary |
| --- | ---: | ---: | --- |
| TEM | 11000 | 360 / 3 | btem |
| PRIMO | 16938 | 18 / 3 | bprimo |
| SOS | 300000 | 2500 / 200 | bsos |

All request functions 1 through 5 and the system base interval
`-8388607 8388605`; SOS additionally requests one child process and key 3.
The total permanent segment claim is 2878. Generation material selects
RC4000 for SOS and describes three active terminals/two groups for TEM;
that material is not proof of every compiled binary option. Source texts
needed to regenerate the subsystems are not established as installed.

## Initial constraints

The supplied catalog contains startmips, starttem, startprimo, startsos,
btem, bprimo, bsos, configuration material and upsoscat, but no soscat.
Installed binaries do not imply installed subsystem sources or a buildable
replacement. Do not run configuration/generation scripts merely to inspect
them. Check actual exported contents before supplying resources or accounts.

Start with the developer-only inventory command:

```sh
cmake --build build-protection --target sim4000_build_host_image
python3 dev/python/probe_mips_ts.py build-protection/mips-inventory
```

The destination must not exist. The command prepares private MONITOR/UNIT0
copies, a configuration and exchange directory. The helper exports text,
packed binaries and lookup metadata with volatile writes. It never executes
startmips or its component startup scripts. Both original media and copied
base files must remain unchanged. Keep failed workspaces for diagnosis; use
a new destination for each retry.

To repeat the initial component startup through the actual frontend:

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-components \
  --start-components --simulator build/sim4000
python3 tests/python/test_mips_ts.py
```

This first exports the inventory, then boots a fresh volatile session on the
same private base copies. At guest system s it uses `read starttem`, then
`read startprimo`, not FP `i` input. System commands complete at `ready`, not
at the FP BEL prompt. Each later system command gets its own attention/s
selection. Actual listings must show the requested sizes, nonzero keys and
zero stop counts with run/wait state. After a bounded million-call advance,
both processes must still be present and active; TEM reports release 5.1 and
started. This is component creation/initial waiting, not terminal or spool I/O.
Copied/source media hashes and clean frontend/PTY exit are checked.

## Verified operator and initial SOS probes

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-primo \
  --probe-primo
python3 dev/python/probe_mips_ts.py build-protection/mips-catalog \
  --probe-catalog
python3 dev/python/probe_mips_ts.py build-protection/mips-sos \
  --probe-sos --sos-segments 2000 --sos-entries 8
```

PRIMO is selected by attention and its process name. Its selection prompt is
`=` followed by BEL. `display` completes with a timestamped `primo:ready` line,
not another BEL. An intentionally unknown `zzzzz` command completes with
`primo:***command unknown`, without an additional ready line. A second display
and process listing must still work. This proves bounded operator handling,
not printing, queued transports or TEM terminal traffic.

The updater reads an explicit text entry: `soscat=upsoscat catspec`.
Bare upsoscat supplies no interactive configuration prompt. The known minimal
input `maxp 4` completes with end 42 and creates a one-segment temporary area.
Its packed export is exactly 759 all-one bytes followed by three 24-bit words
0, 50, -1. The field meanings/capacity are not inferred from that footer alone.
`--catalog-text` accepts alternative disposable input for diagnosis; an
abnormal end or an area left behind is not a valid-user-catalog certificate.
Each catalog experiment also writes `catalog-outcome.json`, separating normal,
rejected, incomplete and inconsistent completion. Normal requires exactly one
end line, no failure markers and an export matching the allocated segment
count; it is not a semantic account/terminal certificate. A rejected run stays
rejected even if its residual export has the expected allocated length or its
diagnostic says errors 0. The final console summary reports this distinction.
In particular, adding `proc test` creates a two-segment area with a named
process record, but does not yet establish usable claims, password or login.

SOS preparation uses an administrative FP with base `-8388607 8388605` and
login `-8388606 8388605`. Ordinary user-scope startup copies are not visible
to s's read command in the checked setup (area unknown). Administrative setup
is split across short system commands; an overlong console command is not
equivalent to the included multiline startup files.

The empty catalog is promoted to user scope. A private sosboot text is derived
by replacing exactly one resource clause in the exported startsos; the original
installed entry is never changed. After the preparation FP exits normally,
s reads sosboot. The shipped 2500/200 claim is rejected with bs claims exceeded;
300/8 reaches SOS but fails initialization at swpsos with 1722 reported.
2000/8 reaches release 5.0 and started. TEM, PRIMO and SOS then remain active
with zero stop counts; no SOS init trouble is accepted. This is a minimal
bring-up claim, not a justified multiuser capacity or an independent decoding
of the swop-area request. The generated catalog and startup modifications
remain only in that session's volatile overlay; host exports preserve evidence.

## Named process fixture

The following updater input completes normally with end 56 and a two-segment
catalog. It is a deliberately incomplete, disposable account fixture, not a
recommended account policy or a supported login configuration:

```text
maxp 4
proc test
mins 20000
maxs 30000
pass "test"
```

The password must be quoted in this input form: bare `pass test` and
`pass 1234` produce a password diagnostic and abnormal completion. The export
contains the test name in the directory, record reference 1, the literal test
password in the process record and the 24-bit values 20000 and 30000 at packed
offsets 801 and 804. Passwords are therefore not secret in this fixture's
packed export. Never use real credentials in these experiments.

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-account \
  --probe-account --sos-segments 2000 --sos-entries 8
```

This mode fixes the complete input rather than accepting arbitrary catalog
text. It requires normal completion, the two-segment descriptor and exact
1536-byte output, then repeats the SOS started/all-three-active checks. The
record-layout function is a fixed regression expectation, not a catalog
writer or a general format specification. Two fresh account runs and an
empty-catalog control pass. Neither terminal connection nor password checking
has been exercised. Base intervals, FP/device and backing-store claims still
need deliberate account configuration before a useful guest job is expected.

Initial terminal syntax experiments failed. With a current test process, inputs such
as `term console1 test`, `term console1 1 test`, `term console1 1 1` and
`term console1 "console1" test` fail with a locid diagnostic and abnormal
completion. They are negative evidence, not terminal recipes. No terminal
record is promoted from these runs; a residual output area alone proves
nothing about updater success.

### Bounded parser investigation

The earlier numeric-first interpretation is not supported by the native trace:
the term parser first reads a name. `term console1 "1"` and
`term console1 ""` pass the earlier locid check and fail later at term-key.
By contrast, `term console1 1` fails at locid and `term 1 console1` fails at
name. The verified partial form is a name followed by a quoted field, not an
integer followed by a name. The quoted field's complete syntax and semantics
are not yet established. Adding trial positional numeric fields has not
produced a valid record. Both prefixes remain rejected evidence, not terminal
definitions or physical device assignments.

The optional developer debugger probe makes the investigation reproducible:

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-parser \
  --probe-catalog --parser-snapshot \
  --catalog-text 'maxp 4
proc test
term console1 "1"'
```

It saves packed core immediately after the first updater response, identifies
one word-aligned native sequence matching the known packed upsoscat offset
40638, and sets a debugger execution breakpoint there. It replays the updater
with output name sosagain, verifies the instruction bytes again at the stop,
saves stopped core and traces at most 20000 interpreter calls before quitting.
Guest code and CPU semantics are not patched. Missing/ambiguous signatures,
a mismatching program, a replay timeout or reused storage at the breakpoint
are failures, not certified parser traces. This requires catalog-only mode.

The two core files, parser-trace.txt and parser.json remain ignored experiment
artifacts. JSON records program/core identities, breakpoint and trace bound;
replay completion is explicitly unverified. Virtual-program page faults can
enter the monitor during the trace, so the final IC or temporary exception
state alone is not a subsystem failure. This stops at a debugger checkpoint,
not at normal updater completion, and produces no normal-catalog certificate.

### Verified terminal-record fixture

The longer bounded trace establishes why the positional guesses fail: the
second quoted field precedes the trailing numbers. This complete input is
accepted by the installed updater, with normal end 58:

```text
maxp 4
proc test
mins 20000
maxs 30000
pass "test"
term console1 "1" "test" 60 2
```

The observed order is terminal name, quoted locid, quoted terminal key, then
the two numeric values. Do not confuse this terminal key with the CPU storage
protection key. This is an observed disposable fixture, not a full statement
of accepted lengths, defaults, access policy or time units. Both strings are
dummy test data; they are stored in clear text in the packed catalog.

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-terminal \
  --probe-terminal --sos-segments 2000 --sos-entries 8
```

This mode fixes the entire input and checks all 1536 packed bytes before
starting SOS. Relative to the named-process fixture, it replaces the first
39-byte all-one terminal slot at offset 1314. The twelve-byte name is followed
by three locid bytes, twelve key bytes, packed word 03c002 (the supplied 60 and
2 in its two halfwords) and three zero words. The remaining all-one tail begins
at 1353. No general catalog writer or direct disk patch is involved.

Two independent private runs reach SOS started and healthy TEM/PRIMO/SOS rows.
The second repeats call 1 console1, start and start test: their replies remain
ready, terminal not connected and process unknown respectively. A syntactically
valid terminal record therefore does not establish the TEM attachment or an
enrolled user process. The next gate is actual SOS/TEM terminal association
and deliberately configured account claims, followed by a real login/job.

## SOS operator diagnostics

SOS can be selected by attention and its process name, independently of a
working TEM terminal. Its selection prompt is `>` followed by BEL, unlike
PRIMO's `=` prompt. The developer probe can retain a bounded sequence of
single-line commands after the checked account startup:

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-operator \
  --probe-account --sos-segments 2000 --sos-entries 8 \
  --operator-command 'call 1 test' \
  --operator-command start \
  --operator-command 'call 1 missing' \
  --operator-command display
```

Each command gets a fresh attention/SOS selection. The probe verifies the
selection prompt, echoed command and complete timestamped SOS reply, retains
each exchange separately, and checks all three active process rows afterward.
At most eight commands of at most 79 printable ASCII characters are accepted;
embedded controls and use without an SOS/account startup probe are rejected
before creating a workspace. Reply waits have finite deadlines. These are
diagnostic exchanges, not assertions that the requested operation succeeded.

Observed replies in this isolated setup:

| Input | Reply |
| --- | --- |
| `display` | `***sos: command unknown` |
| `call test` / `call 1` | `***sos: syntax` |
| `call 1 test` | `sos: ready` |
| `call 1 missing` | `sos: ready` |
| `start` after `call 1 test` | `***sos: terminal not connected` |
| `start test` | `***sos: process unknown` |
| `start 1 test` | `***sos: syntax` |

The deliberately absent name receiving ready is an important negative control:
do not interpret a call acknowledgement as account lookup, password checking,
process creation or terminal attachment. The meaning of call's arguments is
not yet established. No operator command here demonstrates a working login.
The local operator channel itself and continued subsystem health are verified.
Using `call 1 console1` instead of the test name also returns ready, but a
subsequent start still reports terminal not connected and start test reports
process unknown. A physical-looking name therefore does not establish a
terminal mapping either. The updater's `ipro test` input fails with trans;
it must not be documented as an information/listing command. Static inspection
of the updater binary has not yet established the complete term grammar.

The next gate is a documented test process/account and terminal entry that
produce an actual interactive login and a bounded guest operation. No hosted
Telnet listener, default media promotion or protection relaxation is part of
these probes. Fifteen pure guard tests cover reply/error boundaries, packed empty
and named-process catalog evidence, constrained startup changes and active
component state, operator input limits and complete response framing, and
updater outcome classification without promoting residual output.
The parser locator also rejects absent, duplicate, unaligned or changed native
instruction signatures.
Terminal guards cover normal completion, the complete packed record, account
fields, descriptor size, record boundaries and unchanged trailing fill.

## Integrated blank image and second disk checkpoint

The runtime provides `--mkdisc FILE` and operator command `mkdisc FILE`; there
is no separate utility. Creation is exclusive and zero-fills 180 sectors
(138240 bytes), without guest labelling or attachment. Existing paths, including
symbolic links, are refused. The same C executable is used in ordinary builds
and binary distributions; creating a disk needs no Python.

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-second-disc \
  --probe-second-disc
```

This developer probe invokes actual integrated creation, configures device 16
with a persistent UNIT1.overlay on its private blank UNIT1 and boots private
standard media. UNIT0 writes remain volatile. Guest commands are:

```text
all big run
kitlabel 16 disc1 catdisc1 slow 36 9 2030
kiton devno.16
show
finis
```

The first line is a system s command creating an administrative FP named big;
the others are FP commands. A broadly based ordinary new FP is not equivalent:
the initial attempt failed with algolcheck and a device-status diagnostic.
Using all, kitlabel completes normally (end 35), kiton reports disc1 mounted on
16 (end 48), and show reports 2025 free slices / 18225 free segments on disc1,
independently of the 3075 on disc. The blank UNIT1 base remains unchanged;
the successful session retains 45 sparse overlay sectors. Shipped and private
base hashes are checked.

The historical startup recipe's system kit 16 command is not available at the
post-autostart prompt: the included s removes its catalog-command code when
the first internal process is created, as its distribution notes explain.
The utility kiton requires devno.16, not bare 16 or disc1. No shipped startarea
is edited. The initial checkpoint verified labelling, mounting and free-space
reporting; the retained-file extension below adds I/O and remount. The separate
SOS allocation checkpoint below uses this prerequisite without editing shipped
startup files.

### Retained file and complete restart

The same second-disk probe now imports 158720 bytes of numbered ASCII records
through fromhost, explicitly allocates dkeep on disc1, copies the input there
and retains it with scope user. Lookup must report exactly 207 segments on
disc1 and user scope; the exported content must match every input byte.
Different record numbers prevent a repeated-sector alias from looking correct.

After normal FP finish and simulator quit, the first host process and PTY are
closed. A new emulator process boots the unchanged private UNIT0 and MONITOR
with the same UNIT1/UNIT1.overlay pair. A fresh administrative FP mounts device
16 with kiton devno.16, looks up dkeep and exports it to a different host file.
The complete lookup reply, including stored date, scope, bases, key and other
descriptor words, must be unchanged, and the export must match the original
158720 bytes exactly. No source text is reimported in the restarted session.

The 207-segment file exceeds the blank starter image's 180 sectors. Successful
readback therefore includes persisted data beyond its original length, rather
than just metadata or data within the initial allocation. The base stays
zero-filled and unchanged; all new storage lives in the persistent sidecar.
Two independent fresh workspaces pass the full numbered-record workflow.
second-disc.json records export/base/overlay hashes and the explicit restart
and metadata checks. This does not yet establish protected-profile operation,
kitoff/removal handling or correctness of MIPS swap contents.

### SOS allocation on the second disk

The --sos-disc1 option combines the fixed terminal-catalog startup probe with
private second-disk preparation. Before TEM starts, an administrative FP labels
and mounts the new disk. The private sosboot changes exactly one clause from
perm disc 2500 200 to perm disc1 2500 200; no resource reduction is needed.
SOS 5.0 starts, and TEM/PRIMO/SOS remain active with zero stop counts.

A second administrative FP looks up swpsos after startup. It must find exactly
one 1722-segment temporary area on disc1. The transcript is retained in
sos-disc1-storage.txt, and all three components must still be healthy after
finishing the inspection FP. Source and private base-media hashes are unchanged.
Repeated fresh workspaces reach this checkpoint. The one-disk 2000/8 terminal
startup remains a successful control. No terminal login, job completion or
swap-content verification follows from the descriptor alone.

Next gate: give the successful batch test/password enrollment a dedicated,
bounded lifecycle check. Its parent's stop count must be interpreted using
the monitor's child-process semantics, not the zero-stop startup guard.

The dialogue helper now recognizes a complete password question rather than
waiting for an impossible timestamped completion. Optional --test-password
accepts only dummy values test and wrong. A private batch test experiment
rejects wrong with bad password and leaves all services active. With test,
SOS reports test enrolled and creates a 30208-halfword child test, key 3,
parent sos. The generic postcondition rejects that diagnostic run because
SOS's stop count becomes 1. The start-internal-process description in
monsrc/montxt11 explicitly says starting a child increments its parent's stop
count. This calls for a separate enrollment lifecycle check, not a weakened
startup guard or a completed job/login claim. Bare in test and in 1 still
report identification illegal. Authentication and attachment are separate gates.

### Batch lifecycle checkpoint (not yet passing)

The opt-in --verify-batch mode requires the fixed account/terminal fixture,
exactly --operator-command 'batch test' and --test-password test. It waits for
the post-enrollment BEL, advances a bounded million interpreter calls and
requires a complete system listing. The intended guard requires TEM/PRIMO
with zero stop counts, SOS waiting under s with stop count 1 and a 30208-halfword
test child, key 3, running or waiting under SOS. A second bounded advance must
retain that relationship. This does not test job completion or terminal login.

The new live checkpoint is not yet passing. On both the second-disk stock-claim
fixture and the one-disk 2000/8 control, SOS confirms test enrolled, but the
subsequent system list is interrupted by a to sos console transfer and BEL
instead of completing with ready. Advancing before the list and waiting for
the post-enrollment BEL do not resolve it. The debugger checkpoint reports
exceptions=000. The earlier immediate listing remains evidence of child
creation only, not a stable lifecycle. No passing batch result is emitted.

System-reply parsing fails explicitly on this console transfer rather than
waiting for its deadline or treating a partial list as success. In the private
--verify-batch mode only, a list interrupted this way now answers the pending
SOS input with one empty line and resumes waiting for ready. A second handoff
within that response still fails; ordinary probes never send this response.
This is a diagnostic action, not a demonstrated job-input recipe.

The empty response releases the queued s output. Repeated fresh second-disk
runs then show SOS runnable with stop count 0 and test stopped under SOS, size
30208/key 3. This state remains after another million calls and another list.
The strict enrollment lifecycle guard still rejects it; it has not been
relaxed to accept a stopped child.

The second bounded checkpoint saves packed batch-core.bin before requesting
the final list. A descriptor reader uses the montxt1 a11..a34 offsets, requires
a unique internal test descriptor with the observed size and a containing
300000-halfword SOS parent, and records batch-state.json. The observed child
descriptor at 12030 has first/top 77324/107532, state 184 (0xb8), stop count 0,
key 3, exception register 0 and saved IC 77988. montxt1 defines 0xb8 as waiting
for start by parent, not running after error. The parent's saved state is
0x8f (waiting for event), with stop count 0. Text in the child area contains
FP stack/input diagnostics; this alone does not prove a usable FP session.
The snapshot is taken before the subsequent attention/list interaction, so
the stopped-child observation is not solely an artifact of printing that list.

Next diagnosis: determine what batch input SOS expects and how it releases
or resumes its child. The monitor terminal source in montxt8 explains to/from
headers as input/output sender changes; a to sos header is not by itself a
crash. Do not change CPU/device semantics or the startup guard on this evidence.

### Documented jobfile path and minimal normal completion

Reference material is available outside the standalone tree in doc/mipsts:

| File | Identification | Relevant evidence |
| --- | --- | --- |
| 30008397.pdf | Notes for MIPS/TS Course, 11 scanned pages, mainly Danish | SOS scheduling and terminal access; requires OCR for searching |
| 30008425.pdf | Binding of Terminals when Running MIPS/TS, LKN 790410, two pages | Distinct operator/jobstart/application roles; explicit batch with jobfile example |
| 30008466.pdf | MIPS/TS Operating Guide, Second Edition, October 1979, RCSL 31-D585 | Operator commands, startup, shutdown and SOS/TEM/PRIMO responsibilities |
| 30008475.pdf | Swopping Online System (SOS), February 1982, RCSL 31-D662 | User/reference/operating/installation guide; catalog defaults and complete input grammar |
| 30007648.pdf | Terminal Access Module (TEM), Third Edition, March 1983, RCSL 31-D689 | User/reference/installation guide for terminal access |

The two-page terminal note is scanned in reverse page order. Its printed page
2 shows batch rcsave jobfile savejob and explains why jobfile permits reuse of
the same terminal. The operating guide refers to SOS Manual 31-D512 (September
1978) and TEM Manual 31-D513 (October 1978). These are older than the installed
SOS release; syntax must still be checked live. The historical PDFs are not
renamed, edited or made build dependencies.

The new --batch-job developer mode imports a private batchjob containing only
finis, copies it to a compact guest area and gives it user scope before SOS
starts. It requires the fixed account/terminal fixture and dummy password test;
its only operator command is batch test jobfile batchjob. It cannot be combined
with the older --verify-batch console-input lifecycle diagnostic.

After enrollment, one million calls are advanced and a core snapshot retained.
A complete system listing and the transcript must show exactly one enrollment
followed by exactly one test removed after finis, no guest error markers and
no remaining test process. TEM/PRIMO/SOS must still have their expected sizes,
keys and zero stop counts. The probe records batch-job.json only after these
checks. No empty response to SOS is sent in this mode.

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-job \
  --probe-terminal --sos-disc1 --test-password test --batch-job
```

The minimal job completes normally on fresh second-disk runs and on the
one-disk 2000/8 control. This establishes
password enrollment, reading a named jobfile, FP finis processing and SOS child
cleanup, not useful computation, terminal attachment or output-file handling.
An earlier job containing batchproof=copy batchjob, scope user batchproof and
finis instead produces ***break 10 1 and test removed after break. Thus the
jobfile option resolves the console-input dependency, but a copy workload is
still rejected. Do not infer its cause or general account-resource correctness
from the successful finis-only control.

### Workload isolation and catalog-default evidence

The optional --batch-workload selects only fixed disposable inputs: finis
(default), message, lookup, read, allocate, copy or retain. All must pass the
same normal enrollment/removal and service-health guards. Message additionally
requires exactly one batchprobe output line; lookup requires exactly one
batchjob descriptor on disc. No copy-content verification is claimed by these
termination checks. The preparation also exports the unchanged FP binary for
offline inspection. A nondefault workload without --batch-job is rejected.

The message and lookup controls complete normally. Read (copy batchjob),
allocate (batchproof=set 1) and copy without scope user all produce
***break 10 1 and normal SOS removal after break, not after finis. Thus scope
promotion and output-file allocation alone cannot explain all failures.
A bounded breakpoint experiment at two suspected FP sites did not hit them;
it does not identify the meaning or origin of break 10 1. No CPU/device change
is justified by these observations.

The archived RC8000 SW index identifies D662 as Bits:30008475 and D689 as
Bits:30007648 (also listed as a separate scan, Bits:30008480). It also lists
System Utility Part One manuals Bits:30008478 (D676, 1983), Bits:30010391
(1987) and Bits:30007499 (1989); these have not been read here. The RC4000
File Processor entry Bits:30009220 is explicitly BINARY, not a PDF handbook.
MIPS releases 7.0 (disk, Bits:30004128), 7.1 (tape, Bits:30008171), and 6.0
(IMAGEDISK pair, Bits:30009129/30009128) are identification leads only, not
installed or assumed compatible with the maintained monitor.

D662 section 5.3.1, printed page 50 (PDF page 56), documents default buffers 4,
areas 7 and disc key0 6 entries / 0 segments; keys 1..3 default to zero.
The six entries must not be mistaken for six segments. This agrees with the
existing fixture's packed 004007 resource word and its disc claim's entry
word 6 followed by segment word 0. The defaults are insufficient evidence of
a generally usable account. Printed page 52 gives the bs <name> key0 <entries>
<segments> ... syntax; term records come last in a process description.
The explicit resource comparison below now addresses the temporary claim;
the original default fixture remains the negative control.

### Explicit temporary disk claim

--account-segments 30 adds exactly this directive, before any terminal record:

```text
bs disc key0 6 30 key1 0 0 key2 0 0 key3 0 0
```

The installed updater requires the complete four-key group. An initial
abbreviated key0-only clause is rejected; residual output is never accepted.
The complete account fixture ends with end 58, the terminal fixture with
end 60, and each exports exactly 1536 bytes. Relative to its zero-segment
control, only packed byte 899 changes from 0 to 30: the word at 897..899 is
the key0 segment claim. The probe checks the full expected catalog bytes,
not just that word. Unsupported segment values or unrelated catalog input
are rejected before workspace creation. No image or binary is patched.

With this sole catalog change, allocate and copy workloads finish normally
where the zero-segment controls produced FP break 10 1. Both the account-only
and account/terminal fixtures pass. This isolates inadequate temporary disk
quota in these workloads; it does not establish a general meaning for every
FP break 10 or prove arbitrary resource sufficiency.

The roundtrip workload copies batchjob to temporary batchproof, then runs
copy batchproof before finis. Both reports must name their expected file,
one segment, 47 characters and checksum 4535 (the exact input's character
count and byte sum). Missing/duplicate reports or changed numbers fail even
when the job ends normally. This is aggregate-content readback, not a
byte-for-byte export: checksum collisions are possible. The result explicitly
leaves file_copy_verified false and records copy_summary_verified separately.
The saved result includes the temporary segment claim. Repeated private
second-disk runs and the reduced single-disk control pass.

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-resource-roundtrip \
  --probe-terminal --sos-disc1 --test-password test --batch-job \
  --batch-workload roundtrip --account-segments 30
```

Without persistent-scope quota, the retain workload fails the acceptance guard: copy succeeds, but
scope user batchproof reports no resources, even though finis subsequently
removes the job normally. The guard rejects the error marker and never
promotes normal removal to successful retention. The separate positive
comparison below supplies that quota explicitly.

### User-scope retention and exact logical-content export

--account-user-storage, requiring --account-segments 30, generates this fixed
cumulative claim through upsoscat:

```text
bs disc key0 6 30 key1 2 6 key2 2 6 key3 2 6
```

Granting only key3 while leaving key1/key2 zero is rejected with claim error.
The accepted fixture grants each non-temporary level two entries/six segments.
Relative to the temporary-only catalog, only bytes 902, 905, 908, 911, 914 and
917 change; the complete packed catalog is checked. Existing controls remain
unchanged. No historical image or binary is patched.

Preparation requires batchproof to be absent and exports the compact batchjob
as packed data. The retain job copies it, applies scope user and finishes.
After normal SOS removal, a new administrative FP looks up and exports the
copy with login/user/project bases explicitly 0/0, matching the test account.
Broad default inspection bases instead yield an unclassified scope marker
for the existing file; that output is never accepted as user-scope evidence.

The descriptor must show one segment on disc, user scope, key3 and bases 0/0.
The text export must equal all 53 input bytes. Each 768-byte binary export must
contain the same 53 bytes followed by end marker 25. Unused bytes after that
marker are not logical text and may differ. Both full packed hashes and the
text hash are recorded, with packed_logical_bytes_verified=54 and
sector_padding_compared=false. Physical-sector equality is not claimed.

Startup/job guards validate only the log phase beginning at read startsos/
sosboot, using a byte offset so CRLF normalization cannot corrupt the boundary.
The intentional pre-job unknown-file check is therefore separate; errors in
the actual startup/job phase remain fatal. The inspection FP must finish
normally, followed by healthy TEM/PRIMO/SOS listings.

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-retained \
  --probe-terminal --sos-disc1 --test-password test --batch-job \
  --batch-workload retain --account-segments 30 --account-user-storage
```

Fresh two-disk and reduced one-disk profiles pass. This proves user-scope
retention after child removal and exact logical-content export, not persistence
after simulator restart: the primary disk still uses a volatile overlay here.
Source/private base hashes remain unchanged. The separate interactive-session
checkpoint below adds login/logout; restart and protection remain independent.

### Interactive SOS and TEM session on a separate console

The --interactive-terminal go/run option requires the fixed terminal fixture
and cannot be mixed with batch/operator/password-diagnostic modes. It creates
only a private loopback Telnet endpoint on device 4, CLASSIC protocol, using
a dynamically chosen port. Device 2 remains local for independent system
listings. The endpoint is not installed into any shipped configuration.
The helper lives in dev/python/mips_terminal.py; its existing frontend test
transport has finite socket deadlines. Live tests require local socket access.

D662 section 2.2.1 distinguishes direct SOS go from TEM-spooled run. Both
forms pass as actual interactive sessions: attention selects sos, a fixed
wrong inline password is rejected, the correct dummy password enrolls test,
and FP executes message termprobe and lookup fp before finis. The run reply
must additionally contain exactly one from tem header; go must not contain it.
Message output is checked separately from its echo, lookup must identify the
18-segment FP area on disc, and normal SOS removal must have the correct
origin. The local console verifies no child remains and all services are healthy.

While awaiting terminal input, the observed child has zero stop count but
state stop under SOS, and SOS has zero stop count. A dedicated interactive
identity/bounds guard accepts this intermediary state only as input-wait
evidence; successful command execution and normal logout are still mandatory.
The batch running-state guard is unchanged. The separate password-question
form did not produce a usable remote exchange; this checkpoint uses the
documented inline pass form instead and publishes no password-prompt recipe.
All credentials are fixed disposable test data, not a secure network protocol.

```sh
python3 dev/python/probe_mips_ts.py build-protection/mips-interactive \
  --probe-terminal --sos-disc1 --account-segments 30 --account-user-storage \
  --interactive-terminal run
```

One direct-session run and two fresh TEM-session runs complete. terminal.log,
terminal-active.txt, terminal-finished.txt and terminal.json retain evidence;
probe.txt reflects the successful login instead of the startup-only disclaimer.
No monitor, CPU, device or shipped media change is needed. The global source
and private-base hash guards still apply. This verifies a single-terminal FP
session through TEM, not a multiterminal application or all TEM attributes.
Next gates: primary-disk restart persistence and actual PRIMO device/spool I/O;
strict protection remains a separate final assessment.

Thirty-five pure MIPS guards include password/response boundaries, SOS swap
allocation and disk
utility/mount/free-space checks. A separate integrated mkdisc test covers
CLI/interactive operation, completion, exact bytes and refusal of existing
paths, bad options and invalid destinations. Readline/80-column regression
also passes. No broad suite, sanitizer or PDF refresh is required here.
The retained-file guard rejects wrong document, size or scope, duplicate rows,
truncation, extra bytes and byte changes before and beyond the starter boundary.

## Disposable autostart test environment

The developer-only helper prepares an exclusive, ignored test directory:

```sh
python3 dev/python/prepare_mips_test.py build-mipsts/test --verify
```

The destination must not already exist. Preparation needs the existing
simulator and host-image builder (their paths can be overridden). Python is
needed only for preparation/verification, not for subsequent interactive use.
No standard profile, source image or binary-distribution contents are changed.

This profile has its own MONITOR, UNIT0, UNIT1, exchange directory and
RC4000.mipsts.cfg. Preparation writes only the new private disks; subsequent
runs use separate persistent overlays for both disks. It uses the compatibility
monitor, IX enabled, protection off and loopback Telnet port 4004. The account
and password are both test; no real credentials should be used.

The private startarea mounts device 16 and sets the fixed date 30 September
1986, avoiding the interactive date prompt. It reads starttem, startprimo
and startsos directly. Monitor s limits nested reads to two levels: reading
startmips from startarea would need a third level for each component and
produces `not allowed`. This is an input-stack limit, not a CPU protection
failure. The private startsos keeps its original claim on disc1.

Start from the repository root and connect from a second terminal:

```sh
./build/sim4000 -a -c build-mipsts/test/RC4000.mipsts.cfg
telnet 127.0.0.1 4004
```

After sos started, press Ctrl-A in Telnet, select sos, and enter
`run test pass test`. TEM opens FP; `message hello`, `lookup fp` and `finis`
provide a minimal session. After logging out, Ctrl-E in the local emulator
terminal and quit shut it down. Retain both overlays to resume guest changes.

Two complete starts pass the following checks: automatic TEM/PRIMO/SOS
startup, healthy process listings, rejected wrong password, TEM-mediated FP
login and commands, normal logout, removal of the child, and clean emulator
exit. An administrative FP creates a user-scope text file with bases 0/0 on
the first boot; exact export bytes match on both boots. This is not an
additional claim that an interactive account-created file has been tested
across restart. Prepared base hashes remain unchanged. Evidence is kept in
verification/boot1, verification/boot2 and verification/result.json.

Focused pure guards now total 37 tests. No broad suite or PDF build was run.
This environment is only for trying MIPS/TS; it is not a new default or
release profile. Strict protection and broader device/application coverage
remain separate work.

## Prepared distribution profile

The approved compatibility demonstration is now supplied separately from
the default system: src/disks/UNIT0.mipsts, src/disks/UNIT1.mipsts and
src/config/RC4000.mipsts.cfg. The bases match the prepared images before the
two-boot test, not its runtime overlays. No verification keepsake file or
overlay is promoted. src/disks/README.txt records sizes, identities and
provenance. The standard and historical media remain unchanged.

Ordinary builds copy all three profiles into a flat runtime directory;
protected source images likewise use src/monitor/MONITOR.protected and
src/disks/UNIT0.protected, documented in their parent READMEs. No profile
preparation or Python is required for ordinary builds or packaging. A single
exchange/ directory serves the profiles; use distinct host export names.

The developer helper prepares new candidates exclusively in a new workspace:

```sh
python3 dev/python/prepare_mips_profile.py build-mipsts/candidate
```

It uses the same guest preparation as the test fixture, retains preparation
logs, and exports only the clean base images and the maintained configuration.
It never overwrites shipped media. Guest timestamps can vary, so this is a
repeatable preparation procedure, not a bit-identical image-build claim.
Candidate promotion requires explicit review and updated identity checks.

The shipped configuration uses distinct UNIT0.mipsts.overlay and
UNIT1.mipsts.overlay sidecars, fixed date, loopback port 4004, and the one
test/test account. It additionally defines the ordinary unmounted reader
and punch; this is not a claim of a configured PRIMO peripheral service.
The User Manual has a separate MIPS/TS chapter. PDF regeneration is explicit.

The extracted binary archive passes the opt-in two-boot test using its own
executable, images and configuration:

```sh
python3 tests/python/test_mips_profile.py build-mipsts/flat-distribution-check \
  --runtime-root build-mipsts/archive.QhNz2w/sim4000
```

Its verification/result.json confirms automatic startup, TEM login/logout,
wrong-password rejection, exact persistent-file readback and unchanged bases.
The focused archive test separately covers all flat paths, file identities,
all three profile boots and atomic failure behavior. No broad suite, sanitizer
or PDF rebuild is needed for this packaging/documentation block.

The final focused checks pass 39 pure MIPS/TS guards and 42 monitor/profile
guards. PRIMO capability checks distinguish the two available console-backed
assignments from unknown console3/console4 devices; they do not infer a
third configured printer from generation text. The bounded transport guard
requires exact output and completed state, not merely successful definition.

## User-documentation follow-up

Source documentation for the supported unprotected MIPS/TS operating scope
is complete (100%). Completion means every accepted operating workflow has
prerequisites, a command context, expected results, safe cleanup and a stated
boundary. It does not require inventing a procedure for unsupported behavior.
The ready-made profile remains the single-account compatibility demonstration.

- [x] Files, startup order, fixed date, loopback endpoint and demo credentials.
- [x] SOS/FP/emulator distinction, go/run paths and normal login/logout.
- [x] Resource claims, compact administrative staging and guest visibility.
- [x] Actual remote file creation/retention, later login and full restart,
  exact independent exports, and deliberate cleanup; eight literal blocks
  match `test_mips_files.py`.
- [x] Compact batch input, password question and separately accepted inline
  password, real message/finis completion, unchanged input and cleanup; eight
  literal blocks match `test_mips_batch_manual.py`.
- [x] Real PRIMO-to-printer output, immediate append visibility, normal
  completion, detached-output recovery and bounded queue/waiter limits.
- [x] Idle TCP reconnect and fully echoed known partial input, including
  completion and Ctrl-H correction; private copies preserve both file scopes.
- [x] Status checks, non-destructive handling of otherwise unverified failure,
  stopped backup/reset and the difference between overlays and saved machines.
- [x] Optional larger accounts/parallel sessions distinguished from shipped
  assets; no internal contributor-document links in the User Manual.
- [x] Unsupported commands, uncertain input, in-flight I/O, hard terminal
  errors, crashes, broad account administration and protected-profile promotion
  remain explicit boundaries, not incomplete implied operating instructions.
- [x] Manufacturing instructions remain in `doc/MIPS-TS.md`; the User Manual
  describes operation of prepared media, not the image-generation machinery.
- [x] Task navigation, references, source structure and literal recipe guards.

RCSL 31-D662 and 31-D585 supply historical context, not runtime certification;
OCR spellings do not override verified syntax. No prepared `batchjob` is
claimed to be shipped. The explicitly requested PDF refresh now incorporates
the source completion, with changed operating pages and navigation reviewed.
Future source edits require a new explicit PDF build and pagination review;
source completion alone authorizes neither automatic regeneration,
distribution promotion nor a commit.
Future accepted features need their own documentation before release; they do
not keep the current supported-scope documentation perpetually at 95%.

## Standalone manual manufacturing guide

[doc/MIPS-TS.md](../MIPS-TS.md) now owns the complete manual disk-pair
manufacturing procedure, outside the User Manual. It starts with private
copies of the identified native inputs, uses integrated mkdisc and guest
kitlabel/kiton, creates the exact demo/terminal/resource catalog through
upsoscat, replaces startsos/startarea, freezes clean write-through bases,
and exercises a separate flat overlay-backed runtime. The profile-generation
helper is mentioned only as a concluding automated alternative.

The document-driven acceptance test independently executes the labelled
manual inputs/guest commands, rather than calling the profile generator.
It passes preparation, exact catalog/startup export checks, two automatic
boots, live TEM/Telnet login/logout with password rejection, retained-text
restart readback, and source/clean-base hash guards. No standard image,
configuration or PDF is changed, and candidate promotion is not automatic.
Evidence remains in an ignored exclusive build-mipsts-manual-check directory.

The requested manufacturing-guide coverage is 100% for the fixed demonstrated
recipe. Supported-operation source coverage is also complete under the
acceptance checklist above. The explicitly requested PDF pagination review
is complete for its last built snapshot, not subsequent source edits. The current
functional assessment is summarized above; manufacturing-guide acceptance alone
does not close protection or peripheral gates.

## Optional larger account

The private unprotected two-account fixture is accepted for administrative
input staging, remote allocation/copy and a small Algol compile/run workload.
This is not a new shipped profile or a general account-administration tool.
The original `test` process record and terminal mapping remain byte-identical;
the additional password is disposable demonstration data, not a secret.

Developer host shell (a new exclusive destination is required):

```sh
python3 dev/python/probe_mips_account.py build-optional-account \
  --simulator build/sim4000 --verify
```

The script uses guest `upsoscat`, not a host catalog writer. Alongside the
unchanged demo description it inserts this guest catalog input:

```text
proc develop
mins 100000
maxs 150000
pass "develop"
bs disc key0 20 500 key1 4 100 key2 4 100 key3 4 100
```

Keep the existing `maxp 4` and `term console1 "1" "test" 60 2` declarations.
This adds an alternative account on the existing terminal, not a second
simultaneous terminal. The actual child allocation is 150016 halfwords. Guest
claims are rounded to disk slices: 500 temporary segments become 501, and
100 retained segments become 102. The first attempted retained allowance of
20 was inadequate for the 24-segment compiled program plus its source; an
isolated increase of key3 was rejected by the guest updater. The accepted
fixture uses 100 for each of keys 1--3, with four entries per key.

The checked workflow deliberately separates contexts:

1. **Host shell:** create private media and exchange inputs; the large text
   is 34000 host bytes and the small source is the manual's `hello.alg`.
2. **Local Guest-s:** allocate an administrative FP with
   `all seedprep base 0 0 login 0 0 user 0 0 project 0 0 size 150000 run`.
3. **Administrative FP:** import each input with `fromhost`, compact with
   `copy`, and retain `devseed` and `bigseed` with `scope user`. These 0/0
   intervals match the optional account's visibility. End with `finis`.
4. **Remote SOS:** reject `run develop pass wrong`, then accept
   `run develop pass develop`.
5. **Remote FP:** `bigsrc=copy bigseed` allocates 45 temporary segments,
   exceeding the original demo claim; `clear temp bigsrc` removes that copy.
   Then execute:

   ```text
   devsrc=copy devseed
   devprog=algol devsrc connect.no
   devkeep=copy devsrc
   scope user devkeep devprog
   devprog
   ```

   Expect `algol end`, one `Hello from Algol` and a normal FP `end`.
   The retained program occupies 24 segments; the source occupies one.
6. **Administrative FP:** export `devkeep` with `tohost` and compare its
   logical bytes with `hello.alg`. The staging file `bigseed` also exports
   exactly to the original host text. No direct SOS export is claimed.
7. **Remote FP:** `finis` must remove the child normally. Restart the emulator
   using the same separate persistent overlays, log in again and run
   `devprog` without recompiling. Repeat the source export and byte comparison.
8. **Remote FP:** `clear user devkeep devprog`, followed by `lookup` of each,
   must report both unknown. **Administrative FP:** remove `devseed bigseed`
   with `clear user` and verify all four names absent. These commands delete
   guest files: they are restricted to exclusive disposable test media.

The probe also requires healthy TEM/PRIMO/SOS listings, unchanged base hashes,
unchanged demo login before and after the workload, and exact catalog bytes
including padding. Runtime overlays and verification outputs stay beneath
the private destination; no source media are promoted or modified.

### Host-transfer boundary

Increasing account claims does **not** make `fromhost` usable directly in the
SOS job. The explicit negative control `denied=fromhost devinput` reports
`reserve host-file process result 2`, then leaves an empty temporary entry
which the probe deletes. This unstarred diagnostic must never count as
success. `verify_fp_reply` rejects it outside the deliberate negative control.

The maintained monitor build inserts the kind-94 endpoint into the normal
user-mask check before reservation (`sim4000.mops`, inserted `h51`, and
`montxt3`, `g8`). Its initial descriptor grants access to `s`; the checked SOS
child initially cannot reserve it. This is a guest device-access boundary,
not evidence of a CPU protection failure or insufficient free disk.

RCSL 31-D662 section 2.2.4 (printed pages 18--19, PDF pages 24--25)
documents the regular SOS `include <deviceno>` command. Section 2.3
explicitly separates device access from catalog/job-creation resources.
The command grants access to **all currently enrolled jobs**, not just the
account being exercised. Only one job can reserve the device at a time.
This is not a permanent account attribute or a new host permission domain.

The optional `--include-host` variant now checks this precise path:

```sh
python3 dev/python/probe_mips_account.py build-optional-host-account \
  --simulator build/sim4000 --verify --include-host
```

After the remote account has enrolled, the **local console operator** presses
Escape, waits for `att` and the bell, enters `sos`, and waits for `>` and the
bell. In this **SOS command context**, not Guest-s, FP or emulator CLI, enter:

```text
include 63
```

Expect the timestamped `sos: ready` line; this response has no trailing FP
bell. Any `include not ok` or other diagnostic is failure. Return to the
already running **remote FP** and use:

```text
bigsrc=fromhost biginput
lookup bigsrc
bigdirect=tohost bigsrc
clear temp bigsrc
devsrc=fromhost devinput
devprog=algol devsrc connect.no
devkeep=copy devsrc
scope user devkeep devprog
devprog
direct1=tohost devkeep
```

The 34000-byte large input allocates 72 segments; even the short source
initially allocates 36 before `copy` compacts it. Both exceed the demo's
30-segment claim. Independent host comparisons require both direct exports
to match their input bytes exactly. The source compiles to the same
24-segment program, which prints the expected greeting and ends normally.
`clear temp bigsrc` deletes only the disposable guest import, not the host
input. Host output names must be new and dedicated to the private experiment.

After a normal logout and full emulator restart with the same overlays,
the probe logs in again and requires reservation result 2 **before** repeating
`include 63`. The retained program then runs without recompilation and
`direct2=tohost devkeep` matches the original source. Normal cleanup, healthy
subsystems, unchanged media and original demo login remain mandatory.
Thus inclusion is deliberate runtime administration for enrolled jobs, not a
startup script change or permission saved in an overlay. Later logins and
simultaneous device contention are not covered by this fixture.

No monitor/emulator rights check is weakened, no device is renamed, and no
shipped configuration, image or demo account changes. Broad compiler coverage,
concurrent host transfers and automatic inclusion policy remain unaccepted.

Pure account guards cover the exact two-account layout, unchanged demo fields,
resource/terminal mutations, allocation/parent bounds, unstarred failures and
the explicit include command/completion boundary.
They run as `sim4000_mips_account_guards` only with developer tests enabled.
The live two-boot probe is explicitly opt-in and needs local socket permission.
The bounded optional-account and explicit host-inclusion checkpoint is
approximately 100% complete. Its operating scope and limits are included in
the completed source documentation; future features remain in the roadmap.

## Simultaneous SOS sessions

The opt-in two-terminal fixture extends the accepted optional account, not
the shipped single-account profile. It uses the maintained compatibility
monitor, protection off, private clean disk copies and separate persistent
runtime overlays. Invoke from the developer host shell with a new directory:

```sh
python3 dev/python/probe_mips_sessions.py build-two-sessions \
  --simulator build/sim4000
```

The guest-generated catalog preserves the entire original `test` record.
Only the additional `develop` record receives distinct catalog intervals:

```text
proc develop
stdbase 100 100
userbase 100 100
maxbase 0 100
mins 100000
maxs 150000
pass "develop"
bs disc key0 20 500 key1 4 100 key2 4 100 key3 4 100
term console1 "1" "test" 60 2
term terminal1 "2" "develop" 60 2
```

This replaces the `develop` stanza of the one-terminal fixture; it is not
appended as a duplicate process. Keep `maxp 4` and the original demo stanza.
Both terminal records follow the process options. The extra packed record
and all changed base words are checked against a fixed independent layout;
unchanged demo fields, directory slots and padding remain mandatory.

The private `startsos` changes exactly one `internal 1` clause to `internal 2`.
The SOS parent retains its 300000-halfword area, other claims and disc1 swap
resources. Two configured listeners alone cannot create two usable jobs:
the initial equal-standard-base experiment was rejected with `user conflict`,
and separate bases with the original single-child claim produced
`process creation not ok`. These are expected guest policy/resource limits,
not reasons to bypass the monitor. RCSL 31-D662's error appendix explicitly
identifies identical standard bases as a cause of `user conflict`.

The runtime config retains the existing device-4 console and adds:

```text
DEFINE RC851 DEVNO=5 INTNO=16,15 PROTOCOL=CLASSIC
TELNET DEVNO=5 ADDRESS=127.0.0.1 PORT=<second-port> ATTENTION=CTRL-A
```

The probe substitutes a distinct available numeric loopback port; the
placeholder above is not literal configuration syntax. Device 5 corresponds
to the maintained monitor's `terminal1` descriptor. RC851 startup screen
controls and Telnet negotiation are retained as raw evidence; binary IAC
bytes are never decoded as ASCII. The first endpoint uses console device 4.

An administrative FP is allocated through **local Guest-s** with:

```text
all seedprep base 0 0 login 0 0 user 0 0 project 0 100 size 150000 run
```

It imports and compacts `hello.alg` as `devseed` and a larger text as
`bigseed`, then uses **FP** `scope project devseed bigseed`. Their numeric
catalog bounds are 0/100. The unchanged small seed is readable by both jobs;
FP describes it as `system` from the demo's 0/0 context and `project` from
the larger account. The probe verifies key 3 and numeric 0/100 bounds, not
merely that context-relative label. This does not make it an installed system
utility or justify promoting the experimental files.

The checked operating sequence is:

1. Connect to both loopback ports, select SOS with Ctrl-A and `sos`, reject
   a wrong password at each endpoint, then use **remote SOS**
   `run test pass test` and `run develop pass develop` respectively.
2. Require both enrolled child descriptors under the same healthy SOS parent.
   The reported job sizes are 30208 and 150016 halfwords. Each fits inside
   the SOS area; stop counts remain zero. **Do not require disjoint bounds:**
   SOS swaps the jobs through a reused slot, and both descriptors may report
   the same start address and key. These are simultaneous logical sessions,
   not proof of simultaneous residency or protected memory isolation.
3. In each **remote FP**, use `claim`, a distinct short `message` marker,
   and `joint=copy devseed`. Both temporary entries must exist independently.
   `clear temp joint` in `test` must leave the larger account's entry intact.
4. **Demo FP:** `too=set 33 disc` must report `no resources` and leave no
   `too` entry. **Develop FP:** `room=set 33 disc` must create a 33-segment
   temporary entry normally. Remove it and the larger job's `joint` entry.
   The independent claim reports retain the original demo's 30 temporary
   segments versus the larger account's rounded 501.
5. **Demo FP:** copy the seed as `shared`, then `scope user shared`.
   **Develop FP:** `lookup shared` must report unknown; create and retain
   `ownfile` with `scope user ownfile`. It must in turn be unknown to `test`.
   Distinct user ranges 0/0 and 100/100 are not the equal-base visibility
   arrangement used by the earlier administrative staging fixture.
6. Log out `test` with **remote FP** `finis`. Its child must disappear while
   `develop` continues to execute commands and sees its own retained file.
   Log `test` back in on the same connection, require both jobs again, and
   verify its temporary entry is gone while its user file remains.
7. Delete each account's own retained file, checking it is absent. Log out
   `develop`; the demo must remain usable. Log out the demo and require all
   child descriptors removed, with TEM/PRIMO/SOS still healthy.
8. An administrative FP independently exports `devseed` for exact host byte
   comparison, then uses `clear project devseed bigseed` in the matching
   0/100 project context. This is destructive guest cleanup restricted to
   the exclusive experiment; do not run it on an existing installation.

Commands at the emulator CLI are limited to checking protection off and
normal shutdown. No memory deposits, monitor modifications, permission bypass
or standard-image updates are used. Before creating files the probe requires
their names absent. Prepared and runtime base hashes must remain unchanged;
only private overlays receive runtime writes. Raw per-terminal transcripts,
both claim reports, the simultaneous listing and `sessions/result.json`
record the bounded result. No terminal or image is added to the distribution.

This two-session checkpoint is approximately 100% complete. Eleven pure
account/session guards include distinct catalog bases, terminal bytes,
process identity/removal, valid swapping overlap, binary Telnet negotiation,
numeric seed scope and the earlier host-inclusion checks. Live verification
remains opt-in, not part of a normal build. Abrupt disconnect, concurrent
HOSTFILES reservations, application-level multiterminal attachment, longer
workloads and protected MIPS/TS remain separate work.

## Idle FP TCP reconnection

The shipped unprotected MIPS/TS profile passes two successive TCP connection
closures without guest `finis`, followed by direct resumption of the same
waiting FP. Repeated verification uses fresh exclusive copies of the shipped
monitor, disk pair and config, with a substituted loopback port and their
normal separate persistent overlays:

```sh
python3 dev/python/probe_mips_reconnect.py build-idle-reconnect \
  --simulator build/sim4000
```

An administrative FP imports the manual's `hello.alg`, compacts it into a
one-segment `recseed`, and retains it at 0/0. The remote `test` job creates a
temporary `reclost` and retained `reckeep` from that seed. At a completed FP
prompt, the probe closes the TCP client without attention, SOS `out`, guest
logout or process removal. It requires the unchanged child listing and
healthy TEM/PRIMO/SOS, then confirms `show telnet` reports `listening` with
the expected connection counter and no input/output drops.

While disconnected, an administrative export of `reckeep` must match the
original host bytes. A new client connects to the same device and completes
Telnet negotiation. It sends `message resumed` directly to FP, without
Ctrl-A, process selection, account/password or a new SOS enrollment. The exact
message and FP bell establish actual resumption, not merely a reachable port.
Both descriptors retain their scopes. A compact retained proof copied from
the temporary file is independently exported and compared byte for byte,
then deleted. This repeats for a second close/reconnect cycle.

Normal `finis`, fresh login, retained-file lookup, explicit removal and final
administrative cleanup follow. Source and private base hashes must remain
unchanged even on failure. Raw connection transcripts, disconnected listings,
exact exports and `result.json` remain private evidence. Three pure endpoint
guards cover device/address/state/counters, zero-drop requirements and the
CLI's 80-column wrapping; the live probe remains opt-in.

`disconnect_client` in the host Telnet implementation closes the socket and
clears transport output/parser state; it does not synthesize a guest terminal
hard error or remove the guest job. Consequently, TCP reconnection is not a
fresh guest authentication boundary. Keep listeners on loopback and treat
access by other local clients as access to the current guest session.
Previously queued/dropped output is not reconstructed.

The `--partial-input` extension repeats the same private-copy and exact-export
checks with two fully echoed input prefixes. Before the first TCP close it
sends `message par` without Return and waits for the complete echo; the new
connection sends `tial` plus Return and must produce exactly `partial`.
Before the second close it sends and observes `message wrong`; after reconnect
five Ctrl-H erase the known suffix and `right` plus Return must produce exactly
`right`. No full replacement line is blindly appended to the existing input.
Both cycles preserve the same job and file bytes and finish normal cleanup.

```sh
python3 dev/python/probe_mips_reconnect.py build-partial-reconnect \
  --simulator build/sim4000 --partial-input
```

Fresh-pair repetition passes. Five pure reconnect guards now include exact
completion messages and bindings to the three marked manual input blocks.
The user instruction distinguishes a known echoed prefix from bytes that may
never have reached the guest. Backspace correction is a guest input operation,
not host queue disposal, a new login or a process reset.

This bounded reconnect checkpoint is approximately 100% complete. It
does not establish recovery from unknown/unacknowledged input, commands still executing,
output in flight, actual TEM/terminal hard errors, connection timeouts, emulator
termination, crashes or protected/multiple-account profiles. In particular,
the historical SOS rule for a hard error on a primary terminal must not be
confused with this host-only TCP close. These cases remain separate roadmap
items; no stop/start/break/kill or automatic credential-recovery recipe is
inferred from this result.

## Acceptance and stop conditions

Keep compatibility and strict-protection results distinct. Assert finite
command deadlines and source/copy hashes. A missing user catalog, terminal
protocol mismatch, software probe or resource shortage must be diagnosed
before changing the emulator. Unknown initialization commands must not be
tried against shipped media. Account setup uses disposable test data only;
no personal credentials belong in the repository or operating examples.

Retain a concise supported recipe only after repeating it on fresh private
media. Inventory, successful process creation, interactive login and actual
workload completion are separate evidence levels.
