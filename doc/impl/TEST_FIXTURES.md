# Test Fixtures

The files described here live in `tests/fixtures/` at the repository root.

## Protected guest-editor monitor generation

The protected MOPS wrapper is an outer FP input file; its single i mcommon
call opens the shared build at depth two. Preparation rejects another FP
include. The wrapper compacts padded guest imports, edits temporary ptxt10,
temporarily exchanges guest names and restores the original afterward. It
fits the existing 2000-segment claim; no Python source patch is performed.

Focused preparation tests require the known normalized montxt10 identity and
unique HJ bypass before writing sources, preserve every prepared segment, and
reject missing/empty/changed/duplicate input. Export-result tests reject an
unchanged bypass, extra edits and a missing restoration. Diagnostic tests
reject editor, assembler, FP, nesting and device-status failures even alongside
a success marker. A terminated monbuild is detected without waiting for an
impossible next FP prompt or attempting exports from a dead process.

Two live private negative controls also reject the build: a nonexistent editor
search marker terminates monbuild with a failing result, while an invalid SLANG
token in a private montxt1 copy produces ***slang sorry and is rejected even
though the low-level guest driver exits zero and exports a candidate file.
Neither failure is a success/promotion marker. Failed attempts remain private.

The private protected guest-editor build produces exactly the existing
MONITOR.protected bytes (9055b2f5...); the compatibility path still produces
MONITOR (dff97596...). The guest-editor log completes, pkedited differs by only
the HJ jump line, pkrestored is identical to the original, and the host-prepared
montxt10 still contains the original bypass. Protected boot/rcin/rcout transfers
and the existing two-print Guest-s check pass with real keyed task execution.

tests/python/test_monitor_rebuild.py now supports --protected and an optional
--printer-test binary; sim4000_monitor_rebuild_protected registers that opt-in
generation/identity/boot/printer regression. The compatibility regression also
uses the current assembler-success predicate rather than a pre-printer listing
size, and snapshots build inputs/media, not concurrently edited README prose.

## RC610 printer checks

Maintained builds use sim4000.mops with one printer/dev7/IRQ10 option. Preparation
tests preserve all originals, reject --variant and compare inherited commands
with the historical reference apart from the explicit printer addition.
The guarded DFC403 selection and protected e22 restoration affect working
copies only. Current artifact identities are in [RC610](RC610.md).

Native test_rc610.c uses independent graphics, status, interrupt and raw-control
vectors, exercises bounded width and local/paper transitions, and injects short
write/flush/close failures. Exclusive output never truncates; append preserves
contents across connections. Symlinks, FIFOs and device files are rejected.
A separate reader sees completed CR/VT/FF/NL prints before close.

The C guest harness uses volatile disks. Compatibility, protected and DFC403
checks complete two Guest-s IRQ-vector prints and a later system command.
Protected mode requires actual keyed task execution; a mismatched non-protected
monitor fails startup rather than falling back to unprotected execution.
Numeric-format and explicit reservation-owner verification remain open.

The source check compiles print_source.alg through guest Algol and sends the
installed cpuregtest source through a physical printer zone (mode 0), not an
output area named printer. An independent guest export is compared with
the entire printer output before close. Both compatibility and protected
examples require 630 operations / 17309 bytes without running the numerical
program. sim4000_rc610_source_protected uses the matching protected disk,
rcin/rcout and unchanged example. It requires protection enabled and new keyed
task observations during ptext itself (3547317 in the checked run), normal FP
end, no device errors, finis and a subsequent s list. The raw console end-line
parser accepts LF-CR/RUBOUT padding, not abnormal-end substrings. The wrapper
checks tutorial/fixture identity and unchanged input/media hashes even on failure.

test_rc610_cli.py boots source configs over unchanged base media twice for
compatibility, protected and DFC403. It seeds the output with existing text,
requires automatic append and immediate print visibility, verifies exclusive
attach rejection, explicit append attach, reset preservation and clean exit.
No source/config/base-media writes are used to pass the tests.

The native main regression retains exact fixed-image checkpoints for the new
MONITOR identity dff97596: idle moved from 0x001b34 to 0x001c5e. At one million
calls the COW boot snapshot is one disk transfer earlier (1378 interrupts,
1444 I/O operations, 1015439 instructions, 214 written sectors); at four million
calls it has executed 4043669 instructions. These pinned-image expectations
were recalibrated after source generation and independent guest/transfer checks,
not weakened to lower bounds or values computed from the emulator under test.
The developer session driver uses completed console reads/quiet output instead
of hardcoded old-monitor idle addresses for command completion.

Acceptance also covers the extracted binary archive's profiles, and the flat
MIPS/TS two-boot Telnet login/logout/persistence workflow with the new monitor.
Loopback-listener checks require host permission outside restricted sandboxes.

Probe-only inventory checks follow live peripheral table slots, not embedded
names or area entries; historical reader/punch descriptors guard interpretation.
The current input-copy helper never modifies hjmops*. Inventory is separate
from actual device I/O and does not replace native/guest acceptance.

test_mips_printer.py copies the shipped MIPS/TS media/config into an exclusive
workspace and uses independent persistent overlays plus a free loopback port.
It compiles the shipped examples/primo_print.alg in an administrative FP,
signs up printer/kind 14 and submits two sequential disk-area transports per
boot: a short marker and a marker plus 160 listing lines. It requires positive
transport IDs, result/reply 0, state 5/cause -1, normal FP return, byte-exact
output before shutdown and no payload on the console. All 15144 bytes must
survive the second startup and shutdown unchanged. Display checks idle signup
and assignment removal after signoff. Password rejection, TEM-mediated SOS
login, FP message/lookup and logout pass after printing on both boots.
Raw Telnet evidence is retained; NVT CR-NUL is decoded only for text assertions.
Source media and private base-copy hashes must remain unchanged. The older
console-backed experiment is not repurposed as hardware evidence. Full guest
recovery and unrestricted queues remain deferred; protected Algol source
printing is covered by its separate focused check above.

With --offline, this test also detaches the idle device before submitting an
additional transport on boot 1. It requires the exact printer-end-document
notice, a waiting pin row, no premature completion, unchanged host file and
healthy subsystem listings. Append reattachment must not alter the file;
PRIMO start printer must return a normal completed transport and append the
38-byte payload exactly once. The second boot must preserve it and accept
further normal jobs. No CPU/device code or media changes are used to pass the
test. The registered CTest includes this option. The pure test_mips_ts.py
guards reject wrong status/cause, duplicate/missing completions, console payload
leaks, a false idle state and premature completion during the offline notice.
Partial-write/disk-full and mid-job paper recovery are not inferred.

With --queued, tests/fixtures/primo_queued.alg flushes each accepted definition
before waiting. A second new process submits a distinct area/descriptor/program
variant while the first transport is held at the detached printer. The test
requires two distinct positive IDs before reattachment and no prior output.
The first client waits with action 6. The second receives reply 3 and uses
action 4 to query state, bounded to one million queries plus the host deadline.
Both must report result/reply 0, state 5/cause -1 and normal FP ends. The two
payloads must appear exactly once, contiguously and in definition order. Only
already completed ordinary FPs may receive empty console-routing commands.
After removing the second idle client, a fresh transport must complete on the
same PRIMO instance; all output persists across the second boot and both
TEM/SOS checks must still pass. The registered CTest includes --queued.

Each new client requests temp disc 200/10 and perm disc 0/0. all would reserve
all remaining keys; new's default permanent claims also prevent this bounded
pair. The first client's obsolete prbprog is cleared before compiling qone to
stay inside its explicit temporary claim. Separate pin/pjn and plp/qlp entries
avoid shared-base collisions; generated fixture copies alone are specialized.
The pure guards also reject duplicate/zero transport IDs, premature completion,
missing client returns, wrong state/cause and console-only payloads. Acceptance
does not establish two successful blocking waits or arbitrary queue capacity.

The optional --primo-buffers 10/11 diagnostic exports the original startprimo,
primotrim and packed bprimo through guest tools before removing the unused live
PRIMO. It accepts only the known startup token sequence and requires the unique
waitops=1 generation assignment before recreating the private process. The
larger claim still produces the missing-resource/polling path and exact prints.
The second boot uses the unchanged disk startup again. Hashes, startup messages,
queue results and terminal evidence distinguish the requested live claim from
any regeneration. This does not independently decode the binary wait table.

The asynchronous end-document notice may precede or follow the flushed client
definition. The held-definition guard accepts both orders but requires exactly
one positive definition and the exact notice, no other errors and no premature
completion. Pure negative controls also reject changed/missing/duplicate startup
commands before the private recreation. This is protocol framing, not a relaxed
guest-success check.
Invocations belong in [Development](DEVELOPMENT.md); user operation in the
line-printer chapter, not this implementation record.

## Unprotected MIPS/TS batch manual exercise

tests/python/test_mips_batch_manual.py uses private copies of the shipped
unprotected profile and persistent sidecars. It requires an absent batchjob,
prepares the compact two-line message/finis file with matching 0/0 intervals,
checks a one-segment user descriptor and finishes the administrative FP. Two
local SOS submissions independently check the password-question and inline
forms. Both must enroll exactly once, print batchprobe exactly once, finish
normally and leave no test child; resident keys/stop counts remain healthy.
A later interactive Telnet exercise checks that the account was released.
Exact tohost readback must preserve the original jobfile; explicit clear user
cleanup must leave its name unknown. Source/private-base hash guards apply
even on failure. The result records both accepted password forms, not a
fallback or general password-dialogue guarantee.

Eight mips-batch TeX blocks match the fixed live sequence. Two new pure guards
bring test_mips_ts.py to 48 tests, covering literal manual drift and exact
password-frame kind/echo/prompt rules. Existing message/removal guards reject
missing/duplicate output, abnormal completion or a surviving child. This live
exercise is opt-in, not an ordinary build dependency, and makes no remote-
submission, concurrency, crash or arbitrary-program acceptance claim.

## Unprotected MIPS/TS user-file exercise

tests/python/test_mips_files.py is an explicit opt-in live test, not an ordinary
build prerequisite. It uses new copies of the shipped compatibility MIPS/TS
pair/config and both persistent overlays, with a free loopback port. CPU output
must confirm protection off. An administrative FP with all four intervals 0/0
first requires keepseed/keepsake to be unknown, imports the independent LF text,
compacts it and retains only the seed. The remote test child copies/retains
keepsake within its existing quota. A second normal login reads the same file.
After full emulator restart, another login reads it without recreation; the
fourth session deliberately deletes it. Matching-base tohost exports on each
boot must equal the host input bytes; final guest lookups require both exercise
files absent after seed cleanup. Raw wire/per-login/per-boot logs are preserved,
all normal logouts remove the child and all source/private-base hashes stay
unchanged. No default images, quotas or startup files are changed.

manual_recipe binds eight marked TeX blocks to the checked literal dialogue.
Its pure guard and the structural manual checks need no emulator, sockets or
PDF generation. This distinguishes guest user retention from administrative
preparation and host overlay persistence; it is not disconnect/crash recovery.

## Separate protected-subsystem diagnostic

dev/python/probe_mips_protection.py uses new copies of the maintained protected
monitor, disk and config, with IX/protection actually enabled and no persistent
sidecars. Only the combined assessment opens a loopback listener.
Separate TEM/PRIMO runs require startup banners and save
the complete core, CLI CPU/instruction/key output and live process registers.
PRIMO additionally passes a timestamped display/unknown-command/display exchange.
Recorded stop counts are zero and PK=1 code keys agree; descriptors are Key 0.
SOS now starts separately as well, using a new blank second disk labelled and
mounted under protection and a separately guest-generated empty SOS catalog.
Its stop count is zero, PR=8f and PK=3; saved-IC memory has Key 3 and the live
descriptor Key 0. A separate administrative FP checks the 1722-segment swpsos
entry on disc1. These checks are not protected MIPS/TS login/spool acceptance.

Process matching checks kind/name/bounds/system parent and reciprocal process
queue links (+18/+20 in montxt1). The link targets are queue fields, not process
descriptor starts. This rejects the otherwise matching descriptor copy held in
a subsystem's own buffers. tests/python/test_mips_protection.py covers live
fields, copied/ambiguous descriptors, bad links/parents/bounds and preservation
of a stopped task's saved fault context. Additional guards cover the exact demo
catalog and declared kind, SOS-parent matching and PR-based instruction access
(distinct from PK). Retained-session guards distinguish first-boot creation
from second-boot read-only verification and reject temporary/missing files or
wrong/duplicate text. A media guard accepts intentional 80-column CLI wrapping
but rejects volatile storage, duplicated devices and mismatched sidecar paths.
Fault guards require one exact break/removal pair, an even in-child next IC
matching the loaded RS program and complete core, while allowing FP's two
reserved header words. Wrong causes/PC/code/bounds or duplicate replies fail.
Updater guards additionally require its own task PL/IRQ-0 evidence, exact
catalog allocation and linked module probe, normal FP recovery and preserved
input. Ordinary monitor-call IRQ-0 entries are allowed; duplicate PL faults,
wrong context or missing module identity are rejected.
Candidate guards check protected/loopback config, distinct persistent media,
flat artifact names, automatic append printing, fixed-date direct component
reads at FP depth two and rejection of unknown bootstrap text/invalid ports.
The unprotected manual's literal remote-file blocks are also checked against
the shared retained-file dialogue. The twenty-five pure guards are registered as
sim4000_mips_protection_guards; live startup assessments stay explicit opt-in.

The combined opt-in assessment reuses the NVT-aware terminal exchange from the
printer acceptance helper, with an optional active-session snapshot callback.
It checks incorrect/correct passwords, message, lookup and finis, including
child removal. CPU protection and IX are checked during the session; all three
subsystem code keys and live descriptors are preserved. The suspended child is
checked separately: state 184 is stopped by parent, so its saved PC need not
currently carry PK. Parent-key memory is accepted only for this stopped child;
no CPU protection behavior is relaxed. Snapshot failures at the CLI save their
core without trying to suspend an already suspended emulator again.

The combined --printer extension uses the same completion verifier as the
compatibility printer acceptance, whose pure negative controls reject duplicate
completions, nonpositive transport IDs, wrong states/causes, abnormal termination
and payload leaking to the console. It compiles the unchanged example under
protection and uses rcin for source/payload transfer. Two sequential jobs (29
and 4509 bytes) complete through RC610/dev7 and match exactly before shutdown;
the output hash must survive exit. A later protected Telnet exchange checks
subsystem health. Failures preserve a core and transcript on the private media;
no output or completed-job manifest substitutes for the terminal checks.

