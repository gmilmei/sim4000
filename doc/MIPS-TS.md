# Manually prepare the MIPS/TS disk pair

This guide creates `UNIT0.mipsts` and `UNIT1.mipsts` from the maintained
compatibility disk using the emulator and guest tools. It is a **manufacturing
procedure**, not part of the [User Manual](usermanual.pdf). For operating the
already prepared environment, use that manual's MIPS/TS chapter instead.

The result starts TEM, PRIMO and SOS automatically, with one demonstration
job description/password `test`/`test` and a loopback Telnet terminal. It uses
the compatibility `MONITOR`, IX enabled and **protection off**. Do not substitute
`MONITOR.protected`, `UNIT0.protected`, STDMON or an RC8000/V2 monitor.

The procedure follows the existing guest-based generation, but all commands
needed for manual preparation are given below. No Python or guest compiler
is required for the manual route. The acceptance status is recorded at the end.

## 1. Prerequisites and safety boundaries

You need a built `sim4000`, the maintained source disk `src/disks/UNIT0`, and
`src/monitor/MONITOR`. The disk must already contain `fromhost`, `tohost`,
`upsoscat`, `kitlabel`, `kiton`, `starttem`, `startprimo`, `startsos`, `btem`,
`bprimo` and `bsos`. This is **not** a recipe starting from `UNIT0.original`:
that historical disk has different host-transfer command names.

For the tested inputs, the SHA-256 identities are:

| Input | SHA-256 |
| --- | --- |
| `src/disks/UNIT0` | `e5d293bd01c39385218c11531412156ae4b65afa542c1263b2dd3c919f630a1e` |
| `src/monitor/MONITOR` | `b5a40b73a38f65f02f47835ef1557c9989055df68c692a0719bf536ba14cfbb2` |

The development/test platform is Linux on x86-64. Building needs C17, CMake
and GNU Readline development files; running needs the matching runtime
libraries. An ASCII/LF text editor is needed for the input files and configs,
and a Telnet client for the final check. `sha256sum` below is a host utility,
not a simulator command.

Never select a historical or shipped image for write-through. All guest
changes in this procedure target **new private copies**. Stop on an unexpected
error instead of trying initialization commands on another device. In
particular, `kitlabel` initializes guest disk metadata and can destroy existing
files even though the host `mkdisc` refuses to overwrite an existing file.

### Command contexts

Every command block identifies its interpreter. Do not type context headings,
example output, or the contents of a host text file as guest commands.

- **Host shell:** builds, copies, editor invocation, `sim4000` launch, Telnet.
- **Emulator CLI:** the `sim4000>` prompt after local Ctrl-E; e.g. `quit`.
- **Guest-s:** press Escape locally, wait for `att` and the bell, enter `s`,
  then wait for its input bell. System commands normally finish with `ready`.
- **FP:** the guest command interpreter in `startproc`, `mipsprep` or another
  explicitly named process. Wait for the input bell after each command.
- **Remote guest attention/SOS:** Ctrl-A in the configured Telnet endpoint,
  then select `sos`. This is neither the host shell nor the emulator CLI.

Ctrl-E suspends the whole machine, including remote jobs; `continue` at the
emulator CLI resumes it. Attention does not log out or remove a process.

## 2. Create an exclusive workspace

