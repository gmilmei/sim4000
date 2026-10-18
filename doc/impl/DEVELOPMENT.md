# Development Guide

Commands start at the repository root unless stated otherwise. Normal builds
produce the emulator and copy ready-made media; they build/run no tests and
require neither Python nor TeX. Runtime operation is described in the
[User Manual](../usermanual/README.md).

## Source and tooling layout

| Directory | Responsibility |
| --- | --- |
| src/, include/ | Native runtime C17 implementation and public core interfaces. |
| src/config/, src/monitor/, src/disks/ | Bundled runtime configuration and media. |
| src/guest/, monsrc/ | Guest utilities and maintained monitor build sources. |
| tests/c/ | Native unit/integration checks and the FP probe. |
| tests/python/ | Optional host orchestration and independent oracle checks. |
| tests/cmake/ | Python-free binary-package verification. |
| tests/fixtures/ | Shared guest input/reference data. |
| dev/c/ | Bounded guest-session driver for developer verification. |
| dev/python/ | Media regeneration, archive analysis and source/trace checks. |
| dev/cmake/ | Explicit binary-distribution packager. |
| doc/usermanual/examples/ | Standalone tutorial inputs for runtime users. |

C/Python source files are non-executable; compile C and invoke scripts with
python3. Test adapters and developer tools are not runtime-distribution tools.
Project-specific user utilities belong in C.

The optional Python modules have distinct purposes: make_unit0_copy verifies
pristine fixture copies, make_host_image regenerates guest utilities,
make_standard_monitor builds/validates the DFC403 monitor, rebuild_monitor
drives guest SLANG, inspect_monitor_tape analyzes a pinned source archive,
check_manual_sources checks TeX structure, and trace_compare compares
architectural traces. Shared copy/verification routines are imported rather
than duplicated. None is needed for ordinary execution.

The separate [MIPS/TS Manufacturing Guide](../MIPS-TS.md) gives a complete
manual route using only the runtime executable and guest tools. It does not
need the developer builder or Python. To verify its exact labelled command
and input blocks independently of the profile generator, run explicitly:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tests/python/test_mips_manual.py build-mipsts-manual-check
```

The destination must not exist. This opt-in test needs PTYs and loopback port
4004, writes only exclusive private copies, retains its preparation/boot logs
and result.json, and runs two boots with actual Telnet login/logout and exact
retained-file readback. It is not part of a normal build. Manufacturing stays
outside the User Manual; that manual covers the completed environment.

## Build and focused checks

### Maintained RC610 printer

Every maintained monitor is built from monsrc/sim4000.mops and includes
printer/dev7/IRQ10. Historical hjmops* are not build alternatives. Ordinary
builds use the supplied printer-capable images and C backend, without Python.
All configs automatically append to their own printer text files.

Focused checks (tests are opt-in, never part of an ordinary build):

```sh
cmake -S . -B build-printer -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-printer --target sim4000 sim4000_tests \
  sim4000_rc610_tests sim4000_rc610_guest_tests -j4
build-printer/sim4000_tests --quick
ctest --test-dir build-printer -R '^sim4000_rc610' --output-on-failure
```

Native vectors cover status, modifiers, graphics, width, controls, errors and
exclusive/append policies, including independent file visibility before close.
Guest checks print through compatibility/protected/DFC403 monitors. The source
check compiles a guest Algol program, prints cpuregtest and compares every host
output byte against a separate guest export; it does not run cpuregtest.
sim4000_rc610_source_protected repeats the same example with the matching
protected profile, rcin/rcout and actual keyed task execution during printing.
To run only the two source-print checks, select
-R '^sim4000_rc610_source(_protected)?$'. Both verify normal FP exit and
subsequent s operation, unchanged base media and fixture/tutorial identity.
The CLI check verifies automatic output, per-print visibility, attach/detach,
reset preservation and two-boot append in all three direct monitor profiles.
Each guest test uses a new output and volatile disk writes.

Developer-only monitor generation keeps candidates private until deliberately
promoted. Enable SIM4000_BUILD_DEVELOPER_TOOLS first, then use new destinations:

```sh
python3 dev/python/rebuild_monitor.py build-printer/compat \
  --builder build-printer/sim4000_build_host_image --verify-boot
python3 dev/python/rebuild_monitor.py build-printer/protected \
  --builder build-printer/sim4000_build_host_image --protected --verify-boot
python3 dev/python/rebuild_monitor.py build-printer/dfc403 \
  --builder build-printer/sim4000_build_host_image --dfc403 --verify-boot
```

All consume the same maintained generation file. --dfc403 selects the guarded
hardware disk options; --protected selects sim4000-protected.mops, whose guest
editor restores e22 key setting in a temporary guest file before invoking the
shared generation file. Python does not patch the Protection source.
The protected wrapper compacts padded imports in the guest so its temporary
source/log files fit the unchanged 2000-segment build claim.
Protected verification uses UNIT0.protected and its older rcin/rcout binding,
not incompatible newer fromhost/tohost programs. No historical source or base
disk is modified. The standard-monitor target now builds the DFC403 variant
from these maintained sources rather than exporting disk-resident stdmonitor.

[RC610](RC610.md) records current artifacts, exact mapping and scope limits.
The optional live inventory probe requires the installed printer by default;
--prepare-sources copies the already printer-enabled inputs into a new private
tree, without adding another option. It checks live table pointers, known
reader/punch controls and unchanged source hashes. Neither a descriptor nor
an empty file establishes actual printing. Sequential PRIMO-to-RC610 spooling
now has a separate acceptance test:

```sh
python3 tests/python/test_mips_printer.py build-test/mips-printer \
  --offline --queued --simulator build-test/sim4000
```

The destination must not exist. It retains copied bases, independent overlays,
raw Telnet evidence, console logs, exact job output and result.json. The test
needs a loopback listener, but does not modify shipped disks or startup files.
With BUILD_TESTING enabled, the same test is registered as sim4000_mips_printer
and uses a temporary private workspace. It is not run by an ordinary build.
It checks the shipped primo_print.alg example, repeated jobs, two boots and
TEM/SOS login/logout. --offline adds a detached-output transport with explicit
PRIMO waiting/start recovery. --queued adds two independent outstanding
transports, exact FIFO output, client cleanup and a same-instance followup.
The second blocking wait is refused with reply 3; the test explicitly uses
bounded status queries instead of claiming both waits succeed. Registered
CTest includes both options. Queue exhaustion, partial host-write errors and
mid-job paper recovery remain open.

For the separate wait-resource diagnostic, add --primo-buffers 10 or 11 to
--queued, using a new destination for each run. Before printing, it exports
and checks the original startprimo, exports primotrim and packed bprimo, then
removes/recreates only the unused private live PRIMO with the requested claim.
It records hashes and settings in boot1/primo-resources.json. No startup file,
trimming source or binary is patched. Eleven buffers do not remove the second
wait's reply 3; the reference and primotrim's waitops=1 explain the likely
generated limit. The registered test retains the unchanged startup profile.
PDF regeneration remains explicit.

### Protected MIPS/TS startup assessment

For isolated protected MIPS/TS startup assessment, use separate new workspaces:

```sh
python3 dev/python/probe_mips_protection.py build-test/mips-protected-tem \
  --component tem --simulator build-test/sim4000
python3 dev/python/probe_mips_protection.py build-test/mips-protected-primo \
  --component primo --simulator build-test/sim4000
python3 dev/python/probe_mips_protection.py build-test/sos-catalog \
  --prepare-sos-catalog --simulator build-test/sim4000
python3 dev/python/probe_mips_protection.py build-test/mips-protected-sos \
  --component sos --catalog-from build-test/sos-catalog \
  --simulator build-test/sim4000
