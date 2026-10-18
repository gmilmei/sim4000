# Disk-Image Working-Copy Workflow

This is the fixture-maintenance guide for contributors. Ordinary startup and
disk persistence options are documented in the [User Manual](../usermanual/README.md).
Shell examples here start at the repository root unless explicitly stated
otherwise.

For the complete manual construction of the separate MIPS/TS disk pair, use
[MIPS/TS Manufacturing](../MIPS-TS.md). Its write-through phase is restricted
to exclusive preparation copies; clean bases are frozen before running a
third, overlay-backed acceptance copy. Do not put this manufacturing recipe
into the User Manual or reinterpret it as permission to modify shipped media.

The checked-in `src/disks/UNIT0` is the standard native snapshot, containing
`fromhost` and `tohost`. `src/disks/UNIT0.original` retains the unchanged
historical image with `fromdos` and `todos`. Their identities are:

```text
UNIT0
size     4660224 bytes
SHA-256  e5d293bd01c39385218c11531412156ae4b65afa542c1263b2dd3c919f630a1e

UNIT0.original
size     4658688 bytes
SHA-256  838621f65fef164cf55fca6ad60ff4f1b3ae5b5326fd7f29a38bfc75a4a5b22a
```

Read-only and copy-on-write runs may use either directly. Any test or operator
session intended to modify a complete disk image must use a verified working
copy.

## Create a blank additional disk

The simulator itself creates a fresh unlabelled starter image:

```sh
build/sim4000 --mkdisc work/UNIT1
```

The parent directory must already exist. Creation never replaces a path and
writes 180 zero-filled 768-byte sectors, not a formatted filesystem. The
equivalent suspended operator command is mkdisc FILE. Use an isolated config
with an additional RC433X device and a new sidecar; do not overwrite a base
image to initialize the guest disk. The developer recipe and current evidence
are in [MIPS/TS Plan](MIPS_TS_PLAN.md).

## Create and use a working copy

Create the copy at a new path. The tool verifies the source before creating
the destination, uses exclusive creation, flushes the result, and verifies the
copied size and SHA-256 digest. It never replaces an existing destination.

```sh
mkdir -p work
python3 dev/python/make_unit0_copy.py work/UNIT0.test
```

The default source is the native `src/disks/UNIT0`. To create an original-image
working copy, add `--source src/disks/UNIT0.original`. The tool accepts only
the recorded pristine identities; it rejects modified images.

Enable direct writes only together with the explicit working-copy path:

```sh
(cd build && ./sim4000 -a \
  --disk ../work/UNIT0.test \
  --disk-write-through)
```

The equivalent configuration form is:

```text
DEFINE RC433X DEVNO=15 FILE=work/UNIT0.test WRITABLE=YES
```

At a suspended operator prompt, a detached disk can be attached in the same
mode with:

```text
attach 15 work/UNIT0.test writable
```

Write-through mode changes and may extend the named image immediately. It does
not allocate overlay sectors. Never select the checked-in `src/disks/UNIT0` as
its target; the same applies to `UNIT0.original`. The command-line option requires an explicit
`--disk` argument and conflicts with `--disk-overlay`.

For an isolated test, create the working copy inside a temporary directory,
run the test, retain it only when investigating a failure, and otherwise
remove the entire temporary directory. A new test run always starts with a new
verified copy; an image left by a previous run is never reused implicitly.

## Build the native host-utility image

The shipped native `UNIT0` snapshot was generated from `UNIT0.original` by
replacing its global `fromdos` and `todos` entries with source-built
`fromhost` and `tohost`. Rebuild a separate `UNIT0.host` candidate with:

```sh
cmake --build build --target native-image
```

The build copies the verified `UNIT0.original` into a private staging directory,
boots the delivered monitor, imports the maintained Algol sources using the
original utility, renames the global entries, and compiles into those entries
with the disk-resident Algol compiler. An administrative `all` process is
necessary to modify the global catalog; its temporary disk claim is limited
to 1000 segments and 20 entries. The compiler reports `algol end 55` and
`algol end 47`. Temporary source/test entries are removed before validation.

Catalog checks require the new names, executable contents, original global
base interval and permission key, and absence of the old names. A second
boot in copy-on-write mode creates a normal user process and repeats text and
binary transfer tests. Text is compared exactly with LF conversion. Binary
testing compares a complete 768-byte input prefix, including all byte values,
and permits the historical allocation padding at the end of the export.

Only after those checks is the image published at its new destination. The
original SHA-256 is checked before and after the build. The tool never
overwrites an existing image; to regenerate a candidate after source/tool changes, first
move the previous derived image to a backup path. CMake may report this
non-overwrite error when its dependencies are newer than an existing image.

An explicit destination is also supported:

```sh
python3 dev/python/make_host_image.py work/UNIT0.host \
  --builder build-release/sim4000_build_host_image
```

The destination's parent directory must exist. The C helper is a low-level
builder for a disposable working image; use the Python wrapper or CMake target
to obtain verified copying and publication. Generated images are kept in
ignored build directories. Record the printed SHA-256 if distributing a
particular generated version. The native-image integration test compares a
freshly rebuilt candidate byte for byte with the shipped `src/disks/UNIT0`.
When intentionally changing guest sources or generation behavior, regenerate
and promote a validated candidate to `src/disks/UNIT0`, then update the native
identity in the working-copy tool, fixture tests, and documentation.

Normal CMake builds copy both shipped images and create `build/exchange`.
The standard `RC4000.cfg` selects `UNIT0` and enables `HOSTFILES` with that
root. Start from `build` with `./sim4000 -a`. Disk writes remain volatile by
default. Select `--disk UNIT0.original` to run the preserved historical image,
or `--disk UNIT0.host` to try a newly rebuilt candidate.

## Verify and restore the fixture

Verify the source fixture without modifying it:

```sh
sha256sum src/disks/UNIT0
stat -c '%s %n' src/disks/UNIT0
sha256sum src/disks/UNIT0.original
stat -c '%s %n' src/disks/UNIT0.original
```

If either value differs, stop all emulator processes and preserve the suspect
file under a different name for investigation. Restore `UNIT0` from the Git
object recorded by the current revision, then verify it again:

```sh
mv src/disks/UNIT0 src/disks/UNIT0.damaged
git restore --source=HEAD -- src/disks/UNIT0
sha256sum src/disks/UNIT0
```

Do not restore from a prior working copy: it may contain valid but unwanted RC
disk writes. The automated working-copy test verifies successful copying,
destination non-overwrite, rejection of a non-pristine source, and cleanup
after failure.

## When overlays remain useful

Overlays are optional. A full working copy is preferable for integration
tests, SLANG builds, and experiments that must behave like a normally writable
disk. Volatile overlays remain convenient for disposable interactive sessions;
persistent overlays remain space-efficient when a small set of changes should
survive without duplicating the complete image.