The explicit --restart extension uses private persistent overlays for both
disks; ordinary probes remain volatile. A local FP imports/retains only a seed
file in the demo's 0/0 base interval. The remote SOS child creates keepsake via
copy, retains it with scope user and displays it with c=copy. This avoids rcin's
36-segment minimum in the account's 30-segment temporary quota. Matching-base
administrative rcout exports on both boots must equal an independent ASCII/LF
fixture exactly. The second emulator reuses the sidecars, mounts (does not
label) disc1, starts the subsystems manually and reads the existing file from a
fresh login without recreating it. Source/private-base hashes stay unchanged;
restart-result.json is only emitted after the second successful shutdown.
This is normal-exit persistence, not power-loss/crash or autostart acceptance.

The separate --isolation run assembles a fixed forbidden monitor-store fixture
with guest SLANG and retains it in the demo base interval. An execution
breakpoint filters unrelated FP-loader visits to the predicted RS address;
live W1/W2, PR=e7, task mode and Key-3 code must match. A full pre-instruction
core supplies exact code verification; a reported next IC after RS and SOS
removal after break establish the observed fault. The Key-0 sentinel word is
compared before/after and must differ from the attempted value. A post-fault
core is retained but cannot establish original code identity because SOS
reclaims the removed child's interval. Fresh login, FP operations/logout and
PRIMO operator recovery check continued subsystem health. These are bounded
monitor-store/recovery checks, not general sibling isolation or a security audit.

The separate protected candidate generator reuses the guarded catalog factory,
protected import and guest startup transforms. Write-through preparation is
restricted to new private copies. Exact rcout exports verify both installed
startup texts, and the interim psosboot is removed. Only four clean artifacts
are copied before verification; verification has its own copies, loopback port,
overlays, transcripts and user files. Two automatic boots must pass terminal/
retained-file checks and four total local PRIMO jobs with 9076 exact immediate
append bytes. The shared protected printer helper accepts a prior-output byte
prefix and a per-boot evidence directory; its original single-run callers keep
their empty-output default. Candidate/runtime base hashes must remain unchanged.
No source image/config or distribution is updated by candidate generation.

The strict protected preparation path records an upsoscat break in tofrom1,
not a failed SOS initialization. Its failure core and transcript remain private.
The explicit compatibility factory runs the same maxp-4 guest recipe used by
the existing empty-catalog verifier; it verifies CPU protection off, normal
updater completion, descriptor and exact packed bytes, then records a manifest
only after source/private-base hash guards pass. The protected import rejects
missing/mismatched manifests, changed data or a claimed protected factory.
rcin initially reserves 36 segments; guest set reduces only that newly imported
private area to one segment. Exact rcout export is required, so shrinking cannot
silently clear/truncate the catalog data. No host-side disk-image mutation or
protected updater success is inferred. Original startsos is exported before and
after creating psosboot and must match. CPU protection stays enabled throughout
the importing/initializing/SOS session. A missing startup banner is recorded as
incomplete initialization after saving state, not discarded as an unexplained
assertion or relabelled as success.

## Manual MIPS/TS manufacturing acceptance

`tests/python/test_mips_manual.py` reads the labelled text/config/guest-command
blocks directly from [the standalone manufacturing guide](../MIPS-TS.md).
It does not call either profile-generation entry point or the developer C
image builder. Private native input copies and integrated mkdisc are followed
by a real FP preparation session: kitlabel/kiton, original script exports,
guest upsoscat, exact 1536-byte terminal catalog comparison, startup replacement,
retention, normal finis and clean emulator exit. Exported startup texts must
match the manual's host input bytes. The SOS script changes only its disk
document; unchanged component scripts retain the expected sizes.

An explicit allowlist freezes bases before verification. A separate runtime
copy uses the manual's flat config with two persistent overlays. Both full
boots must reach automatic SOS startup without a date question or not-allowed
diagnostic; actual Telnet exchanges require wrong-password rejection, TEM
enrollment, unique FP message output, FP lookup and normal logout. Local
listings verify healthy services, correct child identity and child removal.
An administrative FP with matching 0/0 bases creates and retains compact text
on the first boot; it is exported exactly on both boots, without recreation
on the second. Source artifacts and all frozen/runtime base hashes remain
unchanged. This does not extend acceptance to interactive account imports,
arbitrary catalog recipes, protected MIPS/TS or crash recovery.

The test keeps preparation.log, boot1/boot.log, boot1/terminal.log and matching
boot2 evidence plus result.json under its exclusive ignored destination.
The result identifies the manual digest, consumed blocks, candidate hashes,
input hashes and passed gates. Invocation belongs in
[Development](DEVELOPMENT.md); operating instructions remain in the User Manual.

## Protection development baseline

The annotated tag `baseline-before-protection` identifies commit
`a5de1763382bd7611507e35a398ac7e27f026c25`. Before runtime changes, the focused
`sim4000_operator_acceptance` test passes: startup/date initialization, local
and remote process operation, suspension/resumption and reconnect checks.
This test requires permission to open local sockets.

An isolated Debug developer build also regenerated the monitor with
`dev/python/rebuild_monitor.py --verify-boot`, using the unchanged source
fixtures. The candidate is byte-identical to MONITOR, 37,632 bytes, SHA-256
`b5a40b73a38f65f02f47835ef1557c9989055df68c692a0719bf536ba14cfbb2`.
Candidate boot and text/all-byte binary host transfers pass. These checks do
not exercise hardware protection and do not replace the later integration gates.

Verified baseline media SHA-256 values:

| Artifact | SHA-256 |
| --- | --- |
| STDMON | `acd1f238c18357bf5b9dfa8ae9338e3775845956d7ccbd53b552d300f0652be8` |
| UNIT0 | `e5d293bd01c39385218c11531412156ae4b65afa542c1263b2dd3c919f630a1e` |
| UNIT0.original | `838621f65fef164cf55fca6ad60ff4f1b3ae5b5326fd7f29a38bfc75a4a5b22a` |

## Protection state primitives

`sim4000_protection_tests` is an opt-in native C test target. It covers zeroed
key allocation, shared halfword keys, all eight word slots including register
addresses, an odd final halfword, invalid keys/addresses, null/destroyed cores,
data writes preserving keys, and independent data/key reset. All 256 PR inputs
are checked against eight explicitly listed MSB-first masks and key-zero
invariance. Panel reset/start preserves PR and keys; full machine reset clears
keys and restores PR `0x80`.