python3 tests/python/test_mips_protection.py
```

The probe copies the existing protected monitor/disk/config and uses volatile
disk writes. It needs a local PTY, not a Telnet listener. It starts exactly one
subsystem, advances a bounded million steps and retains session.log, listing.txt,
state.bin and result.json. PRIMO also gets a display/unknown-command/display
operator check. Core-key observations and reciprocal process links distinguish
real monitor descriptors from subsystem copies. It verifies startup banners,
protection/IX and unchanged source/base hashes, but explicitly does not certify
functional protected MIPS/TS. No artifacts are promoted. See
[MIPS/TS](MIPS_TS_PLAN.md#current-protected-assessment-checkpoint) for scope.

SOS preparation is explicitly two-stage. The compatibility-only factory runs
guest upsoscat for an empty maxp-4 catalog and records catalog-result.json plus
exchange/catalogbin. The protected probe requires exact known bytes and matching
factory provenance before creating its runtime. It creates a new blank UNIT1,
labels/mounts disc1 through guest tools, imports with rcin and resizes the new
catalog area to one segment with set. Exact rcout readback is required before
retention. A separate psosboot changes only the SOS permanent-claim document;
the exported original startsos must remain unchanged. swpsos must be 1722
segments on disc1. All runtime disk writes are volatile and discarded on exit;
these are diagnostics, not a persistent protected MIPS/TS profile.

Running --component sos without --catalog-from attempts the strict protected
updater path instead. The installed upsoscat stops with break 0 in tofrom1;
preparation-failure.json and preparation-failed.bin preserve that failure.
There is no automatic fallback or mode change. The verified catalog import
isolates this preparation limitation from actual SOS startup.

For the separate bounded protected terminal assessment, manufacture the known
demo catalog and import it into a new workspace (host shell):

```sh
python3 dev/python/probe_mips_protection.py build/protected-demo-catalog \
  --prepare-sos-catalog --demo-account --simulator build/sim4000
python3 dev/python/probe_mips_protection.py build/protected-terminal \
  --component combined --catalog-from build/protected-demo-catalog \
  --simulator build/sim4000
```

This opens an ephemeral loopback-only Telnet port and uses volatile disk writes.
The two-segment catalog is imported/resized/read back exactly; its manifest must
declare the known demo fixture. Checks cover wrong-password rejection, test/test
login through TEM, FP message/lookup, normal logout and child removal. Raw Telnet
bytes, active/finished listings, active core and descriptor/PR/PK/code-key JSON
are retained. A stopped-by-parent child may have the SOS parent's current code
key at its saved PC; this is recorded without treating a saved PC as a live
instruction fetch. No protected PRIMO transport, restart persistence or reusable
release image is certified by this terminal assessment.

Add --printer to --component combined for the bounded protected spool check,
using another new destination. It signs up printer/kind 14, imports the shipped
primo_print.alg via rcin, compiles with installed Algol and submits two jobs
from a local 150000-halfword FP process. Completion must report result/reply 0,
state 5/cause -1; printer.protected.txt must equal both payloads immediately,
without a sleep, flush command or shutdown. No payload may leak onto the console.
Signoff and the subsequent Telnet login/commands/logout are also checked.
printer-result.json records transport IDs, lengths and source/output hashes;
job logs retain completion replies. Source/base hash guards remain in force.
This establishes local protected PRIMO printing, not remote user submission,
persistent file operations, restart or general fault recovery.

For the protected SOS file/restart assessment, use --restart with --component
combined and --catalog-from, again with a new destination. Only this explicit
mode adds persistent sidecars UNIT0.protected.overlay and UNIT1.protected.overlay
to the private configuration. Both bases remain read-only and hash-checked.
Keep these diagnostic sidecars together with their exact private bases; do not
copy them into a release profile or reuse them with another disk pair.

The first boot prepares the catalog/second disk once, then an administrative
FP creates a small shared seed in base interval 0/0. The actual remote test/test
session copies the seed to keepsake, applies scope user, looks up the descriptor
and displays its text using c=copy. This fits the existing demo quota and avoids
expanding it merely for rcin's 36-segment import allocation. A separate matching-
base administrative FP exports keepsake through rcout for an exact host-byte
comparison. It does not create or retain keepsake on the user's behalf.

After normal logout and full emulator exit, a new emulator process opens the
same private bases/sidecars. It initializes the same historical date, mounts
disc1 with kiton (never kitlabel), and explicitly starts TEM/PRIMO/psosboot.
There is no second catalog manufacture/import or file creation. A fresh Telnet
login must find the one-segment user-scope keepsake and display the exact text;
a new rcout export must again equal the independent host fixture. The second
boot records CPU protection/IX, both persistent media identities and its own
session/Telnet transcripts under boot2. restart-result.json is written only
after both boots, normal logouts/exits and base hash guards succeed. This tests
normal-exit persistence, not autostart, crash/power-loss recovery or isolation.

Use --isolation with --component combined and the verified demo catalog in a
separate new workspace for the bounded forbidden-store assessment. It cannot
be combined with --restart. Guest SLANG assembles the private badstore program;
the remote SOS child executes it, attempting RS w2 x1 with W1=8192 and W2=1.
A diagnostic execution breakpoint at the installed FP's predicted load origin
(child first + 1536, RS at offset 10) must validate live operands, task mode,
PR=e7 and Key-3 code. FP may visit that address before loading the program;
at most 64 visits are allowed. The exact instruction body/literal target must
also match the pre-fault core. No deposit, register/key editing or mode switch
is used. Both reserved FP words are allowed to differ after loading.

The target must be Key 0 and not already contain the attempted value. After
continuing, exactly one break 0 at the next IC and normal SOS removal after
break are required; the target word must be unchanged. The helper then checks
healthy TEM/PRIMO/SOS, a fresh login with message/lookup and normal logout, and
PRIMO display/unknown-command/display recovery. fault-instruction.bin captures
the actual loaded program before the store. fault-core.bin is the post-removal
diagnostic; SOS can already have reclaimed that interval. fault-response.txt,
fault-context.json, sentinel-before/after.bin and isolation-result.json retain
the narrow evidence. This does not certify arbitrary process isolation or a
security boundary, and no candidate image is promoted.

The installed catalog updater's protection failure has a separate bounded
instruction-level negative probe (host shell, new destination):

```sh
cmake --build build-test --target sim4000_build_host_image
python3 dev/python/probe_protected_upsoscat.py \
  build-test/sim4000_build_host_image build-test/upsoscat-fault
```

This developer helper uses the maintained protected monitor/disk with volatile
writes, no Telnet listener and no changes to shipped media. Guest FP imports
maxp 4 through rcin and exports the installed upsoscat binary/descriptor, then
runs soscat=upsoscat catspec. Existing CPU diagnostic instrumentation records
the actual IRQ-0 task PL address 34, raw 730022, PC/saved-next IC, PR bf/Key 1
and registers. Ordinary JD/service IRQ-0 entries are not mistaken for the PL
fault. The linked binary must have 114 segments, tofrom1/tofrom2 and the probe
sequence inside tofrom1. FP is restarted through Guest-s proc hostcheck start;
rcout readback must equal the original input exactly. result.json records the
known incompatibility, never a successful catalog build. This independently
localizes the same PL-address-34 mechanism as cmp; neither tool is repaired by
the probe or by rebinding a catalog entry. Source-based rebuilding, if pursued,
requires matching source and a separately validated old runtime first.

### Experimental protected MIPS/TS candidate

This is retained developer work. Normal MIPS/TS remains unprotected; candidate
promotion and build/distribution integration are explicitly deferred. Do not
replace the shipped compatibility profile with these experimental artifacts.

Prepare and optionally verify a new private candidate (host shell):

```sh
python3 dev/python/prepare_protected_mips_profile.py \
  build-test/protected-mips-candidate --simulator build-test/sim4000 --verify