**Host shell, repository root (`linux/`):**

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build -j4
MIPS_REPO=$(pwd)
MIPS_WORK=$(mktemp -d "$MIPS_REPO/build-mipsts-manual.XXXXXX")
mkdir "$MIPS_WORK/prepare" "$MIPS_WORK/prepared" "$MIPS_WORK/run"
mkdir "$MIPS_WORK/prepare/exchange"
cp -i "$MIPS_REPO/src/disks/UNIT0" "$MIPS_WORK/prepare/UNIT0.mipsts"
cp -i "$MIPS_REPO/src/monitor/MONITOR" "$MIPS_WORK/prepare/MONITOR"
sha256sum "$MIPS_REPO/src/disks/UNIT0" "$MIPS_REPO/src/monitor/MONITOR"
"$MIPS_REPO/build/sim4000" --mkdisc "$MIPS_WORK/prepare/UNIT1.mipsts"
```

Record `MIPS_WORK` so another host terminal can use the same directory; shell
variables are not automatically shared between terminals. Keep the printed
input hashes. Do not proceed if an input differs unexpectedly or a command
fails. Each destination is new: an overwrite prompt is a reason to stop.

`--mkdisc` creates an unlabelled 180-sector starter image (138240 host bytes),
not a mounted guest disk or a pre-existing filesystem. It must succeed once;
do not recreate the file later to mount it. The guest geometry initialized
below can address more sectors than this starter file initially contains.

## 3. Boot the private preparation copies

Create the following ASCII/LF file with a host editor. Its relative paths
are resolved against the configuration directory, not the current shell.
Write-through is intentional **only here**, to these private preparation
files; it makes the stopped files complete bases without overlay flattening.
Do not add `OVERLAY` options or use a disk-path command-line override.

**Host file contents: `$MIPS_WORK/prepare/RC4000.prepare.cfg`:**

<!-- mips-manual: preparation-config -->
```text
PROTECTION MODE=OFF
CPU IX=YES
MONITOR FILE=MONITOR
CORESIZE=0,1048576
OPERATOR ESCAPE=CTRL-E
OPERATOR DATE=ALT-D
INPUT DEL=CTRL-H
DATE YEAROFFSET=-40
THROTTLE VALUE=50%
DEFINE CONSOLE DEVNO=2 INTNO=6,3
DEFINE CLOCK DEVNO=3 INTNO=11
DEFINE RC610 DEVNO=7 INTNO=10 FILE=printer.preparation.txt APPEND=YES
DEFINE RC433X DEVNO=15 FILE=UNIT0.mipsts WRITABLE=YES
DEFINE RC433X DEVNO=16 FILE=UNIT1.mipsts WRITABLE=YES
DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT=exchange
```

There is no preparation Telnet listener. `exchange/` must already exist.

**Host shell, same variables as above:**

```sh
"$MIPS_REPO/build/sim4000" --check-config "$MIPS_WORK/prepare/RC4000.prepare.cfg"
"$MIPS_REPO/build/sim4000" -a -c "$MIPS_WORK/prepare/RC4000.prepare.cfg"
```

The first command must report a valid configuration, protection off and IX
on. The second boots the copied default disk: expect `disc mounted on 015`
and `initialize date:`. The first startup still asks for a date because its
`startarea` has not yet been changed.

**FP/startproc, at `initialize date:` (one line, then Enter):**

<!-- mips-manual: initial-date -->
```text
86 09 30 08 00 00
```

Wait for `pause startproc finis fp`. Press Escape, wait for `att` and the
bell, select `s`, and enter the following **Guest-s** command:

<!-- mips-manual: preparation-process -->
```text
all mipsprep size 150000 run
```

This creates an administrative FP with sufficient preparation resources.
`all` is deliberate for this private manufacturing session, not a suggested
claim for the demo user. Wait for `to mipsprep` and the FP input bell.

## 4. Initialize and mount the second disk

**Warning:** the first command below destroys/replaces guest metadata on
device **16**. Confirm that device 16 is the newly created private
`UNIT1.mipsts`; device 15 is the system disk and must never be labelled here.
Do this only once, before there is any wanted data on the new disk.

**FP/mipsprep:**

<!-- mips-manual: initialize-disc -->
```text
kitlabel 16 disc1 catdisc1 slow 36 9 2030
kiton devno.16
show
```

Both utilities must finish normally (`end` followed by a number), with no
`***`, hard error, giveup or abnormal end. Mounting must report
`disc1 mounted on 016`. `show` must include `disc1`; initially the checked
geometry provides 2025 free slices, or 18225 segments. Utility end numbers
and spacing are not commands to type.

`kiton` needs `devno.16`, not just `16` or `disc1`. On a later manual remount,
use only `kiton devno.16` in an administrative FP, never `kitlabel` again.
The final autostart instead uses the initialization-time Guest-s `kit 16`
before creating the resident subsystem processes.

## 5. Export and inspect the original startup texts

**FP/mipsprep:**

<!-- mips-manual: export-originals -->
```text
oldboot=tohost startarea
oldsos=tohost startsos
oldtem=tohost starttem
oldprimo=tohost startprimo
```

These create new host files under `prepare/exchange/`. Export refuses existing
host filenames: use a fresh workspace rather than overwriting evidence. Inspect
them with a host editor in another terminal while FP waits for input.

The expected original `startarea` creates `startproc`, un-stacks, then feeds
`startdate c` and `finis` to that process. `startsos` requests size 300000,
35 buffers, 45 areas, program `bsos`, one internal process and key 3, with
permanent resources `disc 2500 200`. `starttem` requests size 11000,
`disc 360 3` and `btem`; `startprimo` requests size 16938, `disc 18 3` and
`bprimo`. All three select functions 1 through 5 and the broad system base
interval. If these do not match, stop and identify the different software
before applying this fixed recipe. Do not run trim/generation scripts.

## 6. Create soscat with the guest updater

Use a host editor to create the complete file below, including the final LF.
It is **input data for `upsoscat`**, not a series of FP or Guest-s commands.

**Host file contents: `$MIPS_WORK/prepare/exchange/catinput`:**

<!-- mips-manual: catalog-input -->
```text
maxp 4
proc test
mins 20000
maxs 30000
pass "test"
bs disc key0 6 30 key1 2 6 key2 2 6 key3 2 6
term console1 "1" "test" 60 2
```

`maxp 4` requests catalog capacity; the historical updater rounds capacity
up to a multiple of 50, not to four actual account entries. It does not
provision four independent accounts. `proc test` defines the demonstration job, with memory bounds
20000/30000 RC halfwords and password `test`. Unspecified buffer/area claims
take the updater's defaults (four/seven for these inputs). `bs` specifies
entries then segments: key 0 has six entries/30 segments, and keys 1--3 each
have two entries/six segments. Keep the entire four-key clause: an abbreviated
key-0-only form is not the checked grammar.

The terminal record must follow the resource description. `console1` is the
guest terminal name corresponding to the runtime device-4 console in this
setup. `"1"` is the location identifier and `"test"` the terminal password/key,
distinct from the job password even though the demo uses the same text.
The last two numbers configure TEM input buffering (60 buffers) and the
timer count (two timeout periods before an answer), not a host port.
Do not replace these fields with `127.0.0.1` or `4004`. The actual Telnet
endpoint is configured separately. This recipe provisions just this one
terminal/job description, not a general account-management service.

**FP/mipsprep:**

<!-- mips-manual: create-catalog -->
```text
catspec=fromhost catinput
soscat=upsoscat catspec
lookup soscat
catalogbin=tohost conv.bin soscat
scope user soscat
```

For this exact input, expect the updater's normal `end 60`, no rejected
transaction/trans/syntax/abnormal-end diagnostics, and a two-segment temporary
`soscat` descriptor on `disc` before promotion. The packed host export
`prepare/exchange/catalogbin` must be 1536 bytes. The final command retains
the catalog across the preparation process's removal. A file left behind after
an updater error is not evidence of a successful catalog; do not continue
with it merely because `lookup` finds a name.

The manufacturing FP can import the specification even though the demo job
cannot necessarily do so: `fromhost` can initially allocate 36 segments for
a small input, exceeding the demo job's 30-segment temporary claim.

## 7. Prepare startsos and startarea replacements

Create both host text files below with LF endings. Leave `starttem` and
`startprimo` unchanged. In the SOS text the only intended resource change
is the document `disc` to `disc1`; do not reduce the original 2500/200 claim
as a substitute for provisioning the second disk.

**Host file contents: `$MIPS_WORK/prepare/exchange/sosboot`:**

<!-- mips-manual: sos-startup -->
```text
* new sos size 300000 function 1 2 3 4 5 buf 35 area 45
* perm disc1 2500 200 base -8388607 8388605 prog bsos internal 1
* key 3 run
* unstack
```

**Host file contents: `$MIPS_WORK/prepare/exchange/mipsboot`:**

<!-- mips-manual: automatic-startup -->
```text
* kit 16
* date 86 09 30 08 00 00
* read starttem
* read startprimo
* read startsos
* unstack
```

This startup mounts device 16 before the first internal process removes
Guest-s's initialization-only catalog code. It sets the fixed date 30 September
1986 and starts the components without a date-input process. It does **not**
track the current host date; every boot resets the guest clock to that date.

The leading `*` characters and final `unstack` commands are required parts of
the guest startup text. The monitor permits only **two nested input files**:
`startarea` and one component script. Do not put `read startmips` inside
`startarea`: `startmips` would read a third-level component file and produce
`not allowed`. That error is an input-stack limit, not proof of a CPU
protection failure. Each component's own `unstack` returns to `startarea`.

**Warning:** the following `copy` commands replace installed guest startup
entries on the preparation copy. Confirm the selected configuration and
the saved `oldboot`/`oldsos` exports first. Never run them on shipped or
historical media. Keep each `scope user` operation; temporary replacements
may otherwise disappear on `finis`.

**FP/mipsprep:**

<!-- mips-manual: install-startup -->
```text
boottemp=fromhost sosboot
startsos=copy boottemp
scope user startsos
clear temp boottemp
boottemp=fromhost mipsboot
startarea=copy boottemp
scope user startarea
clear temp boottemp
newboot=tohost startarea
newsos=tohost startsos
lookup soscat startarea startsos
finis
```

Wait for the input bell after every line; stop on errors. The two `clear`
commands delete only the named temporary import, not a scope or disk. Compare
`prepare/exchange/newboot` to `mipsboot` and `newsos` to `sosboot` in a host
terminal; the text must match. Retained descriptors must still exist.
Wait for `pause mipsprep finis fp` before leaving.

Press Ctrl-E in the emulator's original terminal. **Emulator CLI:**

<!-- mips-manual: exit-emulator -->
```text
quit
```

Do not copy media while the emulator is running or merely suspended at the
CLI. Wait until the host shell returns.

## 8. Freeze clean bases before any runtime verification

**Host shell, emulator fully stopped:**

```sh
cmp "$MIPS_WORK/prepare/exchange/mipsboot" "$MIPS_WORK/prepare/exchange/newboot"
cmp "$MIPS_WORK/prepare/exchange/sosboot" "$MIPS_WORK/prepare/exchange/newsos"
cp -i "$MIPS_WORK/prepare/MONITOR" "$MIPS_WORK/prepared/MONITOR"
cp -i "$MIPS_WORK/prepare/UNIT0.mipsts" "$MIPS_WORK/prepared/UNIT0.mipsts"
cp -i "$MIPS_WORK/prepare/UNIT1.mipsts" "$MIPS_WORK/prepared/UNIT1.mipsts"
mkdir "$MIPS_WORK/prepared/exchange"
```

Both `cmp` calls must succeed without output. The preparation config writes
directly to its private files, so they now contain all preparation changes.
There must be **no preparation disk overlays**. If overlays were used instead,
stop: copying the base alone would lose changes, while renaming an overlay
as an image would produce an invalid result. This recipe deliberately needs
no overlay flattening or image-reconstruction tool.

Copy only these three named files. Do not copy the whole preparation
directory, its write-through configuration, input/export files or logs.
The bases are frozen **before** any login, swap activity or persistence-test
file is created. Never promote runtime overlays into these clean bases.

## 9. Write the final flat runtime configuration

**Host file contents: `$MIPS_WORK/prepared/RC4000.mipsts.cfg`:**

<!-- mips-manual: runtime-config -->
```text
; MIPS/TS demonstration; paths are relative to this configuration.
; One dummy job/password test/test; fixed boot date 30 September 1986.
PROTECTION MODE=OFF
CPU IX=YES
CORESIZE=0,1048576
MONITOR FILE=MONITOR
OPERATOR ESCAPE=CTRL-E
OPERATOR DATE=ALT-D
INPUT DEL=CTRL-H
DATE YEAROFFSET=-40
THROTTLE VALUE=50%
; THROTTLE VALUE=ORIGINAL
; THROTTLE VALUE=OFF
DEFINE CONSOLE DEVNO=2 INTNO=6,3 PROTOCOL=CLASSIC
DEFINE CLOCK DEVNO=3 INTNO=11
DEFINE RC610 DEVNO=7 INTNO=10 FILE=printer.mipsts.txt APPEND=YES
DEFINE CONSOLE DEVNO=4 INTNO=20,19 PROTOCOL=CLASSIC
; Telnet is unencrypted; keep this endpoint on loopback.
TELNET DEVNO=4 ADDRESS=127.0.0.1 PORT=4004 ATTENTION=CTRL-A
; Independent persistent sidecars, never shared with another base/profile.
DEFINE RC433X DEVNO=15 FILE=UNIT0.mipsts OVERLAY=UNIT0.mipsts.overlay
DEFINE RC433X DEVNO=16 FILE=UNIT1.mipsts OVERLAY=UNIT1.mipsts.overlay
DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT=exchange
DEFINE READER DEVNO=0 INTNO=9
DEFINE PUNCH DEVNO=1 INTNO=12
```

This configuration uses the existing compatibility `MONITOR`, not another
monitor image. Both disks have their own persistent overlay. No `WRITABLE=YES`
remains. Reader and punch have no mounted host files; this does not configure
a PRIMO peripheral service. Leave port 4004 free and run one instance only.

**Host shell:**

```sh
"$MIPS_REPO/build/sim4000" --check-config "$MIPS_WORK/prepared/RC4000.mipsts.cfg"
cd "$MIPS_WORK/prepared"
sha256sum MONITOR UNIT0.mipsts UNIT1.mipsts RC4000.mipsts.cfg
```

Record these hashes. Guest timestamps/allocation can differ from a shipped
snapshot, so a newly prepared disk need not have the shipped disk's exact
hash. Successful preparation and acceptance are not a claim of bit-identical
regeneration. Do not overwrite source images to make their hashes match.

## 10. Verify a disposable runtime copy

Keep `prepared/` untouched as the candidate set. Populate the still-empty
`run/` directory with an explicit allowlist, not a recursive copy.

**Host shell:**

```sh
cp -i "$MIPS_WORK/prepared/MONITOR" "$MIPS_WORK/run/MONITOR"
cp -i "$MIPS_WORK/prepared/UNIT0.mipsts" "$MIPS_WORK/run/UNIT0.mipsts"
cp -i "$MIPS_WORK/prepared/UNIT1.mipsts" "$MIPS_WORK/run/UNIT1.mipsts"
cp -i "$MIPS_WORK/prepared/RC4000.mipsts.cfg" "$MIPS_WORK/run/RC4000.mipsts.cfg"
mkdir "$MIPS_WORK/run/exchange"
"$MIPS_REPO/build/sim4000" -a -c "$MIPS_WORK/run/RC4000.mipsts.cfg"
```

Expect both `disc mounted on 015` and `disc1 mounted on 016`, TEM/PRIMO
release/start messages, and `sos ... started`. Messages may interleave.
There must be no date question, `not allowed`, resource-claim rejection or
abnormal subsystem exit. Do not count a release banner alone as acceptance.

**Host shell in a second terminal:**

```sh
telnet 127.0.0.1 4004
```

**Remote guest attention/SOS:** press Ctrl-A, wait for `att` and the bell,
type `sos` and Enter, then at its `>`/bell input enter:

<!-- mips-manual: rejected-login -->
```text
run test pass wrong
```

Expect `***sos: bad password`, not enrollment. Request attention again,
select `sos` again, and enter **Remote SOS**:

<!-- mips-manual: accepted-login -->
```text
run test pass test
```

Expect `from tem`, `sos: test enrolled`, then an FP input bell.
**Remote FP/test**, one command at a time:

<!-- mips-manual: remote-session -->
```text
message termprobe
lookup fp
finis
```

Require actual `termprobe` output, not only echoed input, an FP descriptor
showing 18 segments on `disc`, and `test removed after finis`. Log out before
disconnecting Telnet. A socket disconnect is not a supported logout/recovery
recipe. Leave the emulator running for the local status and persistence
checks below; stop it only where those steps explicitly say to do so.

### Inspect processes and check persistence across a full restart

During each boot, before stopping the emulator, use the original local
terminal: Escape, `s`, then **Guest-s**:

<!-- mips-manual: inspect-processes -->
```text
list
```

TEM, PRIMO and SOS should have sizes 11000, 16938 and 300000, nonzero keys,
zero stop counts and runnable/waiting states. The `test` child must disappear
after logout. While awaiting interactive input it can show `stop` with stop
count zero; that alone is not a failure.

To verify exact retained data, make a host ASCII/LF file in the disposable
runtime's exchange directory. **Host file contents:
`$MIPS_WORK/run/exchange/keepinput`** (one line, final LF):

<!-- mips-manual: retained-input -->
```text
Persistent MIPS/TS test profile.
```

On the first boot, request local attention, select `s`, then **Guest-s**:

<!-- mips-manual: verification-process -->
```text
all verify base 0 0 login 0 0 user 0 0 project 0 0 run
```

The explicit intervals match the demonstration account. This is a separate
administrative FP, not an expansion of that account's quota. **FP/verify,
first boot only:**

<!-- mips-manual: create-retained-file -->
```text
keeptmp=fromhost keepinput
keepsake=copy keeptmp
scope user keepsake
```

The copy compacts the import before retention. **FP/verify, first boot:**

<!-- mips-manual: read-first-boot -->
```text
lookup keepsake
keepcheck1=tohost keepsake
finis
```

Expect `keepsake` on `disc`, user scope, and normal `pause verify finis fp`.
Use local Ctrl-E, then `quit` at the emulator CLI. With the emulator stopped,
compare **in the host shell**:

```sh
cmp "$MIPS_WORK/run/exchange/keepinput" "$MIPS_WORK/run/exchange/keepcheck1"
"$MIPS_REPO/build/sim4000" -a -c "$MIPS_WORK/run/RC4000.mipsts.cfg"
```

The comparison must succeed. Do not copy the bases again or remove overlays
before this second start: it must resume the same `run/` disk pair and its
two sidecars. Confirm automatic startup and repeat the wrong/correct login,
remote FP commands and logout. Create `verify` again with the same Guest-s
command and matching intervals, but **do not import/create `keepsake` again**.
**FP/verify, second boot:**

<!-- mips-manual: read-second-boot -->
```text
lookup keepsake
keepcheck2=tohost keepsake
finis
```

Stop cleanly again (Ctrl-E, emulator-CLI `quit`). **Host shell:**

```sh
cmp "$MIPS_WORK/run/exchange/keepinput" "$MIPS_WORK/run/exchange/keepcheck2"
cmp "$MIPS_WORK/prepared/MONITOR" "$MIPS_WORK/run/MONITOR"
cmp "$MIPS_WORK/prepared/UNIT0.mipsts" "$MIPS_WORK/run/UNIT0.mipsts"
cmp "$MIPS_WORK/prepared/UNIT1.mipsts" "$MIPS_WORK/run/UNIT1.mipsts"
sha256sum "$MIPS_REPO/src/disks/UNIT0" "$MIPS_REPO/src/monitor/MONITOR"
```

All comparisons must succeed, and the original input hashes must still match.
Only `run/` overlays contain verification writes. Host exports live separately
in `run/exchange/`. Preserve transcripts/errors when acceptance fails; do not
replace a shipped image or claim a working preparation on a banner alone.

## 11. Keep and use the correct files

The clean deliverable is the **prepared/** allowlist: `MONITOR`,
`UNIT0.mipsts`, `UNIT1.mipsts`, `RC4000.mipsts.cfg`, plus an empty `exchange/`.
Use a compatible `sim4000` executable with it. Neither Python nor this source
tree is needed to operate that set. Keep applicable licenses and the User
Manual when assembling a distribution; this procedure does not grant new
rights in inherited RC software.

Do **not** deliver `prepare/`, `run/`, `.overlay` files, `keepinput`,
`keepsake`-bearing runtime state, exports or verification logs as clean bases.
The base files in `run/` are still unchanged, but `prepared/` is the deliberately
untouched candidate source. A wanted continuing user session is different:
back up its stopped bases, configuration and **both** overlays together.

To reset such a continuing session, stop it and set aside both sidecars as
a pair before starting again. This discards guest changes from active use;
never remove an active sidecar or mix it with a different base image. Do not
relabel the already prepared second disk as a reset operation.

## 12. Known limits and verification status

This is one small compatibility demonstration. It is not a protected MIPS/TS
system, arbitrary-account administration guide, multi-terminal application
certification or a secure public service. The fixed date is reset on each
boot. Larger compilers/workloads can exceed the demo claims. General host
imports are not a checked demo-account exercise. Additional host listeners
alone do not create corresponding SOS catalog entries.

TEM/PRIMO/SOS startup and a TEM-mediated FP session are established by this
manufacturing procedure. Subsequent operating checks cover PRIMO-to-printer
output and reconnection of an idle FP, including a known echoed partial line;
use the separate User Manual for those procedures and their limits. They do
not add setup commands to this manufacturing sequence. Punch/reader service,
all TEM controls, interrupted output and crash/power-loss recovery remain
outside that accepted scope. Finish FP normally instead of guessing SOS
kill/break or guest disk-initialization sequences.

The manual sequence has passed the following checks independently of the image-generation
entry point by `tests/python/test_mips_manual.py`. It reads the labelled
input/command blocks in this document, starts from fresh copies of the
specified native inputs, drives the guest commands, freezes bases before
verification, then checks two boots, real loopback Telnet login/logout,
retained text, startup exports, exact packed SOS catalog and media hashes.
It never calls the profile preparer. Generated evidence remains in its
exclusive ignored workspace; no shipped image is updated. The preparation
procedure is repeatable, not bit-reproducible across guest timestamps.
This verifies the exact bounded demo recipe, not arbitrary catalog inputs,
different source snapshots or all historical MIPS/TS functionality.

## 13. Automated alternative

After understanding the manual steps, the developer-only
`dev/python/prepare_mips_profile.py` offers an automated candidate-generation
route. It uses the same guest preparation, preserves its work/transcripts
and exports clean bases rather than runtime overlays. It needs Python and
the developer host-image builder; neither is required by the manual route
or the resulting runtime. Candidate review/acceptance and intentional
promotion remain separate; do not overwrite shipped images automatically.

**Host shell, repository root, optional alternative (new destination):**

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DSIM4000_BUILD_DEVELOPER_TOOLS=ON
cmake --build build --target sim4000 sim4000_build_host_image -j4
python3 dev/python/prepare_mips_profile.py build-mipsts-candidate \
  --simulator build/sim4000 --builder build/sim4000_build_host_image
```
