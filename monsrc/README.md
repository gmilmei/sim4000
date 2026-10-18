# Monitor sources and generation file

The segmented sources and historical generation files are LF-normalized
copies of the historical `src/MON` build inputs, with lowercase source
filenames. They retain original comments, encodings and emulator-specific
driver names. `sim4000.mops` is the separately maintained generation file
described below. The original historical directory remains unchanged.

## Maintained build input

`sim4000.mops` is the active FP generation file. All new build-option changes
belong here, not in `hjmops*`. The rebuild tool always uses it, including
protected and DFC403 builds. The separate `sim4000-protected.mops` entry calls
this common file rather than duplicating its commands/options.
Every maintained build includes the RC610 printer
on device 7, interrupt 10. No printer-free build variant is maintained.

It is derived from `hjmops.new`, with an English provenance header, LF endings
and RC seven-bit substitutions for the Danish comment characters. Its initial
generation commands/options inherit that baseline, with the explicit addition
of printer. The guarded --dfc403 preparation selects hardware disk options;
--protected selects `sim4000-protected.mops`, which restores key setting with
the guest editor. Both still consume the same shared generation file; the
historical references are never edited.

The build uses:

- `montxt1` through `montxt11`: the main segmented SLANG source.
- `mon8s`: the monitor's operating-system command interpreter.
- `mon8init`: catalog initialization and final relocation/startup segments.
- `sim4000.mops`: maintained FP editor commands, generation options, and
  special drivers.
- `sim4000-protected.mops`: small guest-editor wrapper for the protected build.

## Protected guest edit and FP input depth

The wrapper first compacts padded montxt10/mcommon imports through guest
copy/clear/rename, preserving text within the existing 2000-segment claim.
It then creates temporary `ptxt10` with `edit montxt10`, finds `e22` and
deletes only the known HJ jump that skips the existing key-setting loops.
It keeps the guest original under `pkoriginal`, supplies the edited temporary
under the name `montxt10` while the shared build runs, then restores the
original guest name. The edited temporary is retained for verification.
Neither the checked-in segment nor the historical sources are changed.

The host driver imports the selected entry as `mbuild` and, for protection,
the shared file as `mcommon`. From the console, `i mbuild` uses input depth 1;
the wrapper's sole `i mcommon` uses depth 2. The shared file includes no further
FP command file. Editor input and SLANG `type.yes` consume these existing
streams, not a third FP file. Do not add another wrapper/include layer.

Python only checks the known `montxt10` identity and unique bypass before
preparation; it no longer patches source. After the guest run it checks the
editor log, exported `ptxt10` and restored original, plus assembler diagnostics
and the candidate. Missing, changed or ambiguous inputs and extra FP input
levels are rejected. Failed editor/assembler runs are not successful builds.

## Historical generation files

`hjmops`, `hjmops.new` and `hjmops.old` are historical reference files and must
remain unchanged. `hjmops` is identical to `hjmops.new` in this snapshot. `hjmops.old` and
`mon8mops` are retained alternative generation configurations; they have not
been verified by this rebuild milestone. Obsolete host import/batch scripts
are not needed and are not copied here. The rebuild tool no longer accepts
`--variant`: historical files are reference material, not build entry points.

## Rebuild

Run from the repository root after a release build:

```sh
python3 dev/python/rebuild_monitor.py build-release/monitor-rebuild --verify-boot
```

The tool normalizes only disposable source copies, executes the guest editor
and SLANG assembler, retains logs, and exports a separate candidate. The
historical `hjmops.new` result was verified byte-identical to
`src/monitor/MONITOR`. The maintained generation file preserves its normalized
commands/options plus the printer declaration. Compatibility, protected and
DFC403 variants have been rebuilt, checked and promoted to src/monitor/.
All use automatic append-mode printer output in the shipped configs. Future
rebuilds still export private candidates and never promote them automatically.
See the [Development Guide](../doc/impl/DEVELOPMENT.md#maintained-rc610-printer)
for details and limitations.