```

Omit --verify for generation alone. The tool requires an exclusive destination,
and needs a PTY plus loopback sockets for verification. It does not update source
assets, ordinary build outputs or distribution manifests. The candidate contains
MONITOR.protected, UNIT0.mipsts-protected, UNIT1.mipsts-protected and
RC4000.mipsts-protected.cfg. Catalog manufacture is an explicit separate
compatibility guest stage; the exact known catalog and its factory manifest are
validated before import. The protected preparation stage writes only new private
disk copies, verifies exported startsos/startarea and removes interim psosboot.

startarea mounts device 16, sets the fixed 1986-09-30 date and directly reads the
three component scripts, preserving the two-file FP input limit. startsos keeps
its original 2500/200 permanent claim on disc1. Both scripts are retained with
user scope. Clean bases are frozen before verification; manifest.json records
input/artifact hashes and the catalog's compatibility provenance.

--verify copies only the four candidate artifacts into verification/, selects a
free loopback port there and creates independent persistent overlays. Across two
full automatic starts it checks protection/IX and exact media identities, wrong-
password rejection/login/FP operations/logout, a remote-created retained file
with exact local exports, and two sequential real PRIMO-to-RC610 jobs each boot.
Output must be exact and visible immediately, append across boots, and survive
normal exits. All candidate and runtime base hashes are checked. Each boot has
separate transcripts and printer manifests; verification/result.json is emitted
only after both boots pass. Verification overlays, seed/retained files and spool
output are not copied back into the candidate. This is not crash recovery or
automatic promotion to a supported/shipped profile.

### Unprotected MIPS/TS remote-file recipe

The User Manual's retained-file exercise targets the shipped compatibility
MIPS/TS profile, not the deferred protected candidate. Run its focused live check
explicitly (host shell, exclusive private workspace):

```sh
python3 tests/python/test_mips_files.py build-test/mips-user-files \
  --simulator build-test/sim4000
```

It copies MONITOR, the two shipped MIPS/TS bases and their config, selects a
free loopback port and uses the profile's independent persistent overlays.
An administrative FP in explicit base/login/user/project 0/0 intervals checks
that exercise names are absent, imports text with fromhost, compacts/retains
only keepseed and exits. The actual remote test child creates/retains keepsake.
Four normal Telnet sessions cover creation, logout/login readback, readback
after full automatic restart and deliberate remote cleanup. Administrative
tohost exports on both boots must match the independent input exactly; the
seed is then removed and both guest names must be unknown. Source/private-base
hashes stay unchanged. No abrupt-disconnect or crash-recovery claim is made.

The manual's eight mips-files command blocks are statically bound to the checked
dialogue by manual_recipe; its source-only guard runs without sockets. The live
test is opt-in, not part of ordinary build/test execution. It produces per-boot
and per-login transcripts plus result.json. No image generation, account-quota
change, guest rebuild or PDF refresh is required for this recipe.

### Unprotected MIPS/TS batch recipe

The User Manual now has a complete local-console message/finis jobfile exercise,
not merely historical syntax. Verify it explicitly on private shipped-profile
copies (host shell, exclusive destination):

```sh
python3 tests/python/test_mips_batch_manual.py build-test/mips-batch-manual \
  --simulator build-test/sim4000
python3 tests/python/test_mips_ts.py
```

The live test needs a PTY and a loopback socket for its final interactive health
check. It starts the unprotected profile, checks the jobfile name is absent,
imports/compacts the message/finis text in a matching-interval administrative FP
and retains a one-segment user-scope batchjob. It submits from the original
local console through SOS twice: first answering the password question, then
using the inline dummy password. Actual output, ordered enrollment/removal,
absence of the child and healthy resident services are required each time.
A later Telnet login/FP/logout must work. An exact tohost export proves the
jobfile was unchanged before explicit guest cleanup. Base/source hashes remain
unchanged; all guest writes use private overlays.

Eight marked manual blocks are bound to the checked dialogue. Pure guards
reject wrong/missing password context/echo, duplicate prompts and an unexpected
question for inline submission, alongside existing output/removal guards.
The live recipe is opt-in and generates no PDF or release image. It does not
establish remote batch submission, concurrent-account scheduling, invisible
password input, arbitrary jobfiles or disconnect/crash recovery.

### Normal developer verification

Runtime build requirements are a C17 compiler, CMake and GNU Readline development
files. Rebuilding monitor/media artifacts and developer integration checks
additionally need Python; ordinary builds and binary packaging use the supplied
prebuilt artifacts and do not. PTY/Telnet tests
need pseudoterminals and local loopback sockets.

```sh
cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DSIM4000_REQUIRE_PYTHON_TESTS=ON
cmake --build build-test
./build-test/sim4000_tests --quick
ctest --test-dir build-test -R '^sim4000_operator_cli$' --output-on-failure
ctest --test-dir build-test -R '^sim4000_floating_oracle$' --output-on-failure
```

BUILD_TESTING and SIM4000_BUILD_DEVELOPER_TOOLS default to OFF. Existing CMake
caches retain previous selections; use a separate build tree or reset them
explicitly. SIM4000_REQUIRE_PYTHON_TESTS rejects a disabled test build or missing
Python instead of silently reducing coverage. C-only verification can disable
Python discovery with CMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE; the CMake package
test remains available.

Choose a test by its registered name, not a machine-local transcript. Typical
focused groups are operator CLI/acceptance, runtime directory/devices, throttle,
RC749, IX, monitor manual, shared terminal, catalog workflow, standard monitor
and binary distribution. [Test Fixtures](TEST_FIXTURES.md) owns assertion detail,
independent expectations and explicit limits; this guide owns invocation.

## Integration and sanitizer verification

Use the full suite at a coherent integration milestone:

```sh
ctest --test-dir build-test --parallel 1 --output-on-failure
```

Serial execution avoids competing timing-sensitive pacing tests. A socket-
restricted environment cannot validate the live network checks; record that
restriction rather than treating skipped or failing checks as success.
Do not run other guest builds alongside a broad acceptance suite: instrumented
guest compilation can approach wall-clock deadlines even when its instruction
budget is unchanged. Manual verification permits 300 seconds per monitor
variant; its two-variant wrapper permits 660 seconds. These are finite host
deadlines, not extra guest steps or relaxed content assertions.

A separate sanitizer profile is available:

```sh
cmake -S . -B build-sanitize -DCMAKE_C_COMPILER=clang \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DSIM4000_REQUIRE_PYTHON_TESTS=ON -DSIM4000_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-sanitize --parallel 1 --output-on-failure
```

The [CI workflow](../../.github/workflows/linux.yml) defines GCC Debug, GCC Release
and Clang sanitizer jobs with required Python coverage, serial checks and
fixture-integrity validation. It does not publish artifacts. A local passing
run does not establish a successful hosted CI run.

At the protected-monitor integration gate, also enable
`SIM4000_BUILD_DEVELOPER_TOOLS=ON` in that sanitizer build. Repeat the isolated
protected rebuild and the Algol/system-binding, nested, isolation and lifecycle
commands below using its helper, then run the live protected Telnet check using
its runtime and derived profile. Keep the same ASan/UBSan environment for every
invocation. LeakSanitizer requires an environment that permits its process
inspection; a sandbox/ptrace failure is not a passing leak check. Do not disable
leak detection to label such a run successful.

## Representative guest workload and diagnostics

The numerical workload is opt-in, not a normal CTest or CI requirement:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-release
./build-release/sim4000_tests --regnetest
```

It checks one compiled guest iteration with the expected sums and success/
failure counters while preserving the base image. --regnetest-trace adds a
rolling FP-operation CSV for diagnosis. --hosttest shows the host-file
round trip, --slangtest exercises assembler loading/error reporting, and
--slangbuild tests assembly, export and execution against derived instruction
bytes. These are diagnostic entry points, not new runtime executables.

To exercise the real interactive frontend with either maintained-monitor
profile, use the separate opt-in check (not a registered CTest workload):

```sh
python3 tests/python/test_regnetest_cli.py build/sim4000 \
  --output build-protection/regnetest-compat
python3 tests/python/test_regnetest_cli.py build/sim4000 --protected \
  --output build-protection/regnetest-protected
```

`--protected` selects the supplied source-built protected monitor and matching
system-binding disk. The output directory must not exist; omit `--output` for
automatic temporary-directory cleanup. Retained logs include configuration,
guest output, final CPU/media/key inspection and a success-only result marker
with media hashes. Both modes use volatile writes, default IX and an explicitly
unthrottled session for a finite 120-second first-iteration deadline. The
protected mode additionally requires fresh `algol end 61` compilation of
cpuregtest, the expected version, actual protection selection and key-1 samples
across the claim-reported process area. It does not execute an arbitrary number
of repeated iterations or certify every numerical program. No PDF, full suite
or sanitizer run is needed merely to repeat this opt-in guest check.