A separate execution check sets nonzero keys and PR, then verifies that existing
`kl`/`ks`/`pl`/`ps`/`sp` compatibility stubs remain unchanged. It does not test
protection enforcement in the default profile. Additional machine-API opt-in
execution tests now cover allowed/denied stores across all keys, protected data
and indirect reads, sequential/AM fetch faults, privileged opcodes, branch/link
ordering and real protection instructions. They check `rx` register changes,
partial `ds` writes and `ns` changes before a denied count store, saved fault IC,
register key aliases, IRQ-0 mask invariance, successful instruction/panel autoload
rekeying, and host-access/write-observer boundaries. This is a subset of the
planned vector groups, not complete protected-monitor acceptance.
Additional native cases check configuration defaults, explicit modes, case and
comment handling, duplicate/invalid directives, successful data-only core loads
and malformed/out-of-range rejection with nonzero keys. Failed instruction and
panel autoload retain PR/keys/data while entering reset, without overwriting a
previous reset's saved IC. `sim4000_operator_cli` covers mode reporting, read-only
key inspection, key completion, input/range limits, 80-column output and a literal
`ks`/inspection/reset sequence under an explicit protected configuration. That
sequence is not a protected monitor boot.
Extended native coverage now checks first/last and odd-address aliases, invalid
operand/key/branch addresses, double-store register aliases and zero-address
wrap, first-word denial, `nd` side effects, fetch from a keyed working register,
indirect register stores, return through `je (saved IC)`, IRQ-0 priority over an
external request, mode loss within an AM chain, and a device poll writing keyed
storage during task execution. Allowed scalar-store cases accompany denied
cases, and `pl` checks both all-allowed and all-protected PR values. The
[protection specification](PROTECTION_SPEC.md#cpu-acceptance-coverage-m2) maps
native functions to the planned CPU vector groups. This closes the focused M2
CPU/API gate, not protected-monitor or all-peripheral integration validation.
The target and existing
`sim4000_tests` pass after the state changes, and the unchanged default profile
boots to the date prompt in a bounded run with volatile disk writes.
Allocation-failure cleanup is implemented but not fault-injected by this test.

Run this focused target explicitly after building it:

```sh
ctest --test-dir build-protection -R '^sim4000_protection_tests$' --output-on-failure
```

## Experimental protected monitor checkpoint

`rebuild_monitor.py --protected` applies a guarded transformation to prepared
copies only: remove the identified early return from `montxt10:e22`, retaining
its existing key-setting loops. The unit tests verify unchanged inputs,
unchanged other prepared sources, transformed-source hashes, and rejection
of missing, duplicate or already removed bypasses. Default source preparation
and historical reproduction remain separate paths.

Guest SLANG assembly succeeds and exports a 37,632-byte candidate, SHA-256
`baf14abb99d582ad845478034167265087e1aec9d3785da2bd88cdc78298047b`.
The candidate differs from the shipped MONITOR. Under explicit CPU protection
it reaches date initialization, attention and `new hostcheck size 100000 run`.
`claim` completes in that FP and reports its 100000-byte allocation and disk
resources; the diagnostic run observes 48,316 task-mode calls before the later
failure. The helper now requires the claim report before attempting host import.
The subsequent `hftest=fromhost input` produces `break 0 tofrom1`, called from
lines 89--91, and pauses the FP. This checkpoint therefore **fails acceptance**;
text/binary round trips and process isolation are not established.

The bounded developer diagnostics include an IRQ-0 step snapshot at PC `00d338`,
raw word `730022` (`pl w3 34`), saved IC `00d33a`, PR `bf`, instruction key 1
and task mode. The recorded sequence is:

```text
00d334: 528054  rl. w2 84
00d336: 5c8052  rs. w0 82
00d338: 730022  pl w3 34
00d33a: a3004f  sh w3 79
00d33c: a3003b  sh w3 59
00d33e: 34802e  jl. w0 46
```

The PL is reached as code with W3=`02314f`, not merely decoded from a data dump.
Under the documented RC4000 rules it must raise instruction exception in task
mode. The following comparisons select a different path only for W3 in 60--79;
with the compatibility PL no-op, the retained high W3 value takes the branch.
This resembles a machine-feature probe, but its intended later-machine semantics
and library provenance have not been independently established.

`runtime_probe_privilege_contract` reproduces this literal sequence in native
tests: compatibility mode preserves W3 and takes that branch; protected task
mode faults at PL with the advanced IC and unchanged W3/PR. Both expectations
pass. This narrows the conflict to the guest routine's PL use under strict
RC4000 protection; it is not evidence for allowing arbitrary task-mode PL.
No CPU privilege check has been weakened. Repeating with IX enabled as in the
normal frontend produces the same guest failure.

The first verification attempt also exposed the helper's dependence on fixed
legacy idle-loop addresses and timed out after the break. Protected verification
now uses the generic input/quiet-output completion path and fails promptly on
a guest FP break. It requires actual protection, task execution and nonzero keys
before reporting success; a generated candidate/configuration is not a pass.
All experiments use copied base media and volatile writes. The shipped monitor,
monitor sources, disk images and default configuration remain unchanged.
This initial failed workflow is not the selected M3 profile; later bounded
workflows and broad acceptance are documented below.

## Existing Algol runtime library inspection

`sim4000_runtime_libraries` invokes the developer helper's `--inspect-libraries`
mode on the unchanged monitor and native UNIT0. Guest binary exports use a
volatile overlay and a private host directory. Source monitor/image hashes
must be unchanged afterwards. The exported artifacts are:

| Guest area | Bytes | SHA-256 |
| --- | --- | --- |
| oldalgollib | 38400 | `3006c1589ff38b16005ad0ce75c920892857f996b7a24c96cf453fc62969a043` |
| algollib | 52992 | `c95314f9ec95d97eec7a64d897ae2ef42c486442a17cb9b9ce7ae2634db7d557` |

The test checks the existing module labels: old `tofrom` in block 14, new
`tofrom1`/`tofrom2` in blocks 17/18, and `system0` in block 29. The exact
word-aligned bytes `730022 a3004f a3003b` occur at byte offsets `0x339c` and
`0x590d` in algollib, and not in oldalgollib. These are binary inspection
assertions, not proof that the entire older library works with protection.
No library, catalog entry or shipped image is modified or promoted by this test.

The [protection specification](PROTECTION_SPEC.md#runtime-opcode-conflict-and-older-library-candidate)
records the primary-manual evidence: RC8000 opcode 28 is GG, distinct from
RC4000 PL, while the provided edition does not define register 34.

The inspection also exports the installed transfer binaries and guest lookup
descriptors. `fromhost` is 42,240 bytes, with tofrom1/tofrom2 in blocks 40/41;
`tohost` is 36,096 bytes, with those modules in blocks 39/40. Their aligned
probe offsets are respectively `0x500d`/`0x789c` and `0x4d0d`/`0x759c`.
The modules are linked into these programs, not rebound by later catalog edits.
The older tofrom header is `00c00c`, versus `00e00e` for tofrom1; their first
18 parameter-entry instruction words match before the implementations diverge.
The test asserts these fixture-specific facts, not a general Algol object format.

### Older-tofrom protected transfer checkpoint

The developer `probe_rc4000_runtime.py` workflow creates its own writable copy,
uses guest FP set/Algol to link unchanged sources into separate `rcin`/`rcout`,
restores the original tofrom catalog binding, and retains only these new
programs in user scope. The derived older binding is
`tofrom=set bs oldalgollib 2048.22 83.2665 0 46.14 1.0`.
No host-side library/catalog byte patching or CPU privilege exception is used.

Both compilation/compatibility checks and a separate protected reboot pass.
The protected session selects candidate
`baf14abb99d582ad845478034167265087e1aec9d3785da2bd88cdc78298047b`,
initializes the date, creates hostcheck and runs claim, then checks:

- Text input `Native host file exchange.\r\nSecond line.\n` returns with LF
  normalization, despite the padded guest file allocation.
- A 768-byte `bytes(range(256)) * 3` binary input returns byte for byte as the
  prefix of an integral number of 768-byte segments.
- Actual protected task activity: 168,897 observed task steps and 50,000 words
  with nonzero keys. These counters are evidence, not a process-isolation test.
- Protected verification does not change the private base image hash. Source
  MONITOR/UNIT0 and the candidate hash remain unchanged.

The new programs occupy 54/46 segments (41,472/35,328 bytes) and each has one
older tofrom module instead of tofrom1/tofrom2. The newer nsystem stays linked;
the known remaining GG probes at `0x500d`/`0x4d0d` are in system0 and are not
executed on this transfer path. Their presence is checked rather than hidden.
The helper also counts actual instruction steps with raw word `730022` during
protected verification and requires zero, in addition to validating transfers.
The helper validates the new descriptors and restored original tofrom binding.
The default transfer programs and both library areas remain installed unchanged.

An independent trial aliasing nsystem to the existing old system descriptor
breaks in system2 before a transfer, even with protection off and IX on. The
cause remains open; this alias is not part of the passing workflow. There is
no claim of general old/new-library interchangeability or M3 acceptance.

### Protected local process lifecycle

The same developer probe with `--lifecycle` passes on the candidate above.
Its helper enables the protected CPU profile and IX and uses volatile writes
over its independently built private image. It verifies:

- Original FP `claim`: first 44840, size 100000. All 50,000 word keys are 1
  while running, 0 after stop, 1 after resume and 0 after removal. The two
  adjacent word keys retain their running-checkpoint values at each transition.
- Text and binary files survive stop/resume and export with the independently
  checked expected contents. They are then explicitly cleared in FP.
- Removed hostcheck is absent from the system `list` output.
- Recreated hostcheck: first 44840, size 100006. All 50,003 word keys are 1;
  the non-ten-word remainder is covered. Fresh text import/export succeeds.
  Final removal returns all those keys to 0 and preserves adjacent keys.
- Final observation: 344,222 task steps and zero executions of raw `730022`.
  The initial transfer checkpoint observes 50,000 keyed words; no assertion
  requires freed process areas to retain nonzero keys after removal.
- Every resumed/recreated host export is content-checked separately, and the
  private base disk, candidate and shipped-media hashes remain unchanged by
  protected verification. Helper unit checks reject missing/corrupt exports,
  truncated or misaligned binary output and destination reuse.

Bounds are parsed from fresh guest claim reports and checked for alignment and
core limits before scanning; these addresses are evidence, not helper constants.
This does not cover short child areas, nested processes, siblings, Telnet,
deliberate cross-process accesses or the complete M3 gate.

An earlier direct-removal trial retained the old temporary hftest/hfbin catalog
entries, which remained visible after same-name recreation. The passing
lifecycle checkpoint performs explicit FP cleanup before removal and verifies
their absence afterwards. It is not evidence that system-level process removal
alone reclaims temporary files; whether further monitor/FP policy work is
needed remains separate from the demonstrated core-key transitions.

### Protected independent-process fault isolation

The developer probe's separate `--isolation` run passes with the same protected
candidate and privately rebuilt transfers. Both siblings remain alive on
independent console devices: hostcheck on device 2, attacker on device 4.
Fresh claim reports identify hostcheck `[44840,144840)` with key 1 and attacker
`[144840,244840)` with key 2, each covering 50,000 words. Bounds are read from
the guest and validated for disjointness; these are not helper constants.

Four LF sources are generated exclusively inside the private host directory,
imported with rcin and assembled by protected guest SLANG (`slang ok 1/16/1`).
Each first loads the foreign last word at `0235c6` as ordinary data, which is
permitted by the RC4000 architecture. It then attempts one prohibited operation:

| Probe | Opcode | Expected effect | Observed task fault |
| --- | --- | --- | --- |
| RS w2 x1 | 23 | Deny a different value being written into the victim | IRQ 0 at `023bd2`, EA `0235c6` |
| JL w3 x1 | 13 | Deny entry into the victim's code/storage key | IRQ 0 at `023bd2`, EA `0235c6` |
| PL w0 0 | 28 | Deny a privileged PR load in task mode | IRQ 0 at `023bd2`, EA `000000` |
| KS w2 x1 | 51 | Deny changing the victim word's key to zero | IRQ 0 at `023bd2`, EA `0235c6` |

Every matching fault has key 2, PR `df`, task mode and saved IC `pc+2`.
PR protects victim key 1 but permits attacker key 2. W0 retains the preceding
foreign read value; RS carries a different W2 value and KS carries zero.
The helper arms a dedicated observation before each run so monitor calls or
unrelated IRQ-0 snapshots cannot substitute for the intended failure.

After every complete `pause attacker break fp` notice:

- All 50,000 victim data words equal their pre-probe snapshot and all victim
  keys remain 1. No sentinel is seeded by a host write.
- The monitor has stopped the attacker and returned every word of its area
  to parent key 0. A contiguous boundary changes accordingly; this is checked
  explicitly rather than treated as victim corruption.
- The still-running victim exports its original text to a distinct after0--3
  host file, and Python verifies each result independently.
- The system restarts attacker; FP reports reinitialized and all attacker keys
  return to 2. The next probe is imported/assembled after recovery.

The final run observes 511,680 task steps and zero executions of raw `730022`.
Protected verification does not change the private base image, candidate or
shipped media. No CPU implementation or privilege rules are changed. The helper
unit checks also reject each missing/corrupt after-fault export and reject
combined lifecycle/isolation modes before creating a destination.

This does not validate live TCP/Telnet transport, descendant processes, every
instruction or arbitrary shared software. This isolation checkpoint alone does
not cover protected guest Algol compilation; see the separate checkpoints and
integration acceptance below.

### Protected guest Algol compilation and rebuilt transfers

The separate `--algol` probe passes with the same protected candidate, IX on,
the privately built rcin/rcout bootstrap and volatile verification writes.
It runs the following work under actual protection, not in the compatibility
bootstrap session:

- Import `hello.alg`, compile it to `hello` (`algol end 24`) and execute it,
  requiring `Hello from Algol` in this command's new output.
- Import `protected_algol.alg`, compile it to `arith` (`algol end 24`) and
  execute it, requiring `Protected arithmetic ok`. The program sums 1--10,
  calls a value-parameter integer procedure to double 55, and checks that
  real multiplication `1.5 * 2.0` equals 3.0 before printing success.
- Export both compiled programs: each occupies 24 segments / 18,432 bytes.
- Import the unchanged fromhost/tohost sources, select the older tofrom
  binding, and compile separate `pcin`/`pcout` (`algol end 54`/`46`).
- Restore the original tofrom binding and verify it in fresh guest lookup
  output, without changing either installed transfer program.
- Use pcin/pcout for fresh text and binary round trips, checking LF-normalized
  text and the 768-byte binary prefix plus segment alignment independently.
- Export pcin/pcout: 54/46 segments (41,472/35,328 bytes), each with the single
  older tofrom module rather than tofrom1/tofrom2. Remaining known GG probes
  are confined to system0; the execution counter is zero throughout the run.

The arithmetic fixture SHA-256 is
`e0d299950e4860b78b66e1ab308416b1d18cd020ff81cd43a8d0eece33e66409`;
hello is `8cbd104d81d177ea5469649bb9153b9001042053f73b02c7d4aacfc602e53f88`.
The final observation is 3,055,865 task steps. Protected verification leaves
the private base disk, candidate and shipped media unchanged. Unit checks
reject missing/corrupt exports, wrong binary contents/alignment, wrong compiled
allocation/module layout and conflicting probe modes before filesystem writes.
Synthetic unit records exercise host validators only; they are not guest
compilation or execution evidence.

Both protected transfer compilations report the literal diagnostic
`9. oldalgollib bases`. Its meaning/severity is not established by these tests;
do not silently classify it as harmless or claim a generally clean compiler
profile. The subsequent compilation summaries, module checks and executed
transfers demonstrate this bounded path only. Lookup after restoring the
binding shows system and temporary tofrom descriptors with the original
values. More general catalog-base/scope behavior remains outside acceptance.
Live Telnet, nested processes and broad integration acceptance have separate
checkpoints below; this temporary-binding test alone does not cover them.

### Library-base diagnostic comparison

The developer probe with `--algol --system-binding` retains the older tofrom
system binding on an exclusive private image through guest FP set. The paired
plain `--algol` run instead introduces its older binding in a normal FP session.
Both runs use the same protected monitor, source fixtures, compiler and CPU
rules, and independently verify the program exports and text/binary transfers.

| Selected older-tofrom descriptor | Bases (signed decimal) | Permanence key | Observed `oldalgollib bases` messages |
| --- | --- | --- | --- |
| Temporary alias in normal FP | 8388605, 8388605 | 0 | 2: one per transfer compilation |
| System descriptor in derived copy | -8388607, 8388605 | 3 | 0 |
| oldalgollib area (both runs) | -8388607, 8388605 | 3 | Reference library descriptor |

Guest lookup supplies these descriptors; the helper also validates the matching
system ranges/keys in both the private image's device and main catalogs. The
derived fixture SHA-256 is
`ebc56434088ac0d0a5655924646a45bfd3d739bbb2217f45622c9626641f2483`.
Only the private profile retains the changed tofrom binding. The original
transfer binaries and both library areas remain installed, and source UNIT0
and MONITOR hashes remain unchanged. Protected verification is volatile and
must leave the derived base hash unchanged too.

The system-binding run again executes hello/arithmetic, compiles the unchanged
transfer sources to 54/46-segment pcin/pcout, then executes their text and binary
round trips. Final task-step observation: 3,048,496; remaining GG-probe executions:
zero. The temporary-binding control repeats its two diagnostics and passes the
same bounded checks. Validators require exactly two mode-specific observations
and the expected zero/two diagnostic occurrences, rejecting missing, reversed
or additional evidence. Invalid mode combinations are rejected before writing.

This controlled result supports a catalog-scope explanation for the message;
it is not an authoritative definition of historical diagnostic 9 and does not
prove arbitrary scope/library compatibility. The derived profile is not a
replacement for the shipped image or complete M3 acceptance. Lifecycle/isolation
results above use their own independent fixtures. The live Telnet checkpoint
below covers the system-binding profile; descendants remain open.

### Protected live Telnet lifecycle and reconnect

The manually invoked `tests/python/test_protected_telnet.py` passes with the
identified protected candidate and system-binding profile above. It refuses
unknown monitor/profile hashes and an existing destination, creates its own LF
configuration, runs PROTECTION MODE=RC4000 with volatile disk writes and binds
a single classic console endpoint only to loopback. No CPU implementation,
default configuration or shipped media changes are needed.

Read-only snapshots and fresh guest claim reports verify these observed records:

| FP | Descriptor | First, top | Parent descriptor | Stored PR | Key |
| --- | --- | --- | --- | --- | --- |
| local, device 2 | 11586 | 44840, 144840 | 11438 (s) | bf | 1 |
| remote, Telnet device 4 | 11734 | 144840, 244840 | 11438 (s) | df | 2 |

Descriptor candidates must have internal kind, exact name/bounds and an s
parent; name-string matches alone are not accepted. The two stored PRs permit
their own key and protect the sibling key. Operator key inspection samples
the first/middle/last word of both areas. Operator PR=80 in the idle monitor is
not substituted for the saved per-process PR. Full-area key scanning belongs
to the independent C-helper lifecycle/isolation checks, not this TCP test.

The test verifies Telnet negotiation, Control-A attention and NVT CRLF input,
then imports/exports different local and remote text. After explicit remote FP
file cleanup, system-level removal completes, a fresh list excludes remote,
its descriptor name is cleared and all three sampled area keys return to zero.
The local descriptor, bounds and key samples remain unchanged, and its original
file exports correctly while the remote FP is absent.

Remote recreation yields a valid separate descriptor/area/key again (the same
addresses were reused in this run, but reuse is not required by the script).
Fresh remote text transfer passes. Disconnect moves the endpoint to listening;
a new connection increments accepted from 1 to 2 without process recreation.
The remote FP's pending read and imported file survive, and a new export plus
another independent local export pass. All six host results are content-checked.
Fresh claim/snapshot inspection after reconnect must reproduce both complete
descriptor records from before disconnect, including PR/key and parent.
There are no drops or unexpected guest breaks in the recorded run; unknown
claims, missing/corrupt exports and an unknown candidate are covered by helper
unit rejection checks without opening sockets.

The simulator exits cleanly with original PTY settings restored. Candidate,
derived base, shipped MONITOR and UNIT0 hashes are unchanged. Configurations,
raw transcript and packed snapshots stay in the ignored private work directory;
the success marker is written only after all content and hash checks pass.
The registered CTest count is unchanged: this generated-fixture integration
script is explicitly invoked, not part of an ordinary build or default CTest.
This is not a V2/RC851, adversarial-network, all-key-range or nested-process
acceptance claim. The complete M3 evidence combines this check with the other
bounded workflows and broad acceptance below.

### Protected nested guest processes

The separate developer `--nested` probe passes on the identified candidate and
system-binding profile. It creates a fresh key-2 parent with `key 2 internal 2`
for each case. Parent bounds/descriptor come from a fresh guest claim and a
unique kind/name/bounds scan; PR/key must be `9f/2`. The helper only observes
core and generates source text: the guest parent creates the real child through
monitor calls, rather than a host-constructed descriptor or injected state.

Every generated program first requests PR 80/key 1, which permits keys outside
the parent's allocation; create must return 1. A subsequent corrected PR bf/key 1
request must succeed, with a unique child descriptor pointing at the real parent.
The child executes its own one-word relative self-loop in task mode with PR bf.
Sizes 2, 20 and 26 RC bytes test 1, 10 and 13-word areas. The child starts at
parent top minus 26; each first/top remains inside the 100000-byte parent area.

| Phase | Parent area excluding child | Child area | Additional evidence |
| --- | --- | --- | --- |
| Running | key 2 | key 1 | Actual task self-loop steps, child PR bf |
| Whole tree stopped by s | key 0 | key 0 | Child counter remains frozen |
| Whole tree resumed by s | key 2 | key 1 | Child execution resumes |
| Child stopped by its guest parent | key 2 | key 2 | Normal stop answer waited/released; counter frozen |
| Parent recursively removed by s | key 0 | key 0 | Both descriptor names cleared |

Every word in the full parent area and its two outer boundary keys is checked,
not just the child endpoints. A guest countdown, not a host-written flag, takes
the parent from running to its own child-stop call. The parent uses the monitor's
normal message buffer/answer path and restores its buffer/area claim word after
waiting. Repeated fresh parents verify descriptor/claim reuse after recursive
removal. The test is scoped to these depth-two trees, not arbitrary nesting.

Observed child execution counts are 1,502,590 / 1,499,940 / 1,498,967 for the
three cases; final task observation is 9,789,074, with zero remaining GG-probe
executions. Counts are diagnostics, not hardware timing promises. The validator
requires every size/transition exactly once, positive child execution counts,
denied-widening and released-answer markers plus final completion. Unit checks
reject missing/duplicate phase evidence, zero execution and conflicting modes.
Original media, candidate and the private verification base remain unchanged.

Parameter construction follows the maintained monitor: a44/a45 are the final
words of double-word intervals, requiring offsets 72--78. A separate forward
label identifies the protection parameter, avoiding SLANG's forward-expression
restriction. No monitor/CPU exception or host mutation is used to accommodate
the generated fixture.

This completes the bounded M3 functional set. The integration acceptance below
adds broad regression/sanitizer evidence; default promotion remains a separate
decision.

## Instrumented protected-profile checks

The M3 integration run uses separate GCC Debug and Clang 22.1.8 Debug builds
on Linux x86_64. The complete normal registered suite passes 29/29 tests.
The instrumented profile enables AddressSanitizer, leak detection and
UndefinedBehaviorSanitizer with abort/halt on error; guest workflows use the
same environment as the registered suite.

The first full sanitizer run passes 27/29. Both failures are the same manual
helper's 120-second wall timeout (directly and through the standard-monitor
wrapper), not sanitizer findings. The manual budget is raised to 300 seconds
per variant and the two-variant wrapper to 660 seconds; content assertions and
guest instruction limits are unchanged. Serial targeted repetition passes both
tests: 287.99 seconds for the direct manual test and 711.88 seconds for the
standard-monitor test including its other workflows. Thus all 29 registered
tests have successful sanitizer coverage, not a claim of a single initially
green full run. Source-only manual and rebuild-guard checks also pass after
the documentation updates. M3's bounded integration gate is complete.

The instrumented guarded monitor rebuild reproduces candidate
`baf14abb99d582ad845478034167265087e1aec9d3785da2bd88cdc78298047b`.
Separate instrumented probes pass Algol/system-binding, nested processes,
sibling isolation and local lifecycle. The Algol/system-binding base remains
`ebc56434088ac0d0a5655924646a45bfd3d739bbb2217f45622c9626641f2483`;
the compatibility-binding lifecycle/isolation bases remain
`05fad5b8cc6269a4de22fc6c93c992f001298525c92ecf4abd30b59f5b73e91c`.
Live Telnet also passes using the instrumented runtime and identified
system-binding base. These runs retain all bounded assertions described above;
they add host-memory/undefined-behavior coverage, not new hardware semantics.

A sandboxed rebuild reaches successful guest assembly but LeakSanitizer cannot
inspect the exiting process under that sandbox's ptrace restriction. It is not
counted as passed. Repeating in a permitted environment with leak detection
still enabled passes. Standard media hashes and maintained monitor sources
remain unchanged; no candidate is promoted and no PDF is rebuilt.

## Selectable maintained-monitor workspaces

`prepare_monitor_profiles.py` accepts only the verified original monitor,
guarded protected candidate and M3 system-binding disk. Its unit checks (in
`test_rebuild_monitor.py`) cover explicit profile selection, LF/relative paths,
CLASSIC terminals, non-write-through defaults, invalid profile names, unknown
monitor/disk rejection before destination creation, exact copies and refusal
to overwrite existing work. Synthetic accepted identities are patched only
within the isolated copy test; production input guards remain fixed.

Both generated configurations pass the native configuration validator. Separate
bounded runs of four million interpreter calls, launched with an unrelated
working directory, reach `initialize date:` through the configured monitor and
disk without command-line media overrides. Protection selection is OFF versus
RC4000, IX is on in both. Every prepared file's hash remains unchanged after
these volatile-overlay boots. This verifies preparation/startup, not overlay
persistence, complete interactive operation or distribution inclusion. The
source-built protected profile's deeper guest assertions remain those above;
no binary-only V2 monitor is involved.

## Protected command-driven editor

`tests/python/test_protected_editor.py` passes with the supplied protected
monitor/system-binding disk and, separately, with the unchanged compatibility
media. This explicitly invoked frontend test is not a registered workload.
Its only profile differences are monitor/disk selection, protection OFF versus
RC4000 and the corresponding `fromhost`/`tohost` versus `rcin`/`rcout` transfers.
Both use default IX, unthrottled interactive execution and volatile disk writes.

A fresh 100000-byte user FP imports the manual sample and multiline fixture.
Four editor invocations must each report `edit begin.` and `edit end.`:

- Locate/replace Monitor with Edited; retain the second line.
- Advance two lines, delete the selected line, insert two lines including a
  literal comma/slash and finish with the untouched final line.
- Open that guest result as new editor input and replace Inserted with Reopened.
  Export both the new result and unchanged saved intermediate independently.
- Open the original multiline file again and truncate after the first two lines.

Eleven independent host content exports must match their expected bytes exactly,
including both original guest sources and the saved intermediate. Both host
input files must also remain unchanged. Temporary guest entries are explicitly
cleared only after editor completion/exports. The helper requires actual
protection selection, IX on, volatile-overlay media and key-1 samples at the
claim-reported first/middle/final process words before and after the workflow.
This is sampled state inspection, not full-area isolation coverage. Pure guard
tests reject every missing/corrupt export/input and preserve an existing work
directory. The simulator exits cleanly with its PTY restored; all standard,
protected and historical base-media hashes remain unchanged, also on failure.
Logs/configuration/exchange files remain in an exclusive ignored work directory;
the result marker is written only after all content/state/hash checks succeed.

This certifies these bounded command-driven `edit` paths, not full-screen
editing, every editor command, unfinished-edit recovery or restart persistence.
No CPU, device, monitor, library or shipped disk is changed to make it pass.

The extended check adds text display/copy/rename/concatenation. The renamed
copy is one logical segment versus the imported source's 36; identical text
exports must not be confused with identical allocated disk areas. The old
name becomes unknown, concatenation exports exactly two samples, all compared
inputs remain intact and explicit cleanup restores the initial disk/claims.
Compatibility mode additionally checks `cmp` identical text and precisely one
first-line replacement, including the two named text excerpts and exported
report. A pure report guard rejects altered text/line/type/count and duplicate
discrepancies; existing export guards cover all eleven results.

Protected `cmp msrc moved` instead produces `break 0 tofrom1`, callers at
lines 219-220 and 380-381, then `pause edcheck break fp`. This is a known
unsupported workload, not a passing comparison. The frontend check alone
does not localize the CPU fault; see the instruction-level probe below.
The ordinary protected helper skips `cmp`. Its explicit
`--check-cmp-break` mode requires this break, reinitializes FP via Escape/`s`/
`proc edcheck start`, then checks byte-exact exports of all three surviving
texts, subsequent editing and complete cleanup. No comparison report or
identical-text result is accepted on that path. Combining the negative mode
with compatibility is rejected. Logs retain the break/recovery; the success
marker explicitly identifies the comparison limitation. This does not prove
recovery from arbitrary faults or resolve the installed binary's runtime
compatibility. Default/protected negative and compatibility runs pass their
separately stated expectations without changing shipped artifacts.

`dev/python/probe_protected_cmp.py` and the developer helper's explicit
`--probe-protected-cmp` mode independently localize this break. The observer
records task PC `00fa28`, raw `730022` (`pl w3 34`), effective address `000022`,
saved PC `00fa2a`, PR `bf`, key 1 and monitor mode false. Adjacent live words
are `a3004f`/`a3003b` (comparisons with 79/59). The saved IRQ-0 PC advances by
two as expected for the denied privileged instruction. This observation does
not change guest state or add a task-mode PL exception.

The 80-segment exported comparator has SHA-256
`61a3dec82ba92ea0d896dc217fd612c07f02d08fed0e7a00de268aaf3feb4ff3`.
Aligned newer-machine probe sequences occur at byte offsets `0x9b0d` (system0)
and `0xb49c` (tofrom1); linked tofrom2 is also present. Its descriptor is
system-scoped and recovered original/copy exports match the input exactly.
The helper bounds the circular IRQ-0 observations to the retained 32 entries;
it must not reread wrapped slots as additional faults. Pure guards reject
missing/duplicate markers, changed IRQ state/instruction/saved PC, malformed
descriptor/binary/module evidence, altered input/exports and workspace reuse.
All source media remain unchanged. This is a localized incompatibility, not a
source rebuild or a successful guest comparator. Matching comparator source
has not been identified locally, so relinking remains deferred; merely changing
the current tofrom catalog descriptor cannot alter this linked binary.

The editor/text checkpoint also checks the host fallback directly after
simulator exit. Standard `cmp -s` and `diff -u` distinguish the original/copy
(status 0), the single first-line edit (status 1) and an intentionally missing
file (status greater than 1). The diff hunk is matched against literal expected
old/new lines and unchanged second line; no localized error wording is used
as an oracle. The C locale makes retained diagnostic logs deterministic in
language, not in absolute workspace paths. All content exports are rechecked
after these read-only host operations. Pure mocked guards reject incorrect
statuses, unexpected ordinary stdout/stderr, changed diff text and an existing
negative-control filename, without making host tools a default CTest dependency.

A deliberately colliding export of edited guest text to existing `cmporig`
must yield exactly `*** cannot open or create host file, result -8388608`.
Its bytes, inode, size, mode and modification time must be unchanged; subsequent
guest lookups, exports and cleanup must still work with disk/claims restored.
Both profiles pass this non-overwrite and host-fallback check. This is bounded
regular-file collision coverage, not a new concurrent/race/symlink guarantee.

## Separate-profile overlay persistence

`tests/python/test_profile_persistence.py` passes an explicitly invoked four-
session frontend checkpoint: write/restart for compatibility and protected
profiles. A fresh standalone directory contains the copied executable and both
maintained-monitor media/configuration sets. Only that copy's disk declarations
gain separate relative overlay paths; pacing is disabled for bounded test time.
Restart uses an unrelated working directory, without media-path overrides.

Each profile imports distinct ASCII text and a different 768-byte all-values
binary sample into the same guest names. Fresh lookup must report temp/key 0.
After `scope user`, complete descriptors must differ only in scope/permanence
key 3: area size, document, date/flags, extended allocation references and the
two default base values remain unchanged. Normal FP `finis` removes the process
before orderly simulator exit. After restart, both retained files must be
visible to a new FP with matching claims/intervals and exactly equal complete
descriptors. All parsed fields are recorded in metadata.json; the parser rejects
missing/duplicate descriptor lines, wrong scopes/keys/base pairs and invalid
area size/document. The catalog key is not the CPU storage key.

Before/after text exports are exact, binary prefixes preserve all byte values,
and binary exports are segment-aligned and byte-identical across restart,
including nonzero allocation padding. Host inputs remain unchanged. Both
sessions require the selected protection mode, IX on and persistent-overlay
media. Protected process first/middle/final words retain key 1. Media-state
assertions account for the CLI's 80-column wrapping, which can split a token.

Both sidecars must be nonempty and distinct. Cross-profile startup attempts are
then rejected in both directions with `overlay does not match the base image`;
neither overlay may change. Original and copied executable/config/media hashes
remain unchanged throughout. Pure guards reject missing/corrupt text/binary
exports/inputs, changed binary padding and reuse of an existing workspace.
Each frontend session must exit cleanly and restore its PTY. Only the complete
four-session plus negative-control success writes result.txt. Generated logs,
runtime copies and overlays remain ignored; no new default CTest workload is
registered. This does not establish crash durability, arbitrary process/catalog
intervals, simultaneous writers, profile interchangeability or DFC403 persistence.

## Protected catalog and resource tools

`tests/python/test_protected_catalog.py` passes separately with protected and
compatibility monitor/software profiles, using the real frontend and volatile
writes. It requires `catstat` completion (`end 18`), all 72 segment headers,
each per-segment count and every name/segment occurrence to match an independent
decode of the freshly exported packed main catalog. The decoder scans fifteen
17-word entries per segment, skips only the all-one free marker and decodes
the four name words in high-byte-first order. `cata` must report 72 segments,
1080 slots and the independently calculated free-slot count. Read-only tools
leave the complete packed catalog bytes unchanged between two guest exports.

The offline base and active session are not identical snapshots. The protected
base has 422 stored entries; startup removes temporary `hftest`, `hfbin`,
`fhsrc`, `thsrc` and creates `c`/`v`, yielding 420 session entries and 660 free
slots. The compatibility session has 418 entries/662 free slots. Retained
name/segment occurrences must match the offline base exactly, filtering catalog
permanence key zero in both the offline rows and packed data. This is not proof
that all temporary disk remnants are accessible or that every descriptor field
is unchanged across bootstrap. It preserves the distinction between stored
snapshots, live lookup and the device catalog.

Repeated bare `show` and `claim` are stable. A host import into `spacetmp`
allocates 36 segments: free disk space drops by 12 slices/36 segments, and the
temporary claim drops by those amounts and one entry. Login/permanent claims
and the process resource header remain unchanged. The text export is exact;
after `clear temp`, a fresh lookup must report precisely
`***lookup spacetmp unknown`, and disk/claim counts return to their initial
values. The initial protected free-space report is 991 slices/2973 segments;
the test primarily asserts units/deltas, not a universal free-space constant.
The packed-catalog immutability assertion precedes allocation, avoiding a
false claim that cleanup erases every stored stale descriptor.

Final FP lookup, CPU protection/IX selection, volatile-overlay state, protected
first/middle/final key-1 samples, clean exit/PTY restoration and unchanged media
are required. Pure guards reject wrong catalog size/empty names, incomplete/
duplicated segment headers, inconsistent entry totals or report indentation,
wrong completion codes, malformed resource rows/units and an existing workspace.
The success-only result records profile/media identities and counts; artifacts
stay ignored. This does not certify arbitrary arguments, base intervals,
fragmentation, all catalog fields, another controller or every resource claim.

The extended frontend check also verifies alphabetical `catsort` system rows
against offline device-catalog names/intervals: compatibility has 414 ordinary
system entries, protected has 416 including `rcin`/`rcout`. Console and exported
descriptor rows agree exactly, including metadata and order. Temporary imports
`saved`/`gone` start in the temporary report; after `scope user saved`, only
`gone` remains temporary. Default/project and explicit user/perm groups retain
`saved` while also reporting the separate temporary group. `search` console
and exported reports independently match each expected scope. A temporary
export includes its own result and can include one `wrk` scratch descriptor;
neither remains in a fresh listing after cleanup. The special `catalog` entry
is absent from system reports and its lookup interval is checked separately;
its inline `; ***` label is not accepted as a general error exemption.
Both samples export byte-exactly after reporting; demotion/cleanup restores
the initial disk space and complete resource counters. Pure parser guards
cover full-row metadata changes, numeric type markers, eleven-character names
touching `=` and duplicate descriptor names. These checks do not establish
arbitrary catalog intervals, selectors or sort keys.

## Protected FP command-file workflows

`tests/python/test_protected_commands.py` passes separately with protected and
compatibility profiles through the real interactive frontend. Nine reviewed
fixtures supply one nested input/output call, direct/nested missing-program
continuation, successful/unsuccessful conditional lookup with skipped groups,
and valid/invalid SLANG assembly with guarded binary export, plus redirected
`list.yes` assembly from an FP command file. The test process
has a 100000-halfword core area and a bounded 500-segment/20-entry temporary
claim to hold these fixtures together. Input copies change only `=tohost`
to `=rcout` in the protected profile; all source fixtures/media stay unchanged.

The nested redirected report must contain exactly the expected three ordered
descriptors, absent from console output. Keyboard lookups after command-file
EOF establish both input return and output restoration. Direct and nested
missing-program cases must continue with the exact ordered diagnostics and
later lookups; the conditional example must yield only its two intentional
unknown-name errors, never the skipped missing-program call. The assembly
case requires its undeclared identifier, exact error/success statuses and
partial/successful descriptors. No `badcheck` host export may exist, even empty;
`checkbin` must be exactly 768 bytes with the six independently specified
24-bit instruction words at its beginning. Padding bytes are not asserted to
be zero. The default checkpoint does not execute either assembled program;
the separate `--execute-slang` path below runs only the valid listed sample.

The `slang_commands.fp` copy redirects to `slist`, assembles with `list.yes`,
restores console output and exports separate text/binary files. Four ordered
instruction rows and exactly one `slang ok 1/12/1` must appear in the listing,
not on the console; CR/NUL/end-marker bytes are rejected in host text.
The separate 768-byte binary has the six expected instruction words, matching
the guarded assembly's meaningful code prefix, not arbitrary allocation
padding. The source is re-exported and byte-exact against the reviewed input.
A subsequent keyboard lookup must show executable `slprog`, entry offset 4,
length 12. Listing/source/binary checks repeat after clean simulator exit.
Pure guards reject missing/corrupt listing, source or binary, changed order/
instructions/status, duplicate report content and CRLF text.

After explicit removal all thirteen imported/report names and both guarded assembly
products are unknown; disk space and every claim counter return to baseline.
CPU mode/IX, volatile media, protected first/middle/last key-1 samples, clean
exit/PTY restoration and source/input hashes are required. Pure guards cover
event order, missing/corrupt/wrong-size binaries, forbidden failed exports
and workspace reuse. These are bounded workflows, not general stop-on-error,
rollback, arbitrary recursion, syntax/I/O error recovery or subsystem startup.
The test is opt-in and does not add a default CTest workload. No full
SLANG-language claim follows.

Both profiles pass the explicit `--execute-slang` extension. The compiled
integer sample reaches W0=444/W1=321 and a relative self-loop. Its observed
IC must be even and inside the allocated process area; the four decoded code
words at IC-6, IC-4, IC-2 and IC must be exactly `2c007b`, `2d0141`, `1c0002`,
`348000`, with exact consecutive addresses. Another 257 interpreter calls
must preserve IC and the expected registers. The initial advance is bounded
to one million calls, not a performance benchmark. Protected observations
require PR bf, task mode and code key 1; compatibility mode makes no task-mode
claim from a field that is inactive without protection. Debugger stopped=yes
and no CPU exceptions are expected. Only normal guest execution/stepping and
read-only inspection are used, not host register/code/key modification.

After continue and guest attention, `proc cmdcheck break` must produce exactly
`***break 8 ADDRESS` (ADDRESS equals the observed loop PC) and the complete
process pause. Another attention/system selection and start reinitialize FP
with exactly the expected diagnostic. Subsequent lookup, nested/conditional
command files and assembly checks still pass; final explicit cleanup restores
initial disk space and claims. Pure state/code guards reject wrong arithmetic,
mode/key-register state, exception state, odd/out-of-area IC, wrong instruction
words and duplicate/moved code addresses. This is a bounded interrupt/FP-return
recipe for this known executable, not generic machine-code recovery.

## Initial MIPS/TS inventory and component startup

The developer-only inventory exports eight text files, four packed binaries
and lookup descriptors through the guest's standard host-file utility. It
does not execute the startup/generation material. Every experiment gets new
private copies of both MONITOR and UNIT0 and its own exchange/configuration;
inventory uses volatile writes and checks original and copied base hashes.
The copied scripts retain their exact text; no configuration/generation script
is reimported over an installed entry. Binary size checks establish complete
exports, not successful execution or a source match.

The explicit `--start-components` option boots another volatile session on
those copied bases using the real frontend. Guest system s reads starttem,
then startprimo. Completed s commands use ready rather than an FP BEL boundary;
attention/s selection is repeated before each later system command. TEM reports
release 5.1 and started; actual process rows have sizes 11000/16938, nonzero
keys, zero stop counts and run/wait state. Both processes remain active after
a bounded million-call advance, with the final repeated observation showing
wait states. Source/private base hashes, IX on/protection off and clean frontend
exit with restored PTY are required. Generated transcripts, inventory hashes
and separate inventory/component result files stay ignored.

Eighteen pure guard tests reject existing workspaces, missing/corrupt text,
wrong binary export sizes, missing/duplicate completion, unexpected diagnostics,
missing descriptors and invalid/duplicated/stopped component rows. The tests
do not launch the emulator by default. Service communication, spooling,
fully configured SOS-user catalog creation, login, restart/persistence and
protected operation are still outside this initial checkpoint. Milestones and next acceptance gates live
in [MIPS/TS Plan](MIPS_TS_PLAN.md).

The separate PRIMO operator probe requires display/unknown-command/display,
with exact timestamped success/error completion forms. The unknown command
does not produce a subsequent ready line; it is itself a completed rejection.
Further display and component listing remain healthy. No actual transport,
printer or spool content is validated.

The catalog probe retains its actual source text, updater reply, lookup and
packed export where an area exists. The known maxp 4 input yields end 42,
a one-segment temporary soscat and the exact 768-byte empty catalog (759 FF
bytes plus words 0,50,-1). Pure guards reject altered footer, descriptor, size
and abnormal/error output even if it says errors 0. A proc test experiment
creates a named record but remains input-format evidence, not a usable account.

The SOS probe creates that empty catalog in an administrative base/login scope,
then retains a private copied startup with exactly one resource-clause change.
The preparation FP exits before system startup so its resources are reclaimed.
The 2000/8 claim yields SOS release 5.0/started, then healthy TEM/PRIMO/SOS rows;
stock 2500/200 is rejected at process creation, and 300/8 produces an SOS init
failure at swpsos (1722). Guards require the started marker exactly once,
matching release, all active process rows and no init/error diagnostic. They
also bound system reply waits and reject resource/lookup/syntax failures
immediately, and reject missing/ambiguous/invalid startup substitutions.
Original and private base media remain unchanged on passing and failing paths.
The working catalog/script exist only in the volatile session, not in shipped
media or a promoted profile. This is first initialization, not login acceptance.

The fixed account probe adds `proc test`, size bounds 20000/30000 and a quoted
dummy password to the minimal updater input. It requires end 56, a two-segment
temporary descriptor and the exact 1536-byte packed fixture. Mutation checks
cover the directory reference/footer, process header, password, size bounds,
device data and trailing fill. These offsets describe this observed fixture,
not a general catalog writer. The packed export contains the dummy password
in clear text; no real credentials are appropriate here. Two fresh probes
reach SOS started with healthy TEM/PRIMO/SOS rows, and an empty-catalog control
still passes. Complete user claims, terminal records, password checking and a
real login/job remain unverified; failed locid syntax experiments are not
accepted merely because the updater leaves an output area behind.

The optional SOS operator diagnostic sequence selects the local `>`/BEL
prompt for each command, checks the exact echo and a complete timestamped
SOS response, and retains individual transcripts. Eighteen pure tests include
finite command-count/line-length limits, control/non-ASCII rejection, prompt,
echo, duplicate/wrong-origin/partial reply rejection. Commands are accepted
only with SOS/account startup modes. Actual private probes show display is
unknown, start has no connected terminal, and call with a numeric first
argument plus a name returns ready. A deliberately missing name receives the
same ready response, so no account lookup or login is inferred. All components
remain active with zero stop counts after the diagnostic exchanges.

The catalog outcome report independently distinguishes rejected runs (including
errors 0 and residual areas), incomplete replies, inconsistent completion or
descriptor/export allocation, and normal updater completion. Duplicate end
lines and duplicate descriptors are not accepted as normal. Even normal
completion does not certify login or terminal semantics. A fresh failing term
experiment produces rejected while retaining its actual response and export;
the fixed named-process startup remains the successful control. The report is
developer evidence only and does not change shipped media or runtime behavior.

The catalog-only parser debugger replay saves core, locates a unique aligned
known native signature and rechecks it at the actual execution breakpoint
before saving stopped core and tracing up to 20000 calls. The pure locator
guard rejects missing, duplicate, misaligned and mismatching program sequences.
Replay completion is unverified. Actual traces and updater negative controls
establish a name-first term prefix followed by a quoted locid field: quoted 1
and an empty quoted field advance to term-key, whereas an unquoted integer
fails at locid. These remain abnormal runs, not valid terminal records.
Trace/core/identity artifacts remain private and ignored; no CPU instruction
implementation is changed.

The complete terminal fixture uses term console1, quoted locid 1, quoted
terminal key test, then numeric values 60 and 2, after the fixed account input.
It requires end 58 and exact two-segment output. Only offsets 1314 through
1352 differ from the account-only fixture: twelve name bytes, three locid
bytes, twelve key bytes, packed word 03c002 and three zero words. Guards reject
mutated account fields, each terminal field, record boundaries, trailing fill,
descriptor and abnormal/duplicate completion. These are fixed expectations,
not an independent implementation of the general SOS catalog format.
Two fresh startup probes pass; a repeated call/start check still reports no
connected terminal and no existing process test. No login is inferred from
catalog syntax or startup success. Source/private base media remain unchanged.

The integrated mkdisc test checks standalone and equals-option creation without
a config, 180 exact zero-filled sectors, interactive completion and reuse
refusal, existing files/directories/live and dangling symlinks, invalid or
conflicting options and nonexistent parent paths. It is an optional CTest,
never an ordinary-build dependency. No separate runtime utility is packaged.
The second-disk probe uses that actual option, private device 16 and a persistent
sidecar, then all big run, kitlabel and kiton devno.16. Both utilities need
one normal end, no guest-error markers, one matching mount report and disc1
free-space evidence. The recorded result is 18225 free segments / 2025 slices;
the blank base is unchanged and initialization occupies 45 overlay sectors.
The extended workflow retains a 207-segment, 158720-byte numbered text file on
disc1 and exports identical contents before and after a full emulator process
and PTY restart/remount. The complete lookup reply is unchanged. Guards reject
wrong document, size, scope, duplicate rows, truncation, added bytes and data
mutations both within and beyond the initial 180-sector extent. Export and
media hashes are recorded. A separate --sos-disc1 startup probe retains SOS's
2500-segment/200-entry permanent claim but changes its document to disc1.
An administrative lookup must report exactly one temporary 1722-segment swpsos
on that document; missing/duplicate rows, wrong size/document/scope and guest
error markers fail the guard. TEM/PRIMO/SOS must remain active afterward.
No swap-content, terminal attachment or completed job check is implied.
Pure batch guards require the exact enrollment reply, a unique test child
with the observed size/key/parent and SOS stop count 1, while preserving strict
TEM/PRIMO checks. Missing/duplicate rows, wrong child bounds/parent/state and
unfixed probe inputs fail. Live --verify-batch remains a failing diagnostic:
one empty response to the intervening SOS input releases the list but leaves
test waiting for start by its parent. A saved pre-list core confirms state
0xb8 and exception register 0. The pure snapshot guard rejects truncated core,
wrong descriptor size/kind/parent and duplicate child candidates; recorded
fields are diagnostic and never promoted to successful job execution.
The separate --batch-job fixture imports a finis-only jobfile and requires the
fixed dummy account/password. Its completion guard rejects missing, duplicate
or reversed enrollment/removal messages, abnormal breaks, a remaining test
process and unhealthy services. Wrong probe/command/password combinations fail
before workspace creation. A live copy workload remains a negative diagnostic;
minimal normal completion is not output-file or terminal-login acceptance.
Fixed message and lookup variants additionally require a unique output marker
or a unique one-segment disc descriptor. Missing, duplicate and wrong-name
output fails even when enrollment/removal look normal. Copy-to-console and
allocation-only live variants still fail with FP break 10 1. The default
finis fixture and its existing guards are retained unchanged.
These failures concern the default zero-segment account. The fixed 30-segment
variant must match the entire expected catalog, including the sole changed
byte 899 and known completion marker. Truncation, unexpected claim values,
wrong bytes/completion and unrelated probe inputs fail. Allocation/copy pass
with the explicit temporary quota. The roundtrip guard requires unique source
and copy reports with one segment and the known character count/checksum;
this is aggregate readback, not exact-byte verification. Retention with no
persistent-scope quota is still a negative control despite a normal finis.
The user-storage variant checks every cumulative key1..3 quota word. It requires
the retained name to be absent before the job. After child removal a separate
matching-base FP exports it. Guards require a unique one-segment disc/user
descriptor, key3/base0/0, exact text and complete binary exports with all
logical bytes plus end marker. Wrong metadata, truncation and changed logical
bytes fail; unused segment-tail differences are accepted deliberately. This
proves retention across job exit, not simulator restart or sector equality.
Interactive guards separately identify the zero-stop input-wait child under
SOS without weakening the batch guard. Message requires actual unique output,
not just echoed input. Lookup and logout require unique correctly originated
replies. Run additionally requires the from tem header, absent for go; missing,
duplicated or wrong-path replies fail. Invalid/mixed dialogue modes fail before
workspace creation. Live go and repeated run sessions verify wrong-password
rejection, FP commands, normal removal and healthy services on a private
loopback device-4 endpoint. These are compatibility-only single-terminal checks.
Password event checks also
distinguish the complete BEL-terminated question from a timestamped response,
including partial and differently originated input.

## Core and integer instruction boundaries

tests/c/test_main.c includes direct CPU/core vectors as well as guest integration
checks. The quick mode covers unsigned 24-bit addresses, address/IC/skip/AM wrap,
W-register aliases, final byte/word and double-word transfers, invalid indirect
pointers, missing instruction/operand storage, interrupt entry and saved IC.
Its architectural-capacity fixture allocates 32 MiB of halfword storage;
this does not change the configured runtime limit or validate monitor boot at
that capacity.

SX has all 64 combinations of its three-bit mask and exception-state inputs,
with checks for preserved exceptions/registers. ND includes zero and signed
values and both store-count halfwords. Boundary faults are explicit native
expectations, not host memory faults. These baseline vectors are distinct from
the optional protection vectors described above.
The source vectors and Architecture Notes define the current contract without
requiring an external host-emulator source tree.

## Read-only magnetic tape

tests/c/test_rc749.c checks tagged-record framing, matching trailers, odd-byte
padding, tape marks, gaps/end markers, status and bounded core transfers,
forward/backward positioning and rewind/unload. Unsupported write/erase
operations report intervention rather than successful modification.
The runtime-device check covers operator mounting and the configured confined
guest-mount extension. These synthetic fixtures do not establish an archived
software installation or physical tape timing.

## Optional V2 terminal protocol

`tests/c/test_terminal_v2.c` uses synthetic core message blocks, not a new
historical monitor fixture. Its expectations define the bounded native
attribute/conversion contract documented in Architecture Notes.
Literal expected control-status arrays independently anchor all 128 seven-bit
input bytes for all three types and four read-mode combinations (1,536 cases).
Selected byte-for-byte echoes cover CR/LF, backspace and DEL. Other cases cover
one-shot/blind echo, prompt/NUL output, synchronous attribute controls,
invalid types, complete-block address validation with unchanged buffers,
busy/timeout/attention behavior and independent classic/V2 terminal state.
Both direct device calls and an actual guest GET_ATTR instruction are tested.
The runtime-device CLI test adds a mixed CONSOLE/RC851 configuration and
compares V2 GET_ATTR with the classic control no-op through emulator commands.
Native quick tests also cover console/Telnet conversion. These are
protocol tests, not a V2 monitor boot, UART model or screen-editor validation.

## Floating-point oracle

`test_floating_oracle.py` evaluates all six FP instructions using Python's exact
rational and unbounded signed-integer arithmetic, independently of the emulator's
limb multiplication, division loop and wrapped-register/sign-bit implementation.
The reference is the June 1969 RC4000
Reference Manual, sections 5.1-5.6 (printed pages 22-26). The signed normalized
intervals differ: positive fractions lie in [1/2, 1), negative fractions in
[-1, -1/2). Rounding adds half a fraction unit and takes the floor; low precision
then copies fraction bit 33 into bits 34 and 35. Hand-derived anchors check the
oracle's scaling, sign, rounding and packing before implementation comparisons.

The test-only `floating_probe.c` runs both the FP helper and one actual machine
instruction per request. The 70,144 deterministic FM/FD vectors cover normalized
fraction boundaries, all eight low-bit residues, both signs, zero operands,
exponent-limit crossings, modulo exponent storage, rounding/re-normalization,
division by zero (including zero/zero), both precision modes and all four
register pairs, including W3/W0. A fixed-seed sample adds interior fractions
and exponent combinations. Expectations include untouched registers, the
instruction counter, preserved bit 21, cleared bit 23 and the arithmetic
exception/interrupt request. Divide-by-zero must leave both operand words
unchanged even in low precision. No monitor or disk image is needed.

Another 29,672 CI/CF vectors check zero, both signed integer limits, exact values
on either side of half-integer ties, exponent crossings and extreme signed
24-bit scaling addresses. CI is independently modeled by normalizing the exact
integer; CF uses exact scaled rational values rounded by floor(value + 1/2).
Proven magnitude bounds avoid constructing enormous powers for extreme scales.
CF also covers non-normalized fractions, including unit-sized fraction fields.

The conversion instruction's scale is supplied through an unused index register,
so the CPU checks include signed effective addresses beyond a 12-bit displacement.
Both precision settings must yield the same conversion result. CI overflow
stores the modulo-exponent float and requests the floating exception; CF overflow
leaves its register pair unchanged and requests the integer exception. Successful
CF changes only the selected register, not its preceding fraction word; helper
overflow must preserve the caller's output sentinel. All register pairs and
untouched index/register values are checked.

Another 97,808 FA/FS vectors check cancellation, canonical zero, fraction and
exponent boundaries, both modes/register pairs, and alignment differences around
the 16-, 26-, 28-, 35- and 38-bit boundaries. These include the existing workload
borrow regression and the same fixed-seed interior operands as FM/FD.

The compatibility model quantizes signed fractions onto a lattice with 28
trailing bits, aligns them with mathematical floor division, and adds/subtracts
using unbounded integers. It detects overflow by numerical range, not C's wrapped
sign-bit formulas, and reduces the result to two guard bits before normalizing
and rounding with exact rational arithmetic. Historical dominance cutoffs at
exponent differences of 38 are explicit parts of this compatibility contract.

A second model quantizes each operand to the manual's two guard bits before
addition/subtraction, without the historical dominance shortcuts. The test
reports differences between the models rather than presenting compatibility as
hardware conformance. In this matrix, 47 generated operand/operation records
differ before register/mode expansion; this is not a general error rate or a
count of hardware defects. The borrow anchor expects second word `9c6001` from
the compatibility model and `9c7001` from the literal 38-bit model.

Another 314,304 vectors contain at least one non-normalized, nonzero fraction:
77,280 each for FA, FS and FM, and 82,464 for FD. Signed unit fields, powers of
two and their neighbors, normalization boundaries, noncanonical zero partners
and exponent/alignment boundaries exercise repeated normalization shifts.
All four register pairs, both precision modes and the same helper/CPU state
checks are retained. FA/FS dominance shortcuts deliberately preserve the chosen
operand even when its fraction is not normalized (manual section 14.8).

FM/FD expectations use exact rational arithmetic followed by mathematical floor
quantization onto the 38-bit fraction lattice **before** final normalization. They do
not assume ideal real arithmetic when intermediate bits have been discarded.
For example, multiplying fraction fields `1` and `1`, both with exponent zero,
produces zero after quantization rather than the ideal `(2^34, -69)`. Multiplying
`-1` and `1` leaves a negative guard unit and produces `(-2^35, -37)`, rather than
the ideal `(-2^35, -70)`. Likewise, division of fraction `-1` by `2^35 - 1`
quantizes to five negative guard units, exposing truncation before normalization.
These hand-derived anchors make that compatibility contract explicit.

The FD matrix now includes both large and small fraction quotients and division
by zero. Additional anchors cover `2 / 1`, signed fraction extrema divided by
unit fields, and exponent overflow. Another 5,184 boundary-focused vectors within
the FD count check both signs around ratios `2^25`, `2^26`, `2^27` and `2^34`,
where the former fixed host-width shift either changed sign or lost high bits.
The expanded oracle failed against the old implementation before the correction.

For fraction quotients of magnitude at least two, the rational model first
rescales the exact quotient by powers of two and compensates the exponent.
The C implementation instead scales the denominator before generating quotient
bits. With architectural 36-bit inputs, the denominator remains at most `2^35`,
the generated guard quotient is less than `2^38`, and each doubled remainder is
less than `2^36`; host overflow is avoided without a wider integer type. Smaller
quotients retain the preceding truncation contract, and all normalized-operand
vectors remain unchanged. The former `2 / 1` result had an unrepresentable
positive fraction; fraction `2^34` divided by `1` even returned zero without
an exception. The new anchors require `(2^34, 2)` and `(2^34, 35)` respectively.

The manual describes normalization during non-restoring quotient generation;
the native implementation uses bounded restoring division and guard rounding.
The tests validate the stated native arithmetic contract, not equivalence of
these algorithms for arbitrary non-normalized hardware inputs. The total oracle
now comprises 511,928 helper/CPU vectors.

This is not exhaustive hardware certification. FA/FS and non-normalized
arithmetic expectations deliberately follow the documented compatibility
contract where it differs from ideal arithmetic or the manual.
Python and the probe are test dependencies only; neither is needed at runtime.

## Operator CLI

`test_operator_cli.py` uses a pseudoterminal and an isolated configuration to
verify Readline command/argument/file completion, history recall, line editing,
register writes, core save/load, guest-to-operator handoff, long media paths,
and 80-column emulator output. It checks
terminal restoration after normal exit, prompt EOF, and SIGINT/SIGTERM/SIGHUP
while line editing. No disk image is attached or changed. This is a focused
operator checkpoint, not the complete guest-run/Telnet acceptance test.
An independently encoded guest reader verifies exact local bytes after ANSI
cursor/SS3/string filtering, bracketed-paste framing, Delete mapping and UTF-8
rejection. A guest loop tests split sequence bodies, bare Escape and operator
escape inside an unterminated control string.

`test_operator_acceptance.py` extends this to the complete control/debug path
using independently specified AL/RS/JL words and an odd-parity bootstrap tape.
It checks execution and write breakpoints against actual register/memory
results, reset return-address storage, start/autoload, media actions, and a
byte-identical core round trip. A live monitor session uses two ephemeral
loopback TCP ports: one CONSOLE and one RC851 endpoint. It verifies negotiation,
connection/rejection while a Readline command is partly edited, ANSI RC851
initialization, independent local/remote `geo` sessions, reconnect without
losing the remote guest process, and SIGINT/SIGTERM/SIGHUP cleanup during raw
guest input. The RC851 connection check does not claim a full screen-editor
workload. Monitor/native/original image hashes remain unchanged.

The two-FP extension inspects local and remote via montest on their respective
device-2/device-4 streams before geo starts. Require distinct descriptor
addresses, non-overlapping 100000-unit allocations and a common nonzero parent.
Independent CLI core reads check kind +0, four name words +2..+8, bounds
+22/+24 and parent +50. Remote Control-A selects s to remove remote; a completed
remote system list must retain local and omit remote. The remote slot's first
name word is zero while local's stable descriptor fields remain unchanged.
Recreate remote and compare both guest displays and raw core fields with the
original records, including fixture-specific slot/allocation reuse. Continue
both geo dialogs and reconnect checks. Telnet command lines use NVT CR LF,
not a bare CR that the NVT parser deliberately buffers. This extension covers
default MONITOR/RC433X only, separate-terminal lifecycle rather than shared
console read arbitration, and no guest protection or general allocator claim.

The live acceptance test needs permission to create loopback TCP sockets, not
Internet access. A sandbox that forbids sockets cannot run this checkpoint;
do not interpret that environmental failure as a passed or skipped test.

## Runtime configuration boundaries

`test_runtime_devices.py` checks recognized-but-unimplemented device rejection
before opening monitor or media files, in bounded, interactive, and autostart
modes. It also derives occupied devices/interrupts from the active shipped
configuration, confirms eight used and thirteen free interrupt levels, and
validates six additional terminal pairs. A seventh pair reusing an occupied
level is rejected. This establishes parser capacity, not monitor support or a
live eight-terminal installation.

## Execution throttle

The quick C suite checks setting/configuration validation and deterministic
governor arithmetic with synthetic wall/CPU timestamps, including dropping
host-stall credit. `test_throttle_cli.py` checks rate pacing, three instructions
per AM-chain interpreter call, the `original` preset, measured process CPU
usage under a 20% budget, invalid-setting preservation, completion, unpaced
debug steps, Telnet negotiation during a `1ips` wait, operator escape and
signal cleanup. Timing assertions use broad tolerances rather than requiring
cycle-exact host scheduling. It also boots and initializes the real monitor
with the shipped 50% setting on volatile media and verifies the base-image hash.
The TCP permission requirement is the same as the operator acceptance test.

## SLANG source

`slang_integer.sl` is an ASCII/LF source assembled by the supplied disk-resident
SLANG program in the `--slangbuild` integration checkpoint. Two reserved words
precede the four instructions at FP entry offset 4. The expected instruction
words, derived independently from the RC4000 format, are `2c007b`, `2d0141`,
`1c0002`, and `348000`. The program loads 123 and 321, adds W1 through its
core alias at address 2, and loops on the final relative jump. The test checks
the exported binary prefix and execution with W0=444 and W1=321.

The backing-store export occupies one 768-byte segment. Bytes beyond the
18-byte prefix are not part of the sample and are not expected to be zero.
The source and expected words are maintained independently of the emulator's
instruction encoder.

## Monitor manual examples

`hello.alg` is a newly written minimal Algol program that prints
`Hello from Algol` and a newline. It is not a historical reference fixture.
`test_monitor_manual.py` imports it with the disk-resident `fromhost`,
compiles it with `algol`, and verifies its execution in an ordinary process.
The same checkpoint verifies text import/copy/rename/export, editor
search/replacement/completion with byte-for-byte result export, read-only
export of the installed `startsos` script against its expected text, process
stop/start, normal FP termination, and explicit process removal. It checks
exported host bytes and process listings, not merely command echoes.

Console text display uses `c=copy msrc`, then `c=copy msrc moved`. The checkpoint
isolates each command's output and checks the exact two-line sample once or
twice respectively before the copy summaries. It checks the source summary
and both input/destination summaries for concatenation. The existing subsequent
exports compare source and copy bytes with the original host sample, verifying
that display did not change them. Binary display, paging and other destinations
are not covered.

In the manual checkpoint, guest `catsort` is called before creating temporary files. The bare report must
list only `c` and `v` in the temporary base interval, with zero segments and two
entries. `catsort scope.system` must contain 414 unique alphabetical names,
matching the offline device catalog minus `catalog`, the expected permanent
base heading, the 2,818-segment entry total and the separate 967-slice/
2,901-segment disk summary. Numbered headings and form feeds must match the
observed eight native-monitor pages or seven standard-monitor pages; both
variants must include every name and the final disk summary. These are
fixture-specific catalog totals, not
free space. Both calls must return without guest diagnostics; subsequent
`claim` commands verify keyboard continuation. The same checks run with native
and standard-monitor/controller variants in volatile overlays. Other scopes,
sort options and the `cat` alias are not covered by that checkpoint.

A second `catsort scope.system` runs after `o sortreport`. The console must
not contain its page headers or descriptor rows. After explicit `o c`,
`sortout=tohost sortreport` exports ASCII/LF text without NUL padding, CR or
the guest end marker. Every sorted name and complete descriptor row must
match the earlier console report, and both totals must remain intact. A
console lookup confirms a temporary disk result; explicit deletion and a
subsequent unknown-name diagnostic confirm cleanup. The repeated `show`
baseline brackets this workflow and must prove full disk-space reclamation.
Before report deletion, the checkpoint deliberately calls `catstat sortreport`.
It must produce exactly one `break 0 line 18-19` and `pause muser break fp`.
The helper waits specifically for the pause notice; it does not weaken normal
next-input command boundaries. After explicit attention/system selection,
`proc muser start` must produce exactly `***fp reinitialized` as its diagnostic.
The `sortagain` text export must equal the original `sortout` bytes. Subsequent
lookup, deletion, free-space accounting and the rest of the manual session
must still work. This native/standard-monitor negative case tests FP recovery,
not successful statistics output or recovery from every guest fault.

The separate `sim4000_catstat_diagnostic` checkpoint (`tests/python/test_catstat.py`)
boots a small session, exports the binary main catalog, runs bare `catstat`,
recovers FP and exports the catalog again. It uses helper mode
`--verify-catstat`; optional `--verify-catstat-dfc` uses the standard monitor
and DFC403/RC4819, also exercised by the standard-monitor checkpoint.
Only while the guest program runs, the helper observes `jd` monitor call 42
(`0xfff82a`) whose four-word name is exactly `catalog`, then checks the return
at the caller's next instruction. The source contract in `monsrc/montxt11`
at `p21` specifies w1 = tail address, w3 = name address and w0 = result.
The observed result is 0, the size is 72 segments and tail word 9 contains
`0xffffff` (contents/entry 4095/4095). These are fixture-specific values,
not a proposed replacement descriptor or proof of a compatibility defect.
After that return, the observer captures the first unassigned instruction:
`0xf52fed` (opcode 61, w1, x2-19), followed by `0x004001`. It verifies that the
same interpreter step enters interrupt 0, increments the interrupt-service
counter and stores the instruction's next address in core word 10. The observed
index is 1 and descriptor bounds are 0 and 40; its origin plus 2 equals the
lookup's tail address. These values match an in-bounds integer-array access
under the native IX descriptor-displacement/shift encoding, not an
enabled instruction in this IX-off checkpoint. No registers, core words or guest descriptors
are modified by the observer, and no absolute guest code address is assumed.

The test requires exactly one such completed lookup, one `break 0 line 18-19`,
one process pause and one FP reinitialization. Both binary exports must be
55,296 bytes and identical, with no extra host files. Hashes of the shipped
monitor and both disk fixtures (and optional standard monitor) must remain
unchanged. This identifies the unassigned-opcode exception in the observed
failure path after a successful lookup; it does not recover the Algol source,
determine the correct invocation, prove IX alone would suffice, or validate
`catstat` statistics. Existing report-input recovery checks remain
in the longer monitor-manual checkpoint.

The separate `sim4000_ix_tests` uses independently calculated vectors for all
four registers, signed bounds/type, positive shift counts including modulo-32
and 24-bit truncation, overflow, alignment, displacement, all addressing modes,
AM, register aliases and PC wrap. Invalid cases check Ex22/Ex23, unchanged Ex21,
masked fallback, active masks and deferred interrupt delivery. Ex21 is initially
both clear and set on successful and invalid operations.
Masked invalid indices must not create a deferred request even if the mask
is enabled immediately afterward; existing pending requests must survive.
AM followed by invalid IX checks deferred floating-interrupt delivery and the
saved next IC; AM followed by inaccessible origin checks instruction exception
0 without partial destination/flag updates.
Missing inline,
origin or bounds storage must fault without changing destination/flags. Default
opcode assignment and other unassigned slots remain checked.

`test_catstat_ix.py` uses helper modes `--verify-catstat-ix` and
`--verify-catstat-ix-dfc` for the positive case. It compares all 72 segment
headings and all 418 records by segment, name and multiplicity against the
offline listing, requires normal FP continuation and byte-identical 55,296-byte
catalog exports, and checks fixture hashes. CLI sessions verify configuration,
override precedence, status, decoding, execution and reset retention. CTest runs
the supplied-monitor case; the standard-monitor checkpoint additionally runs
the DFC variant. These checks do not validate other argument forms or hardware.

`sim4000_ix_runtime` runs seven configuration/override combinations through
the actual `--run --steps 2 --trace` frontend, without booting a guest system.
It creates a disposable big-endian synthetic monitor containing AL followed
by PC-relative IX. The trace must show the exact destination, effective address,
next IC and interrupt count. IX-off runs must enter interrupt 0 and save the
next address in core word 10; IX-on runs must consume the inline operand without
an interrupt or core writes. Equivalent selections must produce byte-identical
traces, and the synthetic input must remain unchanged. This covers bounded-run
option propagation separately from interactive CLI and full guest checkpoints.
An omitted CPU setting must enable IX, just like explicit YES; explicit NO and
`--no-ix` keep the negative exception checkpoint available. The session helper
deliberately selects IX-off outside its dedicated IX-on modes, preserving the
historical negative guest diagnostics independently of the frontend default.

The positive catalog checkpoint also starts the real CLI with `-a`, no CPU
configuration and no IX override. A pseudoterminal supplies the date, selects
the system process through attention and starts a user FP. Bare `catstat` must
print all 72 segment headings with 418 entries and finish with `end 18`, without
a break or FP reinitialization. A subsequent `lookup catstat` must finish too.
Ctrl-E then returns to the operator prompt, where IX-on and volatile-overlay
media are checked. Fixture hashes and restored terminal attributes are verified
after this path as well; guest line endings are normalized only for assertions.

`tests/python/test_regnetest_cli.py` is a separate, opt-in integration checkpoint:

```sh
python3 tests/python/test_regnetest_cli.py build-release/sim4000
python3 tests/python/test_regnetest_cli.py build/sim4000 --protected \
  --output build-protection/regnetest-protected
```

It boots the normal `-a` frontend without a CPU setting, starts a user FP and
enters `i regnetest`. One iteration must produce both known numerical sums
with `test ok`, one successful run and zero failed runs. Ctrl-E then suspends
execution before requiring another iteration; IX-on state, volatile disk media,
terminal restoration and fixture hashes are checked. The guest computation has
a 120-second output deadline. This test is not part of routine CTest: run it
after relevant CPU/FP compatibility milestones, not after every small edit.
It supplements the instrumented C checkpoint; it is not a hardware oracle or
a claim about every numerical workload.

The same checkpoint now also passes with the supplied source-built protected
monitor and derived system-binding disk. It observes fresh guest compilation
(`algol end 61`, program version 83.04.12) before requiring the same two exact
numeric outputs, one successful run and zero failed runs. No protection check,
library binary, monitor or CPU behavior is patched for this workload. Final
operator inspection requires `protection=rc4000`, IX on and volatile-overlay
media. A fresh guest claim identifies the 100000-byte test area; its first,
middle and final word must all retain key 1 after the workload. These are key
samples, not a new full-area isolation assertion. The full inherited tests of
process-area keys remain separate.

The CLI check reads guest lines under one 120-second iteration deadline and
rejects an FP break/reinitialization rather than treating it as success. On
completion Ctrl-E suspends the recursive command file; it does not claim normal
guest termination. Terminal settings must be restored on clean exit. Both
standard and protected monitor/disks and the original historical disk retain
their hashes, also on failures. Optional `--output` refuses existing work and
retains configuration/transcript; `result.txt` is written only after all checks
pass. Pure guard tests reject wrong/missing sums, success words, run counts,
break/reinitialization markers and an existing output directory. Registered
CTest workload count remains unchanged. This is one tested iteration, not a
long-duration soak, a hardware oracle or complete numerical-library acceptance.

### Diagnostic-file identity

Read-only inspection of the supplied UNIT0 links `regnetest` to `cpuregtest`:
the former's text starts with a mode-0 conditional compilation of the latter
to `aaa2`, runs `aaa2` with a fixed title and thirteen numeric values, then
includes itself again. `cpuregtest` is Algol source for MPROFILE VER 8EU,
credited in its header to Kaare Lund Rasmussen (7-FEB-80), with a printed
version of 83.04.12. Its simulation self-checks use literal reference values
360.7006434 and 1855.9898087 and strict absolute tolerance 0.0000001.

The offline device and main catalogs both locate cpuregtest at first slice
425, size 23 segments; its traversed allocation uses slices 425 through 432,
with each required continuation link equal to +1 (three segments per slice).
Text uses three 8-bit characters per 24-bit word, high byte first, while the
disk stores each word little-endian. Stop source text at character 25; bytes
after that marker are not additional source. The regnetest command prefix is
at first slice 667. These are fixture-specific inspection coordinates, not
an assumption that arbitrary guest files are contiguous or an extraction API.
No diagnostic execution or disk write was needed to establish this relationship.

Read-only binary inspection also followed the actual allocation chains for
claimtest (35 segments, first slice 101, 12 slices) and montest (146 segments,
first slice 769, 49 slices). Both catalog tails have contents 2 and entry 2952;
neither was interpreted as an FP text command file. Montest contains command
labels and embedded help covering process/area descriptors, message buffers,
chain tables, core/dumps, formats, lines, output and locking. The binary's
string storage interleaves printable fragments with runtime/code data; a
printable-string scan is not a faithful reconstruction of its complete help.
The user guide therefore records only inspection evidence and possible risks,
not verified command syntax or monitor-layout compatibility. Claimtest's
parameter/error fragments do not establish a usable parameter contract or
justify running it on the active monitor.

The subsequent `sim4000_montest_help` CTest uses the normal autostart frontend
and a volatile image overlay, creates a user FP of 200000 RC bytes and executes
`montest info` and help for internal, external, buf, chain, typein and end. Each must
print its expected syntax/description and return to FP without a break or error.
A following lookup checks FP continuation, and Ctrl-E/quit checks terminal
restoration and fixture hashes.
The subsequent extension runs `montest internal name.mtest` and checks the
reported descriptor address, name, kind and memory bounds against direct CLI
core reads after Ctrl-E suspension. Independent offsets come from montxt1
(kind +0, four name words +2 through +8, first +22, top +24); top minus first
must be 200000 RC bytes. A second lookup checks FP continuation after display.
Claims, state, queue links, timers and register snapshots are not compared
across observations because they may change during return to FP.
The all-slot extension runs `montest internal all`, parses each displayed
descriptor and checks unique addresses, one own process and one supervisor s,
the own process's previously checked identity/allocation and its parent link
to s. It requires an unnamed zero-allocation slot too: this report enumerates
descriptor slots, not just active jobs. For every displayed slot, independent
CLI reads check kind, four name words, first/top and parent at +50 (montxt1 a34).
No fixed descriptor count/address is assumed, and enumeration completeness for
arbitrary monitors is not claimed. A subsequent FP lookup must still work.
Remaining printed fields, including resource claims, remain outside coverage.
The additional `montest external name.disc` call checks disk name/kind and
device number against raw descriptor words (name +2 through +8, kind +0,
device number times 64 at +10, per montxt1). The normal monitor reports kind
62 and device 15, but the guest tool labels +10 as `main : 960 (!)`; the test
and user guide preserve this mismatch rather than validating it as a pointer.
Subsequent FP lookup must work. Reserver/user/message fields, other process
descriptors, other conversational commands,
core/dump, locking and output-file commands remain outside the validated scope.

The chain extension executes `montest chain docname.disc` and compares six
displayed fields with `--list-catalog` on the immutable base image: auxiliary
catalog name, size, first slice, document name, slice length and last slice.
They must match catdisc/36/2/disc/3/2029 for the shipped fixture. A following
lookup verifies FP continuation. This compares in-memory header reporting with
independent offline parsing, not the complete chain-link table, free-space
accounting.

The dialog extension enters the exact call `montest typein`, requests only
`info chain`, then exits with `end`. The subsequent `lookup montest` must be
handled by FP. Synchronization uses echoed input and the guest input prompt;
it does not assume a descriptor-record `>` prompt for command input.
The record extension issues internal name.mtest, synchronizes on its `>` record
prompt, repeats the record and compares its descriptor address/name/allocation
with the first display and the later FP-mode core-checked display. It then
uses finis and requires info chain to work inside montest before end returns
to FP. Changing record fields are not compared. Other navigation controls,
out and alternative multi-parameter typein forms remain untested.

Pass an optional generated STDMON path as the script's second argument to
run this same focused suite with DFC403/RC4819 instead of RC433X:

```sh
python3 tests/python/test_montest_help.py build-release/sim4000 build-release/STDMON
```

The suite checks the alternate monitor's hash too. Both profiles pass the
limited field/header and dialog checks, and both show the same misleading
main : 960 (!) label. The standard-monitor checkpoint invokes this suite after
generating its disposable monitor, while routine CTest retains the default
profile. A focused run can reuse an existing generated monitor without running
the full export/manual/release suites. No V2 or protected-monitor support is
implied.

The additional external devno.15 selector must return the same disk descriptor
address, name, kind and device number as name.disc, including the existing
main-label mismatch. Both selections are related to the same independent core
reads. FP continuation and immutable-media checks remain required. This is a
positive selector check, not coverage of missing devices or other device kinds.

The system-return extension requests explicit simulator attention from the
suspended completed FP session, resumes, selects s and submits list. Require
the interim to mtest input prompt, provide one empty line only to this idle
ordinary FP, then require from s, its complete mtest row and ready. The core
address and size must match the independent descriptor checks. Explicit
attention then selects mtest again and lookup montest must work, without a
guest break or FP error. Both profiles pass. This covers an outstanding FP
read ahead of the system reply, not arbitrary program input or a complete
multi-process lifecycle; no console/CPU/monitor behavior was changed.

The sequential lifecycle extension finishes mtest with finis, then completes
a system list and requires absence before checking the released descriptor.
The pause notice alone is not a removal-completion boundary: an initial
alternate-profile check suspended too early and still saw the old first name
word. Requiring the completed list fixes that observation race, not guest code.
Create mpeer with size 100000/run, compare its montest kind/name/bounds/parent
with independent core reads and require reuse of the former mtest slot in this
fresh fixture session. Remove mpeer explicitly through s, complete another
system list and require absence. After both removal paths, first name word +2
must be zero; old first/top remain present in this fixture. This matches
montxt11 g6's name(0) marker and is not complete descriptor erasure. Retain
terminal restoration and all fixture hashes. Both focused profiles pass;
simultaneous FP read arbitration, general slot reuse and process trees remain
outside this checkpoint.

The host export remains available after the guest report is deleted. No
base image is changed, and no guest report is promoted to permanent scope.

Bare guest `show` is checked before an additional temporary text import,
after import and after deletion. A repeated baseline report must not change
the counts; each reported slice must correspond to three segments. The decrease
must match the 36-segment `lookup spacetmp` descriptor (12 slices), and deletion
must restore the baseline and make that name unknown. The test does not hard-code
the initial free count or claim to validate all allocation structures, disk
fragmentation or additional `show` options. Both monitor/controller variants
exercise this bounded workflow without writing the base image.

Bare guest `claim` is checked at the same allocation boundaries. Four reports
must contain stable `area`/`buf`/core headers and ordered `temp`, `login`, `perm`
rows with three segments per slice. The `temp` decrease must match the imported
file's 36-segment descriptor, 12 slices and one entry; other rows stay unchanged,
and deletion restores the initial values. Core size and starting address are
cross-checked against the stopped process listing. Initial disk counters are
not hard-coded and are not equated with the literal `new ... temp disc 200 10`
request. Full inheritance, enforcement, other scopes and resource-changing
arguments are not established by this differential test.

The `cmp` checkpoint compares the imported sample with its renamed copy and
with the single-line editor replacement. It checks the identical-text message,
one `TEXT REPLACED` report with both line contents, and the same report saved
through FP `o` redirection and exported through `tohost`. Subsequent byte-for-byte
exports verify that comparison did not alter any of the three inputs. The
imported and copied entries have different allocated sizes: this is text
comparison, not binary area equality. Additional comparison options and complex
alignment are not covered.

`editor_lines.txt` is a newly written four-line ASCII/LF fixture. The same
checkpoint verifies two relative `l1` movements, single-current-line deletion
with `d0`, multiline `i!` insertion with literal commas/slashes, and `f`
completion preserving the last line. A separate `l2,d1,f` edit confirms the
inclusive deletion of the third and fourth lines. Both result exports are
compared byte for byte; re-exporting the original proves editing did not
overwrite it. Temporary editor entries are explicitly removed. These checks
do not establish arbitrary backward movement or search-based deletion ranges.

`slang_commands.fp` is a newly written ASCII/LF FP command file. The manual
checkpoint imports it and `slang_integer.sl`, executes it through `i`, redirects
the assembler's `list.yes` source/report output with `o slist`, restores the
console with `o c`, and exports binary and listing files. It checks all four
instruction words plus the two reserved words, the 768-byte segment length,
`slang ok 1/12/1`, absence of assembler errors, an executable descriptor at
entry offset 4/length 12, and a successful keyboard command after command-file
EOF. Source output must be absent from the console and present in the exported
listing. Allocation padding beyond the 18-byte code is not compared to zero.
The test cleans up the temporary source, command, listing and program entries.
It does not validate arbitrary FP nesting, conditionals or batch administration.

`nested_outer.fp` and `nested_inner.fp` are newly written ASCII/LF command files
for one bounded nested call. The outer redirects output to `nestlog`, looks up
`fromhost`, selects the inner input, then looks up `algol`, restores output with
`o c` and exports the report. The inner looks up `tohost` and reaches EOF.
The exported report must contain exactly those three descriptor names in that
order and no errors. None of those descriptors may appear in that call's console
output; a subsequent keyboard `lookup slang` must appear there. Cleanup is
confirmed with unknown-name diagnostics for both command files and the report.
Both monitor/controller variants run this checkpoint. No recursion, deeper
nesting, early termination or command-file error recovery is claimed.

`command_error.fp` and `command_error_outer.fp` are newly written ASCII/LF
fixtures for a missing-program error. The inner file looks up `fromhost`, calls
the nonexistent `nosuchprog`, then looks up `tohost`. The outer looks up `slang`,
selects the inner file and looks up `algol` after its EOF. The checkpoint runs
the inner directly and through the outer. Each isolated invocation must produce
exactly one error (`***fp name nosuchprog`) with the expected descriptor order,
and each must return to a successful keyboard lookup. Cleanup is confirmed by
unknown-name diagnostics for both command entries. These checks establish
continuation for this error, not stop-on-error batch semantics or general
recovery for other failures.

`assembly_error.fp` is a newly written ASCII/LF command file. It assembles
`slang_error.sl` imported as `batchsrc`, looks up the partial `batchbad` result,
clears that result and looks up `slang`. The test requires exactly
`***slang sorry 1 1/4/1`, the undeclared-identifier diagnostic, and ordered
descriptors for the one-segment `2.4`/length-4 partial result and the assembler.
After EOF, keyboard lookup must find the result absent, another utility lookup
must work, and source/command cleanup must make both names unknown. The script
does not run the partial result. This is a specific error-continuation example,
not a general compiler-recovery or conditional-batch guarantee.

`conditional_commands.fp` is a newly written ASCII/LF FP fixture. It checks
`if ok.yes` and `if ok.no` after successful and failed lookups, then one skipped
and one executed parenthesized group. The isolated transcript must contain an
exact ordered sequence of ten descriptors and two intentional missing-entry
diagnostics. Guarded `nosuchprog` commands must not execute. A final unguarded
lookup proves group boundaries; keyboard lookup and confirmed cleanup prove
normal EOF return. `absentone` must not exist in the supplied test image.
This tests lookup status, not every program's error/status convention.

`assembly_status.fp` is a newly written ASCII/LF FP fixture. It assembles the
known invalid and valid SLANG sources, uses complementary `if ok` guards, and
exports only the successful result. Tests require the exact undeclared-identifier
diagnostic, the failure-path descriptor and partial `2.4`/length-4 metadata,
the successful assembly report and valid `2.4`/length-12 metadata, absence of
the failed host export and the same six independently specified code words in
the 768-byte successful binary. The guarded missing-program call must not run.
Both results are explicitly removed inside the file; EOF keyboard commands
confirm absence, resumed utility operation and source/command cleanup. Neither
assembled program is executed, and other compiler/error status conventions
remain unverified.

`slang_error.sl` deliberately uses undeclared identifier `a1`. The same checkpoint
checks a missing FP program (`***fp name`), missing-entry lookup (`***lookup`),
and a subsequent successful utility call. It then assembles the invalid source,
requires `a1 undeclared` and `***slang sorry 1 1/4/1`, and verifies the delivered
assembler leaves a temporary, executable-marked `2.4` entry of length 4. The
test does not execute that failed result. It removes only that entry, verifies
lookup now fails, assembles a separate valid source in the same process, and
checks the recovered binary's instruction prefix and segment length before
cleanup. Both monitor/controller variants execute this sequence without reset.

The session driver's expected-diagnostic path is explicit per command. It still
requires input consumption, an idle/input-read boundary and a matching diagnostic
within that command's output, not in earlier transcript text. Ordinary commands
continue to reject unexpected `***` or syntax-error reports. This is test/helper
behavior, not a change to the guest assembler or FP error policy.

`sim4000_build_host_image --verify-manual` drives the supplied monitor;
`--verify-manual-dfc` drives the standard monitor on DFC403/RC4819. Both use
volatile overlays. The Python checkpoint checks the native base-image hash
before and after each session. The standard-monitor test supplies a freshly
exported monitor rather than requiring a generated file in the source tree.

The user manual's software inventory is based on the occupied native image
catalog, inspected startup/configuration text, and historical distribution
notes. Relevant historical evidence is in `bin/READ.ME` (System Utility
version, MIPS/TS files, absent SOS user catalog, Algol interface variants,
and the Screen Editor limitation), `src/MON/HJMOPS` (`edit` command scripts),
and `doc/Slang_Assembler.txt` (assembler operation) in the surrounding
historical collection. These files are evidence, not native build dependencies;
the monitor build inputs now also have local reference copies in `monsrc/`.
Allocated script contents were inspected from their catalog first slices;
the geography data also contains country tables. Text found in unallocated
sectors is not counted as installed software. Inventory-only descriptions
do not imply successful execution or establish argument syntax.

## Complete monitor rebuild

`test_monitor_rebuild.py` uses the archived `monsrc/` inputs and a verified
copy of the native disk. It checks LF line endings and lowercase source names,
then executes `dev/python/rebuild_monitor.py` with its local
default sources, checks `slang ok 11/24980/49`, and compares the full exported
candidate byte for byte with `src/monitor/MONITOR`, including padding. It also
boots that candidate and checks text and binary host transfers. Hashes of
both shipped disk fixtures, the monitor, all archived source files, and the
working disk verify that the two COW sessions do not alter reference inputs.
The test does not depend on the surrounding historical tree.

`test_rebuild_monitor.py` covers working-copy line-ending normalization,
the limited high-byte comment mapping, rejection of unsupported text,
source preservation, refusal of an existing work directory, and distinction
between harmless `***` message comments and guest assembly/editor failures.
Failed manual attempts retain console and listing evidence for diagnosis.
The generation helper bounds commands to 500 million interpreter calls;
the wrapper also enforces wall-clock timeouts.

## Optional archive tape inspection

`test_inspect_monitor_tape.py` uses independently constructed minimal SIMH
record wrappers and RC dump records. It checks odd-length padding, record
truncation, marker mismatches, unsupported markers, unterminated files,
complete segment extraction, missing segments/end records, invalid segment
order, and numbered listing-column removal. It needs no network or archived
tape in the source tree.

The real optional `30003039` investigation verifies the published tape hash
before extraction and compares raw file lengths/hash prefixes with the
archive's independently decoded namespace. A separate download of extracted
`mon8part1` confirms its full hash. Downloaded images, extracted listings,
and comparison reports stay in ignored build directories and are not normal
regression fixtures. Findings are recorded in the architecture documentation.

## Retained catalog files

`test_catalog_workflow.py` uses newly written ASCII text and exclusive,
verified native-image working copies. Its subprocess sessions construct fresh
machines through `sim4000_build_host_image`: `--verify-catalog` uses volatile
storage; `--catalog-create` writes the working image; `--verify-catalog-read`
reboots with copy-on-write storage; and `--catalog-cleanup` writes the working
image to demote and reclaim the saved entry. Writable modes are test/image
maintenance helpers and must only target disposable copies.

The test compares exports byte for byte, verifies permanence key 3 and the
default user interval in the offline catalog, and checks that a temporary
control entry disappears at process removal. It also checks exported text
lookup reports, retained-image immutability during the copy-on-write read,
and unchanged native/original base-image hashes. Its missing-file negative
control expects an explicit guest `lookup ... unknown` report, not merely
any failure or timeout. The helper lets error output finish before rejecting
a command and bounds each manual command to 30 million interpreter calls.

The retained-file session checks local `catsort` groups in both volatile and
disposable write-through runs. Before promotion, `scope.temp` must list
`c`, `gone`, `saved`, `v` (72 segments/four entries); afterwards it must list
only `c`, `gone`, `v` (36 segments/three entries). Bare `catsort` must list
`saved` under `project`, followed by the temporary group; `scope.user` must
include the same names but label the retained group `perm`. Both groups use
the supplied default base pair `8388605 8388605`. Checks cover exact row
names, order within each group, the 36-segment retained row, complete totals
and the separate 24-slice/72-segment/two-disk-entry summary. Existing export,
process-removal, reboot and cleanup checks remain intact. This is native-
monitor/RC433X evidence, not a general selector rule for historical versions
or proof for alternate controllers and catalog intervals.

The retained-file session also captures `search temp`, `search user`, and
`search system` reports and the special `lookup catalog` descriptor. It
concatenates these through disk-resident `copy` and verifies the host text
export contains all pristine catalog names exactly once, plus the temporary
and user controls. Name parsing accepts zero spaces before `=` so eleven-
character names are not silently lost. Scope labels distinguish `gone`
(`temp`), `saved` (`user`), `algol` (`system`), and `lookup` (`system.disc`).
The special catalog entry's upper base exceeds the ordinary system interval;
it must be included separately. The test does not equate a sequence of scope
snapshots with an atomic listing of all future or differently scoped entries.

The checkpoint now distinguishes device and stored monitor main catalogs:
36 segments/415 entries versus 72 segments/418 entries in the pristine image.
Main names are device names plus `c`, `v`, `catdisc`. A binary guest export of
`catalog` is scanned for empty markers and compared to `cata`'s free-slot report.
Retained-file main metadata is checked alongside device metadata, but reclaimed
temporary `gone` still appears in the stored main snapshot. This is tested as
a limitation, not treated as a live accessible file.

Ten exclusively temporary image mutations exercise rejected main reads:
missing/wrong-document descriptors, zero/negative/excessive sizes, area length
beyond its chain, invalid first slice, zero/out-of-range links and a cycle.
Each failed CLI read must emit an error without partial output or changing the
input hash. Source image hashes remain unchanged.

## Architectural trace vector

`trace_integer_device_v1.trace` is a hand-calculated version-1 trace. It is
not captured from Sim4000. The unit test constructs the input program from
instruction fields, executes it, and compares the resulting trace byte for
byte with this fixed vector.

The initial state is IC `000020`, W0 through W3 zero, IR zero, IM `800000`,
monitor mode enabled, reset mode disabled, and execution enabled. Core word
`000080` contains `000003`; word `000082` contains zero. Device 5 accepts a
sense operation and returns `654321`.

The seven instruction words and expected effects are:

| Address | Word | Instruction fields | Expected effect |
| --- | --- | --- | --- |
| `000020` | `2d0080` | AL W1, direct 128 | W1 becomes `000080`. |
| `000022` | `501000` | RL W0, direct X1+0 | W0 becomes `000003`. |
| `000024` | `1c0080` | WA W0, direct 128 | W0 becomes `000006`. |
| `000026` | `5c0082` | RS W0, direct 130 | Core word `000082` becomes `000006`. |
| `000028` | `240004` | AM, direct 4 | Adds 4 to the next displacement. |
| `00002a` | `2e000a` | AL W2, direct 10 | W2 becomes 4+10 = `00000e`. |
| `00002c` | `070140` | IO W3, device 5 sense | W3 becomes `654321`; I/O count becomes 1. |

The AM and AL instructions form one interpreter call with an instruction
delta of two. The RS produces the sole `W` record. The I/O instruction checks
the transferred device value and the last-device, command, modifier, and I/O
delta fields.

## Manual coverage and packaging

[Manual Evidence](MANUAL_REVIEW.md) maps operator-facing recipes to these
checks; it does not duplicate their assertions. Distinct evidence layers are
independent CPU/device expectations, specific guest workflows, source/artifact
identity and documentation structure. None substitutes for the others.

The CMake-based sim4000_binary_dist checkpoint builds an archive from the
runtime executable and existing ready-built media/documents. It verifies one
sim4000/ root, source/input hashes, allowlisted examples, non-runtime exclusions,
executable permission through both profiles' date-prompt startup from an
unrelated working directory, explicit protection/IX configuration, unchanged
packaged monitor/disk hashes, staging cleanup
and refusal of missing inputs or sanitizer packaging without replacing a
valid prior archive. UNIT0.original, STDMON and RC4000.dfc403.cfg must be absent;
the root UNIT0 must match the modified native snapshot, while UNIT0.protected
and MONITOR.protected match the M3 identities. The MIPS/TS disks are also
hash-checked and must boot unattended through sos started. All three profile configs,
the default compatibility alias and PROFILES.txt must be present. Missing or
changed protected artifacts are rejected before staging without changing a
prior archive; no compatibility-only fallback is accepted. The test is
developer-only and needs no Python. Archive contents
and invocation are defined in [Binary Distribution](BINARY_DISTRIBUTION.md).

Final integration repeats the native protection target, manual-source and
rebuild/profile guards, plus the Release archive check after the explicitly
requested PDF refresh. The 82-page PDF embeds the requested fonts; changed
runtime-copy, protection/process, transfer, configuration and Telnet pages,
contents and index are visually inspected. Final TeX output has no unresolved
references, missing glyphs or box warnings. The archive's PDF hash matches the
versioned artifact. This focused pass adds to M3's recorded broad acceptance;
it is not a new full sanitizer run or general protected-software certification.
