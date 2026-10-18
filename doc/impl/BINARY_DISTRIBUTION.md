# Binary distribution target

This is a developer packaging workflow, not part of the ordinary build.
It assembles an archive locally; it does not upload or publish anything and
does not resolve the historical-media or linked-library caveats in LICENSE.

## Build

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --target binary-dist
```

The target depends only on the runtime executable. No Python, test execution,
media regeneration, PDF rebuild or developer-tools option is required.
If needed, update the versioned PDF beforehand through the separately requested
user-manual target. Packaging fails rather than silently omitting a missing
required document or license notice. Sanitizer builds are rejected.

The archive name has this form:

```text
build/dist/sim4000-VERSION-SYSTEM-PROCESSOR.tar.gz
```

Version, lowercased system name and processor come from the configured
project/build (`CMAKE_SYSTEM_NAME` and `CMAKE_SYSTEM_PROCESSOR`). This naming
does not establish that the target platform has been tested.
The archive always unpacks beneath one directory named sim4000, irrespective
of its archive filename. It preserves executable permission (0755); data files
are 0644 and directories are 0755.

## Contents

```text
sim4000/
  sim4000
  MONITOR
  UNIT0
  RC4000.cfg
  RC4000.compat.cfg
  RC4000.protected.cfg
  MONITOR.protected
  UNIT0.protected
  UNIT0.mipsts
  UNIT1.mipsts
  RC4000.mipsts.cfg
  PROFILES.txt
  README.md
  LICENSE
  doc/usermanual.pdf
  licenses/GPL-3.0.txt
  licenses/V_1.2_DB_LICENSE.txt
  examples/                  twelve guest inputs plus their README
  exchange/                  empty, ready for host-file transfers
```

Only sim4000 is a runtime executable. The internal static core library, test
executables and guest-session builder are not binary-distribution tools.
The archive contains compatibility, protected and MIPS/TS RC433X profiles. The unchanged
compatibility monitor/native UNIT0 remain at the root, selected by RC4000.cfg
(default) or its explicit identical RC4000.compat.cfg. RC4000.protected.cfg
selects MONITOR.protected and UNIT0.protected. All use the root exchange/.
The protected
artifacts are the M3-verified source-built monitor and derived system-binding
software profile; packaging checks their exact SHA-256 values before staging.
They are supplied prebuilt, not regenerated during packaging or ordinary builds.
The MIPS/TS configuration uses MONITOR with UNIT0.mipsts and UNIT1.mipsts.
Its disk startup automatically mounts disc1 and starts TEM/PRIMO/SOS, with a
fixed historical date and one demonstration account test/test on loopback
Telnet port 4004. It uses two persistent, profile-specific overlays; these
sidecars are never packaged. Their base-image hashes are pinned at packaging
time, just like the protected artifacts. The ordinary build needs no Python
and never prepares or tests these images. See the MIPS/TS manual chapter for
operation, shutdown, persistence and compatibility limits.
STDMON, RC4000.dfc403.cfg and UNIT0.original remain available
in the source distribution, not in this binary package.
Source/config/media are copied from the checked-in source
locations, not from writable files or modified configurations in a build tree.
Tutorial inputs use an explicit allowlist rather than copying arbitrary files.

Packaging uses a fresh owned staging directory under build/dist, publishes
the archive only after successful tar creation, and removes that staging
directory. A failed preflight or sanitizer rejection leaves an existing archive
unchanged. Reinvoking the target intentionally replaces its generated archive;
the output remains ignored by Git.

## Try the result

Extract into an empty destination, substituting the generated archive name:

```sh
tar -xzf build/dist/sim4000-VERSION-SYSTEM-PROCESSOR.tar.gz -C /path/to/empty-directory
cd /path/to/empty-directory/sim4000
./sim4000 -a
# Or explicitly choose a profile (run one at a time):
./sim4000 -a -c RC4000.compat.cfg
./sim4000 -a -c RC4000.protected.cfg
./sim4000 -a -c RC4000.mipsts.cfg
```

The executable must match the destination host's operating system and
architecture. The host supplies a compatible dynamic loader and shared
libraries, including GNU Readline. This is not a statically linked or
platform-independent container. Neither Python nor TeX is needed on the
destination host. See [Development](DEVELOPMENT.md#host-portability) for the
development and test platform and portability limits.

Keep the entire extracted directory together. The protected profile uses
`rcin`/`rcout`; its original installed `fromhost`/`tohost` remain incompatible
with strict protection. PROFILES.txt supplies operational notes alongside the
existing ready-built PDF, which is not automatically rebuilt. Compatibility
and protection default to volatile writes; MIPS/TS defaults to two persistent
overlays. The profiles' disks/overlays must not be interchanged. CLASSIC terminals
are used by all three; no binary-only V2 monitor is included. Selecting the optional
protected configuration does not change the compatibility default.

## Focused verification

With BUILD_TESTING=ON, sim4000_binary_dist tests archive paths, input hashes,
included/excluded files, tutorial inputs, executable permission through actual
execution, all profiles' explicit protection/IX selection, date-prompt boots
for compatibility/protection and unattended MIPS/TS startup
from an unrelated working directory, unchanged packaged media, staging cleanup
and refusal cases. It also
rejects inclusion of the original disk or alternate-monitor files. It uses
CMake and the built emulator, not Python. Run it explicitly:

```sh
ctest --test-dir build-test -R '^sim4000_binary_dist$' --output-on-failure
```

Normal builds neither compile nor run tests.

The optional Python acceptance helper additionally checks two complete boots
and real Telnet login/logout against an extracted archive in a fresh workspace:

```sh
python3 tests/python/test_mips_profile.py build-mipsts/archive-acceptance \
  --runtime-root /path/to/extracted/sim4000
```

This opens loopback port 4004 and retains logs under the exclusive destination.
No Python helper is included in or needed by the binary distribution.

## Licensing boundary

The root LICENSE offers MIT terms for new native-project contributions only
within their contributors' rights. Preserve its full caveat section and the
two third-party notices when moving the binary archive. Historic RC/ICL media
rights and the conditions for publishing a Readline-linked binary remain
separate questions; creating or testing this archive is not clearance.
See [the provenance review](PROVENANCE_REVIEW.md).