The command-driven editor has a separate opt-in frontend check:

```sh
python3 tests/python/test_protected_editor.py build/sim4000 \
  build-protection/editor-protected
python3 tests/python/test_protected_editor.py build/sim4000 \
  build-protection/editor-compat --compatibility
```

It defaults to the supplied protected monitor/software profile and uses
`rcin`/`rcout`; the comparison selects the unchanged compatibility media and
`fromhost`/`tohost`. Each destination must be new. The private exchange inputs,
eleven content exports, configuration, transcript and success-only result are retained.
Both modes use volatile writes and require byte-exact replacement, insertion/
deletion, truncation and reopening of a guest-edited result, without changing
the source or saved intermediate. Explicit editor begin/end messages distinguish
editor input from FP commands. Protection/IX/media and sampled process keys are
inspected before and after. This check does not certify a screen editor,
crash recovery or persistence across simulator restarts. It is not a normal
CTest workload and triggers no PDF or broad sanitizer run.

The same helper now verifies text display, copy, rename and concatenation in
both profiles. The imported source has 36 logical segments and its copied/
renamed result has one; exported text must nevertheless match exactly. Cleanup
must restore every claim counter and free disk allocation. Compatibility mode
also checks identical/replacement `cmp` and an exported comparison report.
The installed `cmp` is **not supported in the protected profile**: even the
identical-text call pauses in `tofrom1`. A separate instruction-level probe
below identifies a task-mode `pl w3 34`, not a CPU defect to bypass. The default protected
check skips `cmp`, not the positive text operations. Explicit negative coverage:

```sh
python3 tests/python/test_protected_editor.py build/sim4000 \
  build-protection/cmp-known-break --check-cmp-break
```

This requires the specific `tofrom1` break and `pause edcheck break fp`, then
Escape, `s`, `proc edcheck start` and exactly `***fp reinitialized`. Following
exports must preserve all three compared texts, further editor work must pass
and cleanup must restore disk/claims. This is a bounded recovery check, not a
successful protected comparison. Do not combine it with `--compatibility`.
The result marker records this distinction; no runtime/image/library patch is
made to bypass the failure.

To reproduce the instruction-level diagnosis, build only the developer helper
and use another new evidence directory:

```sh
cmake --build build-protection --target sim4000_build_host_image
python3 dev/python/probe_protected_cmp.py \
  build-protection/sim4000_build_host_image build-protection/cmp-instruction
```

The helper reads the supplied protected media with volatile writes and observes
IRQ-0 state without modifying registers, keys or loaded code. It requires raw
`730022` (`pl w3 34`), effective address 34, task PR `bf`, key 1 and a saved
instruction counter advanced by two. The exported 80-segment comparator has
linked `tofrom1`/`tofrom2` and the aligned `730022 a3004f a3003b` sequence in
`tofrom1`, followed by comparisons with 79 and 59. Both recovered inputs must
export exactly; original media hashes remain unchanged. This localizes the
same newer-machine probe pattern already found in installed transfer binaries.
It is negative compatibility evidence, not successful comparison or a fix.
No comparator source is present in the maintained guest-source directory or
was identified by the repository filename search. An older `tofrom` catalog
entry cannot rebind an already linked binary. A compatible source rebuild
therefore remains deferred until matching source is identified; use the
documented host-text comparison alternative in the meantime.

The editor/text checkpoint now verifies that alternative using the host's
`cmp -s` and `diff -u`: equal exports return 0, the edited export returns 1,
and a controlled absent filename returns greater than 1. The changed diff
hunk must match the independently specified two-line sample exactly.
`host-comparison.log` retains statuses/output in the exclusive workspace;
the developer test requires these standard host tools and forces their C
locale, but the normal build/runtime has no new dependency. The checkpoint
also attempts to export edited text over the existing original. It requires
exactly `*** cannot open or create host file, result -8388608`, unchanged old
bytes/inode/size/mode/mtime, continuing FP operations and full final cleanup.
Host comparison runs again on verified exports after clean simulator exit;
all exports must remain unchanged afterward. This is ordinary host text
comparison, not a repaired guest comparator or a binary-length recovery tool.

The two-profile persistence check copies the runtime executable, both media
sets and configurations into a new standalone workspace:

```sh
python3 tests/python/test_profile_persistence.py build/sim4000 \
  build-protection/profile-persistence
```

Both profiles import distinct text/binary samples under the same guest names,
promote them with `scope user`, finish FP normally, quit and restart from a
different host working directory using separate configuration-relative overlays.
Before/after exports and complete lookup metadata must agree, including catalog
permanence key 3 and base intervals. Protected CPU/key inspection remains
separate from catalog permanence. Finally both attempts to reuse the other
profile's overlay must fail with the specific base-image mismatch, without
changing either sidecar. Source/copy hashes, logs, metadata and a success-only
result stay in the new ignored workspace. This opt-in check tests clean restart,
not crash durability, arbitrary catalog scopes, concurrency or other controllers;
it runs no PDF build, registered full suite or sanitizer workload.

The catalog/resource tools have an isolated opt-in frontend check too:

```sh
python3 tests/python/test_protected_catalog.py build/sim4000 \
  build-protection/catalog-protected
python3 tests/python/test_protected_catalog.py build/sim4000 \
  build-protection/catalog-compat --compatibility
```

Each new workspace retains the transcript, offline inventory and two packed
guest exports of the main catalog. `catstat` must match an independent byte
decode of the same session's export, and `cata` must agree with its occupied/
free slot count. Retained names/segment positions also match the offline base;
do not equate all temporary entries before and after boot. Repeated `show`/
`claim` reports are stable. An imported 36-segment temporary area must consume
12 slices and one claim entry, then release all three on explicit cleanup.
The test checks the expected unknown-name diagnostic, continuing FP operation,
export content, selected CPU/key state and unchanged source images. It is not
a general catalog consistency check or a new default CTest workload.

The same check exercises `catsort`, `catsort scope.temp/user/system` and
`search temp/user/system`. Expected installed names and intervals come from
the offline device catalog (414 ordinary system entries in compatibility,
416 in protected). Two imported files exercise promotion, group labels,
per-group sorting and cleanup. Console/exported alphabetical rows must agree
in full, not just by name. Exported search reports must select the correct
scope; their temporary result and at most one `wrk` scratch entry are checked
as snapshot-local names, absent again after cleanup. The special `catalog`
descriptor is checked separately: its inline `; ***` is an unclassified
scope label, not a leading FP error. File contents and initial disk/claim
counts must survive all reports and explicit test-file cleanup.

The bounded FP command-file examples have a separate opt-in checkpoint:

```sh
python3 tests/python/test_protected_commands.py build/sim4000 \
  build-protection/commands-protected
python3 tests/python/test_protected_commands.py build/sim4000 \
  build-protection/commands-compat --compatibility
```

Both runs use reviewed fixtures on volatile media in an exclusive new workspace.
For protected command-file copies, only executable `=tohost` transfers are
changed to `=rcout`; descriptor lookups keep their original installed names.
The check covers nested EOF/output restoration, missing-program continuation,
conditional lookup/group skipping and SLANG success/failure export guards.
It also runs the reviewed `slang_commands.fp` listing example, checking ordered
source instructions in redirected text, independent binary words, byte-exact
source preservation and keyboard/console restoration. Source/listing/binary
exports remain available after simulator exit; padding is not compared as code.
It checks exact event order, independent instruction words in the successful
binary, absence of the failed export, explicit cleanup, restored disk/claims,
CPU mode/IX and sampled protected keys. Logs, copied inputs and success-only
results remain ignored. It does not execute the assembled loop or establish
general batch safety, rollback, arbitrary nesting or all compiler statuses.
It is developer-only, not a normal build or default CTest dependency.

Append `--execute-slang` to either command to check actual execution of the
known integer sample, rather than only assembling/exporting it. After launching
`slprog`, Ctrl-E and a bounded million-call step reach the self-loop. The test
requires W0=444, W1=321, the four literal instructions at the observed IC-6
through IC, then the same IC/registers after another 257 calls. The protected
profile additionally requires task mode, PR bf and code key 1. The compatibility
profile's monitor-mode field is not treated as architectural task evidence
when protection is disabled. Stopped=yes is expected at the debugger prompt,
not a guest failure. No register/core/interrupt injection or breakpoint is used.

The test resumes, requests guest attention, selects s and sends
`proc cmdcheck break`. Exactly `***break 8 ADDRESS` at the observed loop PC and
`pause cmdcheck break fp` are required. A second attention/system selection
and `proc cmdcheck start` must yield only `***fp reinitialized`; later FP
workflows, exports and disk/claim cleanup must still pass. This finite check
does not establish arbitrary machine-code interrupt handlers or endless-loop
safety. The standard invocation still does not execute sample machine code.

For isolated MIPS/TS inventory and initial TEM/PRIMO startup, follow the
[MIPS/TS Plan](MIPS_TS_PLAN.md). The developer-only `probe_mips_ts.py` prepares
exclusive private runtime copies; `--start-components` additionally runs the
real frontend, never against shipped writable media. `test_mips_ts.py` supplies
pure guards and is invoked explicitly, not added to ordinary/default tests.
Inventory and process creation do not establish working terminal/spool services,
SOS initialization, login or protected-subsystem compatibility.
The plan now also provides explicit `--probe-primo`, `--probe-catalog` and
`--probe-sos` modes. PRIMO status/negative-control handling and SOS 5.0 startup
with a private empty catalog pass their bounded checks; neither is a login
recipe. SOS needs an administrative preparation scope and the documented
private reduced resource claim. System errors are captured immediately rather
than waiting for a ready line that will never follow. Do not promote partial
updater output or an error-state SOS process as successful initialization.
The fixed `--probe-account` fixture additionally checks a named process record,
size bounds and quoted dummy password byte for byte before starting SOS.
Repeatable `--operator-command` options retain bounded local SOS replies;
these diagnostic exchanges are not assertions of command success or login.
Even a call naming a missing account can return ready. Input limits, selection
and reply framing have pure guards; see the plan for the observed responses.
Catalog experiments write a classified `catalog-outcome.json` report and print
its status separately from probe completion. Rejected residual output is never
promoted by this report; a normal updater completion is still not a login check.
The catalog-only `--parser-snapshot` option performs a bounded debugger replay
at a checked native updater signature, retaining core snapshots and a 20000-call
trace without patching guest code. It stops at a diagnostic checkpoint, not a
verified updater completion. Use only disposable test data; no runtime profile
or account configuration is promoted by this mode.
The fixed `--probe-terminal` mode checks the complete account plus terminal
record byte for byte and repeats SOS startup. Its two quoted terminal fields
precede the two numeric values; see the plan for the exact disposable fixture.
Successful catalog generation and startup still do not establish TEM attachment
or user login.
`sim4000 --mkdisc FILE` and the operator `mkdisc FILE` command create a new
unlabelled starter disk using C11 exclusive creation. The focused
`tests/python/test_mkdisc.py build/sim4000` check covers exact bytes, CLI option
validation, existing files/directories/symlinks and Readline completion. It is
registered only in the optional test suite; normal builds need no Python.
`--probe-second-disc` additionally checks a retained 207-segment file and exact
numbered text/metadata after quitting and relaunching the emulator and remounting
the same private second disk. It records hashes in second-disc.json and never
relies on an in-memory overlay surviving the restart.
For SOS/account/terminal startup probes, add `--sos-disc1` to label and mount a
private second disk before creating TEM. The derived sosboot changes only the
permanent claim's document to disc1; the original 2500 segments / 200 entries
then suffice. A separate administrative FP must find exactly one temporary
1722-segment swpsos on disc1, followed by healthy TEM/PRIMO/SOS listings.
This does not establish terminal attachment, a job or swap-content correctness.
The opt-in `--verify-batch` check additionally requires exactly
`--operator-command 'batch test' --test-password test` with the fixed account
or terminal fixture. Its intended parent/child guards are separate from
zero-stop startup checks. A single empty response to a pending SOS input can
release the queued list in this diagnostic mode, but the child then waits for
start by its parent. The bounded check still fails. It retains a core snapshot
and decoded child/parent fields for diagnosis; see the plan before interpreting
enrollment as a working job. No successful console-input lifecycle recipe is
published for this mode.
For the separate documented jobfile path, use `--probe-terminal --sos-disc1
--test-password test --batch-job` in a new workspace. This fixed probe creates
a private finis-only batchjob and checks normal enrollment/removal and healthy
services after a bounded advance. It does not use the empty-line workaround
and cannot be combined with --verify-batch. A copy workload still fails with
FP break 10 1; do not generalize the minimal result to useful job execution.
`--batch-workload message` and `--batch-workload lookup` add fixed output
checks; both finish normally. The read/allocate/copy/retain workloads are
bounded negative diagnostics, not supported jobs. The SOS D662 manual confirms
zero default disc segments despite six default entries. Add --account-segments
30 to generate the fixed explicit temporary claim through upsoscat. The entire
export must match its expected bytes (only byte 899 differs from the control).
Allocation/copy then finish normally. --batch-workload roundtrip additionally
checks source/copy character counts and checksums, not an exact byte export.
The temporary-only retain workload fails on scope user no resources. Add
--account-user-storage for cumulative key1..3 quotas of two entries/six segments.
Retain then requires independent descriptor, text and binary export checks
after child removal. All logical bytes and the end marker must match; unused
segment tails are excluded explicitly. This is not a restart-persistence test.
See the plan's comparison results; no shipped media or defaults change.
For an actual interactive session, use --probe-terminal --sos-disc1
--account-segments 30 --account-user-storage --interactive-terminal go (direct
SOS) or run (TEM). A separate private CLASSIC device-4 loopback endpoint tests
wrong/correct inline passwords, FP message/lookup and finis. The helper checks
the TEM header for run, separate echo/output evidence and final child removal.
This developer-only mode requires local socket access, uses dummy credentials
and never changes the shipped network configuration. It is not a multiterminal
application, restart-persistence or protected-profile acceptance test.

The shared-terminal check can display its generated selection/reply frames:

```sh
SIM4000_SHARED_DEBUG=1 \
  python3 tests/python/test_shared_terminal.py build-test/sim4000
```

It covers idle ordinary FPs and short reports on MONITOR/RC433X only. Do not
reinterpret it as support for rapid pasted scripts, arbitrary programs or
the alternate monitor profile.

## Media regeneration

Enable developer generation separately; it is not necessary for tests or use:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DSIM4000_BUILD_DEVELOPER_TOOLS=ON
cmake --build build-release
```

The optional standard-monitor target writes generated/STDMON for comparison
with the supplied artifact. The native-image target writes UNIT0.host.
[Disk Workflow](DISK_WORKFLOW.md) owns copying, regeneration, hashes, candidate
promotion and recovery. Neither target silently promotes generated media.

### Rebuild the historical monitor

The disk-resident SLANG assembler builds the shipped monitor variants from
the maintained inputs in `monsrc/`. From the repository root, after
a release build:

```sh
python3 dev/python/rebuild_monitor.py build-release/monitor-rebuild --verify-boot
```

The work directory must not exist. All needed sources are included in
`monsrc/`; the parent historical tree is not required. `--sources` can select
an alternative directory explicitly. The tool preserves its inputs and
records input and prepared
source hashes in `sources.txt`. The repository sources use LF and lowercase
filenames. The preparation step also accepts CRLF in alternative inputs and
maps the CP850 Danish comment bytes `0x86`/`0x91` to historical RC 7-bit
positions `}`/`{`. Unknown high bytes or high bytes outside comments are
rejected. This is not a general encoding converter.

All monitor builds now use `monsrc/sim4000.mops`, including protected builds
and DFC403 builds. The `hjmops*` files are historical references
and remain unchanged; the old `--variant` option is removed. The maintained
file derives from the previously verified `hjmops.new` with the same normalized
commands/options, an English provenance header and the explicit printer option.
Focused preparation tests verify the inherited commands and one printer entry.
All three printer-capable variants have been rebuilt, verified and promoted;
current artifact hashes are in [RC610](RC610.md).
The wrapper imports `montxt1` through `montxt11`, `mon8s`, and `mon8init`
(under guest name `mon8initcat`), then executes the options/build file through
FP `i mbuild`. The script creates `rt2`, `rt3`, `rt4`, `rt7`, and `rt11` with
the guest editor, and assembles:

```text
binmon=slang type.yes montxt1 rt2 rt3 rt4 montxt5 montxt6,
  rt7 montxt8 montxt9 montxt10 rt11 mon8s mon8initcat
```

`type.yes` is essential: SLANG `t.` reads the generation options and driver
text from the current FP input file, then `n.` resumes the source-file list.
The initial options in `sim4000.mops` include date/time `931010`/`174000`, 95 areas,
10 internals, 85 message buffers, and the historical type-74 host driver.
These are preserved for reproduction, not modernized. The native guest
transfer commands used by the wrapper are `fromhost` and `tohost`.

Before adding the printer, the observed result was `slang ok 11/24980/49`;
that historical `candidate` was 37,632 bytes
with SHA-256 `b5a40b73a38f65f02f47835ef1557c9989055df68c692a0719bf536ba14cfbb2`,
equal to the then-shipped monitor file. Current printer-enabled monitors are
38,400 bytes and do not have that older identity. Both bootstrap and candidate
boots use a volatile disk overlay on a verified copy. `--verify-boot` boots
the candidate and checks ASCII text and all-byte binary round trips.

`session.log`, `buildlist`, and (if assembly/export succeeds) `candidate` are
retained, including failed attempts. Assembly diagnostics are redirected to
`mongenout` by the historical script, so success is checked in the exported
listing, not just the console. The listing contains harmless `***` message
comments; actual `***slang`, `***edit`, and syntax errors are rejected.
No candidate automatically replaces `src/monitor/MONITOR`. Future generation
changes belong in `sim4000.mops`, not in the historical `hjmops*` references.

### Generate the experimental protected monitor

This is an M3 experiment, not a validated replacement for the supplied monitor.
Keep the stable configuration and media unchanged. Use a new work directory:

```sh
python3 dev/python/rebuild_monitor.py build-protection/protected-monitor-e22 \
  --builder build-protection/sim4000_build_host_image --protected --verify-boot
```

`--protected` imports sim4000-protected.mops as mbuild and the unchanged common
sim4000.mops as mcommon. The wrapper uses the historical guest editor to create
ptxt10, locate e22 and delete the recognized HJ return. It temporarily exchanges
guest filenames while the common build runs, then restores the original name.
The host-prepared montxt10 is byte-identical to its normalized input. Python
performs only read-only input identity/uniqueness and result verification;
there is no host-side Protection source transformation.

The console calls mbuild at FP input depth 1, and its sole i mcommon uses depth
2. The common file has no further FP include. Editor/SLANG input uses those
same streams; a third wrapper would exceed the historical FP limit and is
rejected by preparation checks. Generation options, printer declarations and
all common assembly commands remain in sim4000.mops without duplication.

Missing/changed/ambiguous montxt10 input is rejected against its normalized
SHA-256 and unique bypass. The C session driver exports pkeditlog, pkedited
and pkrestored along with buildlist/candidate. Python requires editor completion,
no editor/assembler/FP error markers, exactly the one-line guest deletion and
an unchanged restored original. The original monsrc/ files remain untouched;
sources.txt records original/prepared hashes and profile.txt identifies the
guest-editor transformation, input depth, entry and candidate identity.
The first attempted guest edit was exact, but its retained original exhausted
the previous allocation layout during binmon output. A 2500-segment claim was
rejected by the guest, so that change was not retained. Compacting the padded
montxt10 and mcommon imports with guest copy/clear/rename solves this within
the original 2000-segment claim, without changing their text.
The guest build itself still runs with protection off on a disposable disk copy.

Candidate verification now uses `--verify-protected-profile` and the matched
UNIT0.protected/rcin/rcout libraries, with actual CPU protection
enabled and IX matching the normal runtime default. It uses a volatile overlay,
does not rely on the old monitor's hardcoded idle addresses, and rejects guest
program breaks. Success additionally requires observed task execution and
nonzero storage keys; those checks alone are not proof of process isolation.
The generated `RC4000.protected.cfg` is for this separate work directory only;
it selects `candidate`, the disposable UNIT0 copy and host exchange rooted there.
It is emitted before verification and is not a success or promotion marker.

The targeted regeneration regressions can also be run directly:

```sh
python3 tests/python/test_monitor_rebuild.py build-printer/sim4000_build_host_image
python3 tests/python/test_monitor_rebuild.py build-printer/sim4000_build_host_image \
  --protected --printer-test build-printer/sim4000_rc610_guest_tests
```

They compare candidates byte-for-byte against the verified compatibility or
protected reference, check input/media hashes and verify the candidate boot.
The protected path additionally checks exported editor evidence and exercises
Guest-s printing with CPU protection enabled. These are developer-only focused
tests, not prerequisites of ordinary builds or distribution packaging.

Assembly currently succeeds, and protected startup/date initialization/process
creation work. The first `fromhost` import breaks in `tofrom1`; do not treat this
as a passing protected-monitor workflow. `claim` succeeds first, and the helper
requires its allocation report. The offending task-mode PL instruction and its
adjacent comparisons have a native compatibility/protection regression; the
routine's possible later-machine feature probe still needs independent evidence.
Separately linked older-tofrom transfers pass the bounded checkpoint below;
this does not change the failure of the original installed transfer binary.
Do not replace this investigation with a global task-mode PL exception. Failed
work directories, candidate and
logs are retained. The developer helper reports a bounded ring of IRQ-0 step
snapshots when a protected guest program breaks. See
[the checkpoint](TEST_FIXTURES.md#experimental-protected-monitor-checkpoint).

### Inspect the existing Algol runtime libraries

This exports existing areas without changing them; it does not install or
select an older procedure. Use an empty host directory and compatibility mode:

```sh
mkdir build-protection/library-inspection
build-protection/sim4000_build_host_image --inspect-libraries \
  src/monitor/MONITOR src/disks/UNIT0 build-protection/library-inspection
```

The helper uses volatile disk writes and refuses existing host exports through
the normal host-file service. `oldruntime` and `newruntime` are packed binary
exports of oldalgollib and algollib. `frombinary` and `tobinary` export the
installed transfer programs, and `libmetadata` records guest `lookup` output.
The focused `sim4000_runtime_libraries` test checks identities, linked module
labels, parameter-entry prologues, binding descriptors and probe locations. See
[runtime library inspection](TEST_FIXTURES.md#existing-algol-runtime-library-inspection)
for assertions and limits, and [the protection contract](PROTECTION_SPEC.md#runtime-opcode-conflict-and-older-library-candidate)
for the RC8000 GG versus RC4000 PL distinction and incomplete manual details.

### Probe transfers linked with the older tofrom

Use a separate destination and an already assembled protected candidate:

```sh
python3 dev/python/probe_rc4000_runtime.py build-protection/old-runtime-probe \
  --candidate build-protection/protected-monitor-e22/candidate
```

This developer-only probe refuses an existing destination, verifies the native
source image and makes an exclusive writable copy. The compatibility monitor
imports the unchanged transfer sources and builds new `rcin`/`rcout` programs
using a derived single-segment oldalgollib tofrom descriptor. The FP `set`
command changes the existing binding on this private image, not a temporary
shadow. The helper restores the original binding after linking and validates
it in the resulting catalog. It never replaces `fromhost`, `tohost` or either
library. Only the new compiled programs are retained in user scope.

Compatibility and protected sessions independently check text and binary round
trips. The protected session uses a volatile overlay on the private derived
image, verifies task/key activity and requires the base-image hash to remain
unchanged. Logs, compiled binary exports and a success-only `result.txt` remain
inside the work directory. Failures retain diagnostics, never promote media
and do not imply that protection may be weakened.

The helper's `--build-rc4000-transfers` mode writes its image argument and is
only for such disposable copies; `--verify-rc4000-transfers` uses volatile writes
and the protected CPU profile. The names describe the experimental workflow,
not a guarantee that every linked library path is RC4000-compatible. Newer
`nsystem` is deliberately retained. Its remaining GG probes must have zero
observed executions on the passing protected transfer path. Aliasing it to the old `system` descriptor
fails in `system2` already with protection off and IX enabled; matching
parameter types alone do not establish an interchangeable implementation.

Add `--lifecycle` to exercise a bounded local process lifecycle as well:

```sh
python3 dev/python/probe_rc4000_runtime.py build-protection/protected-lifecycle \
  --candidate build-protection/protected-monitor-e22/candidate --lifecycle
```

The protected helper selects `--verify-protected-lifecycle`: stop/resume the
existing hostcheck FP, verify resumed text/binary exports, remove it, confirm
absence from the system list, then recreate the same name with size 100006.
It imports/exports fresh text and removes the replacement process. Bounds come
from each fresh guest `claim` report, not hardcoded core addresses. Each running
area must have one nonzero key throughout; stopped/removed areas must have the
parent system process's expected key zero. Adjacent word keys must remain as
observed at the running checkpoint. The 50003-word case exercises the e22 tail.

Temporary files are explicitly cleared in FP before removal. A direct
`proc hostcheck remove` trial left their catalog entries visible on recreation;
this check therefore makes no claim about automatic file reclamation. Python
checks each resumed/recreated export independently, rejects missing/corrupt
content, and still requires an unchanged private base image. Zero remaining
GG-probe executions is required over the whole lifecycle, not just the initial
transfer. This is not a sibling/descendant isolation test or M3 acceptance.

Use a separate run for two-console sibling isolation:

```sh
python3 dev/python/probe_rc4000_runtime.py build-protection/protected-isolation \
  --candidate build-protection/protected-monitor-e22/candidate --isolation
```

`--isolation` and `--lifecycle` are mutually exclusive; preserve separate logs
and image identities. The helper's `--verify-protected-isolation` mode keeps
hostcheck idle on console device 2, selects the system process through device 4,
and creates attacker there. Claim reports must describe disjoint full-keyed
areas with different nonzero keys. This uses the real console devices, not a
network connection or two pending FPs competing on one input stream.

The helper exclusively creates four LF SLANG source files in the private host
root. Their target is the last word of the claim-reported victim area. The
attacker imports and assembles each through guest rcin/SLANG; no CPU state or
core contents are injected by the host. Each probe first reads the victim word,
then attempts RS, JL, PL or KS. It must reach exactly one matching task IRQ-0
snapshot with the expected effective address, advanced IC, PR/key selection and
read result. An error banner or an unrelated program break is not sufficient.

All victim data words must match the snapshot taken before running the probe,
and all victim keys must remain intact. Monitor stop/restart legitimately
changes the attacker's area keys (including a contiguous victim's neighboring
word); the helper verifies those transitions explicitly. After each fault the
victim exports its original file, Python checks each export independently,
and the supervisor restarts the attacker FP. Missing/corrupt recovery exports
are failures. Remaining GG-probe executions must still be zero.

This is bounded sibling/privilege coverage, not a general host-security sandbox,
a descendant-process test, live Telnet validation or the full M3 gate.

Run protected guest Algol compilation in another private work directory:

```sh
python3 dev/python/probe_rc4000_runtime.py build-protection/protected-algol \
  --candidate build-protection/protected-monitor-e22/candidate --algol
```

`--algol`, `--lifecycle` and `--isolation` are mutually exclusive. The helper's
`--verify-protected-algol` mode first imports, compiles and executes the hello
fixture and `tests/fixtures/protected_algol.alg`. The latter checks an integer
loop, a value-parameter procedure and real arithmetic before printing success.
Program output must come from each execution command, not a source listing.

Next the unchanged fromhost/tohost sources are imported and compiled under
active protection into separate `pcin`/`pcout` entries, using the older tofrom
descriptor. The visible tofrom binding is then restored and checked by guest
lookup. These newly compiled programs must perform fresh text/binary round
trips and export their own binary images. Python verifies contents, segment
allocations and the older tofrom module layout; it also requires complete
hello/arithmetic binary exports. No installed program is replaced or promoted,
all protected-session catalog writes are volatile, and the base hash remains
unchanged. The known remaining GG probe must have zero observed executions.

The protected transfer compilation currently reports `9. oldalgollib bases`
before `algol end 54`/`46`. The resulting programs execute the checked transfers,
but this diagnostic's meaning/severity remains unclassified. In this normal
FP session, lookup shows both a system descriptor and a temporary descriptor
after restoring tofrom, with the original binding values. Do not generalize
the compatibility-build process's catalog behavior to every user scope, or
claim a universally clean compiler/library profile from this checkpoint.

For the controlled system-binding comparison, use another new destination:

```sh
python3 dev/python/probe_rc4000_runtime.py build-protection/protected-algol-system \
  --candidate build-protection/protected-monitor-e22/candidate --algol --system-binding
```

`--system-binding` requires `--algol`. The explicit developer build mode
`--build-rc4000-profile` retains the older tofrom system descriptor only in its
exclusive writable disk copy, instead of restoring it after the bootstrap.
The original fromhost/tohost binaries and both libraries remain unchanged.
Both device and main catalogs must show tofrom and oldalgollib with matching
system bases `800001,7ffffd` and permanence key 3. No raw image patch is used;
the binding is made through guest FP set in the compatibility build process.

`--verify-protected-algol-profile` then compiles/runs the same source fixtures
under protection without introducing a temporary tofrom alias. Both compiler
base diagnostics must be absent. Python additionally requires exactly two
explicit absent-diagnostic observations; the ordinary temporary-binding
comparison requires exactly two present-diagnostic observations and messages.
Missing/reversed/extra evidence is rejected. Fresh guest lookup and both offline
catalog layers validate the selected profile. Base hashes remain unchanged by
protected verification, while `result.txt` records the derived binding and hash.

The comparison associates the diagnostic with the temporary binding's narrower
catalog scope, not a missing CPU instruction. It does not establish the official
meaning or severity of diagnostic 9. Never use this option to modify the shipped
UNIT0 or promote the experimental disk/configuration automatically. Lifecycle,
isolation and live Telnet are separate checks; this comparison does not certify
them for the derived-system profile.

### Check the protected profile through live Telnet

After producing the identified system-binding fixture above, explicitly build
the normal simulator and run the bounded integration script:

```sh
cmake --build build-protection --target sim4000
python3 tests/python/test_protected_telnet.py build-protection/sim4000 \
  build-protection/protected-monitor-e22/candidate \
  build-protection/protected-algol-system/UNIT0 \
  build-protection/protected-telnet
```

The script verifies the candidate and derived-profile hashes before creating
its new work directory. It listens only on an ephemeral loopback port, selects
PROTECTION MODE=RC4000 and uses volatile disk writes. PTY/TCP permissions are
required; a sandbox refusing local sockets must be handled explicitly rather
than counted as an emulator failure or bypassed. This is a manually invoked
developer integration check, not a normal-build or automatically registered
CTest dependency on generated monitor/media artifacts.

Device 2 runs local FP, and a real Telnet connection to device 4 runs remote FP.
Fresh claim reports and read-only packed core snapshots identify distinct
descriptors, disjoint core areas, a common s parent, keys 1/2 and mutually
protecting stored PRs. Operator `show keys` samples the first/middle/last word
of each area; it does not repeat the helper's full-area native scans. The
current operator CPU PR may be 80 while idle in the monitor, so the script reads
each user's saved PR from its descriptor rather than interpreting operator
PR-protected labels as the user's access mask.

The remote FP explicitly clears its temporary file before removal. A completed
system list and core snapshot confirm removal and sampled key return to zero;
the local descriptor/keys and a subsequent export remain valid. Recreate the
remote FP, import/export fresh text, then disconnect/reconnect the client and
export that same surviving file again without recreating the process. All six
exports are checked independently. Logs, snapshots, configuration and a
success-only result marker stay under the private destination. Candidate,
derived base and shipped MONITOR/UNIT0 hashes must remain unchanged, and PTY
state must be restored on clean shutdown.

This is one classic console Telnet endpoint, not RC851/V2 acceptance, a network
security test or a descendant-process test. M3 acceptance combines these
separate workflows with the broad regression/sanitizer checks described above.

### Check nested guest process protection

Run the isolated generated-guest check in another new work directory:

```sh
python3 dev/python/probe_rc4000_runtime.py build-protection/protected-nested \
  --candidate build-protection/protected-monitor-e22/candidate --nested
```

`--nested` is mutually exclusive with lifecycle/isolation/Algol probe modes and
automatically builds the private system-binding profile. The helper selects
`--verify-protected-nested`, creates hostcheck with `key 2 internal 2`, and
reads its live claim/descriptor. The parent must have key 2 and PR 9f.

Generated LF SLANG sources are imported/assembled by the guest. The parent
itself writes a one-word self-loop into its reserved child area and makes
monitor calls 56/58/60/18 to create/start/stop/wait for the child. The maintained
monitor parameter layout is taken from montxt11, not copied blindly from the
1969 manual: first/top, two claims words, PR/key and max/standard intervals.
Descriptor a44/a45 name the final words of double-word intervals, so these
four base values are read at offsets 72--78. No host CPU/core/key writes are
used to manufacture a child or advance a test phase.

Each parent first requests a child with PR 80/key 1, which would widen its
available keys beyond the parent's allocation; result 1 is required. It then
creates the valid PR bf/key 1 child. Three cases cover 2/20/26 RC bytes (1/10/13
words), including the short e22 path, exact block and remainder. Actual child
steps must execute the guest self-loop in task mode with PR bf.

The parent publishes guest-written stage words and uses a deterministic guest
countdown. The helper stops/resumes the whole tree through s while stage 1 is
running. The parent subsequently stops its own child and waits for the answer;
matching pre/post buffer claims verify release. All parent words, child words
and outer boundary keys are checked at each transition. Counters must stop
advancing when the child is stopped. Recursive removal must clear both names.
The sources, logs and success-only marker remain private, with base/candidate
hashes unchanged by protected verification. This does not certify every tree
depth or scheduling policy. The completed bounded functional checks and broad
regression/sanitizer acceptance are recorded in
[Test Fixtures](TEST_FIXTURES.md#instrumented-protected-profile-checks).

### Prepare selectable maintained-monitor profiles

After the protected rebuild and Algol/system-binding workflow above, prepare a
new workspace from those identified artifacts:

```sh
python3 dev/python/prepare_monitor_profiles.py build-protection/profiles \
  --candidate build-protection/protected-monitor-e22/candidate \
  --image build-protection/protected-algol-system/UNIT0
./build-protection/sim4000 -a -c build-protection/profiles/RC4000.compat.cfg
./build-protection/sim4000 -a -c build-protection/profiles/RC4000.protected.cfg
```

Run one session at a time for the same workspace. The preparer accepts only the
verified maintained monitor, protected candidate and derived system-binding
disk identities; it refuses an existing destination. It performs no guest build,
network access, media promotion or binary-only V2 integration. Generated files
are developer outputs, not new default artifacts or a binary distribution.

Each configuration uses its own flat MONITOR/UNIT0 or
MONITOR.protected/UNIT0.protected pair, with a shared exchange directory.
Paths are configuration-relative, so the complete workspace
can be moved without editing host paths. Both explicitly select IX and CLASSIC
terminals; protection is OFF versus RC4000. Writes are volatile by default;
each configuration includes a commented independent overlay option. Never
reuse one profile's overlay for the other. Optional Telnet remains disabled
until explicitly enabled. The generated README explains protected `rcin`/
`rcout`, retained incompatible original utilities and the software boundary.

Python is required only for preparation, not for running the resulting
configurations with the existing emulator. The separate binary-dist target now
packages the verified prebuilt protected artifacts as an optional alternative
alongside the default compatibility system; see
[Binary Distribution](BINARY_DISTRIBUTION.md). The preparer's workspace is not
itself that archive and does not certify arbitrary installed software.

### Inspect the independent RC8000 monitor listing

For provenance comparison, the optional offline tool reads Datamuseum's
[public tape 30003039](https://datamuseum.dk/wiki/Bits:30003039). Download it
explicitly into an ignored work directory; the tool performs no network
requests and is not part of a normal emulator build:

```sh
mkdir -p build-release/archive-30003039
curl -fL https://datamuseum.dk/bits/30003039 \
  -o build-release/archive-30003039/30003039.tap
python3 dev/python/inspect_monitor_tape.py \
  build-release/archive-30003039/30003039.tap \
  build-release/archive-30003039/extracted
```

Use a fresh download path; `curl -o` overwrites an existing download file.
The extraction destination must not exist. The reader verifies the exact
archive size and SHA-256, all SIMH record wrappers, and the first complete
RC dump's descriptor/segment sequence, segment counts, and end totals.
It extracts only that monitor dump, not unrelated/incomplete later dumps.
The record layout was cross-checked against the primary
[AutoArchaeologist decoder](https://codeberg.org/Datamuseum-dk/AutoArchaeologist/src/branch/main/autoarchaeologist/vendor/regnecentralen/rc489k_tape_dump.py);
this tool is independently written and intentionally not a general tape
restore utility.

Outputs are raw allocated areas (`*.bin`), readable listings
(`*.listing.txt`), numbered-line comparison derivatives
(`*.numbered-lines.txt`), `manifest.json`, and `comparison.txt`. Raw areas
preserve padding; readable copies stop at the first guest end-medium byte.
Numbered-line derivatives are not promised to rebuild: unnumbered messages
and tables are omitted. The report measures shared textual lines and blocks,
not binary compatibility or a complete source diff. Reference files are
read-only inputs; nothing is imported into the native disk or `monsrc/`.
See [the provenance findings](ARCHITECTURE.md#independent-monitor-listing-from-datamuseum).


## Trace comparison

```sh
python3 dev/python/trace_compare.py EXPECTED ACTUAL
```

The comparator ignores descriptive comments and reports the first differing
version-1 field. [Trace Format](TRACE_FORMAT.md) defines the record contract;
tests/fixtures/trace_integer_device_v1.trace supplies an independently derived
CPU/core/device vector.

## Host portability

Development and testing use Linux/x86-64. Other POSIX hosts, including macOS,
have not been validated; platform-specific setup and testing are left to
interested contributors. The runtime uses C17, POSIX interfaces and GNU
Readline, not compiler-specific CPU code. A strict C17 Clang build with
`-pedantic-errors -Werror` also succeeds on the development platform.
The CPU-budget regression in `tests/python/test_throttle_cli.py` specifically
requires Linux `/proc/<pid>/stat`; it is not a portable host test.

An i386 build and bounded startup
of both supplied monitor profiles have succeeded without runtime source changes:

```sh
cmake -S . -B build32 -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-m32
cmake --build build32
```

This requires a 32-bit compiler/runtime and Readline library. It is not full
32-bit validation: regression coverage, long-based file offsets (approximately
2 GiB on ILP32) and package ABI naming still need review. The package filename
currently uses the configured processor string, which may identify an x86-64
host even when -m32 produces an i386 executable.

## Distribution and documentation

[Binary Distribution](BINARY_DISTRIBUTION.md) owns the explicit binary-dist
target, archive manifest and its failure checks. The target packages the
existing PDF and requires neither Python nor test execution.

The ready-built doc/usermanual.pdf is versioned. LuaLaTeX sources and optional
standalone inputs live under doc/usermanual/. Update the PDF only through an
explicit user-manual target invocation; ordinary builds never rebuild it.
Source checks require no TeX:

```sh
python3 dev/python/check_manual_sources.py
python3 tests/python/test_manual_sources.py
```

These checks validate structure/references, not rendered pagination or historical
claims. [Manual Evidence](MANUAL_REVIEW.md) maps documented workflows to bounded
checks. Keep generated logs, candidate images, downloads and renderings in
ignored build trees, not in contributor documentation.
