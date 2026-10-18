# SIM4000 User Manual

The authoritative user manual is now written in LuaLaTeX. Its sources live in
this directory, split into twenty chapters. Edit the `.tex` files directly;
Markdown and Pandoc are not part of the manual build.

The MIPS/TS chapter is `19-mips-ts.tex`, in the guest-operation part of the
manual. It distinguishes the shipped demonstration's accepted recipes from
historical reference syntax and unverified administration/recovery workflows.
Its retained-file exercise now covers compact seed preparation, actual remote
account retention, later login/restart readback, exact host exports and explicit
cleanup on the shipped unprotected profile. Eight marked command blocks are
bound to the focused developer acceptance dialogue; they are not build inputs.
The batch/jobfile exercise likewise covers safe compact preparation, both
independently checked local SOS password forms, actual output/normal removal,
preserved input and explicit cleanup; its eight marked blocks have the same
source-only dialogue binding. These are unprotected-profile operating recipes,
not disk manufacturing or protected-candidate instructions.
The chapter opens with a workflow table linking tasks, command contexts and
procedures with printed-page references. File and batch exercises have staged
headings; printing precedes the consolidated troubleshooting/reference sections.
The supported-operation documentation is complete for the supplied unprotected
demonstration. It covers quotas, diagnostics, retained files, export refusal,
batch completion, sequential printing and detached-output recovery, idle TCP
reconnection, known echoed partial-line continuation/Backspace correction and
non-destructive handling of otherwise unverified failures. Optional larger
accounts and parallel sessions are distinguished from shipped configuration;
their creation is not an extra normal-startup step. This is documentation
completion, not certification of every historical command or future feature.
The explicitly requested PDF refresh includes the completed partial-line and
failure-preservation guidance. Changed operating pages and navigation are
visually reviewed before delivery. Future source changes still require an
explicit PDF build and visual review request; an editorial milestone alone
does not trigger regeneration.

## Build the PDF

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target user-manual
```

The target builds `build/usermanual/usermanual.pdf` and copies it to the
versioned distribution artifact `doc/usermanual.pdf`. This optional target is
available when CMake finds `lualatex` and `makeindex`; it is not included in the ordinary
emulator build. If LuaLaTeX was installed after configuration, run CMake again.
The target runs LuaLaTeX three times and MakeIndex once to resolve contents,
the subject/command index and cross-references. Build it explicitly when a
review PDF is explicitly requested by the user, never automatically after
documentation edits or at an editorial milestone. Source-only checks may be
run without a PDF build. If the sources are
unchanged, invoking the target reuses the existing PDF.

Use a TeX Live installation with LuaLaTeX, KOMA-Script, fontspec, unicode-math,
geometry, microtype, array, booktabs, longtable, calc, footnotehyper, xcolor,
listings, xurl, hyperref, bookmark, makeidx and scrlayer-scrpage, plus MakeIndex.
Install the Libertinus
OpenType fonts in the TeX search path and DejaVu Sans Mono where fontspec can
find it. The manual uses Libertinus Serif, Sans and Math, and DejaVu Sans Mono
with `Scale=MatchLowercase`. No font substitutions are intended.

Logs, auxiliary files and rendered review pages belong in the ignored build
directory. The finished `doc/usermanual.pdf` is deliberately tracked in Git,
so users of either a source or binary distribution can read it without TeX.
Only the explicitly requested `user-manual` target updates this PDF; ordinary
builds and source checks never regenerate it.

## Check sources without building a PDF

From the repository root:

```sh
python3 dev/python/check_manual_sources.py
python3 tests/python/test_manual_sources.py
```

These standard-library checks need neither TeX nor a compiled emulator. They
check UTF-8/LF sources, duplicate labels, internal procedure references,
document inputs, repository-document paths, environment balance and unescaped
underscores in example titles. Comments and verbatim example bodies are ignored
for structural references. The include graph must reach every manual TeX source
from the main document and contain no cycles; inputs must stay within the manual
TeX source set. The checker follows this manual's TeX conventions;
it is not a general TeX parser, command-content validator or pagination check.

The focused validator tests also run as `sim4000_manual_sources` under CTest
when developer tests are explicitly enabled and Python is available. Fresh
ordinary builds default to `BUILD_TESTING=OFF`: they build and execute no
tests. The validator tests never trigger the optional PDF target. PDF
layout verification remains separate and requires an explicit user request.

## Reading and source guide

The PDF provides a linked table of contents and internal procedure links.
Internal links include printed page references. The subject/command index
points to the relevant procedures, not only to broad chapter introductions.
Getting-started, process, file, retained-file, editor, SLANG, FP command-file and
diagnostic examples distinguish input contexts from output or source-file contents.
Emulator-reference examples distinguish configuration contents, shell calls and
operator commands; local and remote key notation is explained in the introduction.
The montest dialogs also separate command input from record navigation. Short
operational notes highlight destructive actions and version differences.
The three parts cover getting started, guest work, and the emulator/device
reference. Build-and-start leads directly to the first monitor session;
copying runtime files is an optional alternative. Chapter source filenames
retain their original prefixes and do not specify the current reading order.
Editor, comparison, SLANG and command-file procedures have their own contents
entries. The introduction includes a short terminology guide.

Tutorial inputs live in [examples](examples/README.txt), separate from the
test tree. Copy that directory beside the runtime files to follow the examples
without a source checkout. Inputs match the corresponding test fixtures; do
not change an example without reviewing the documented expected results.

- [Main document](usermanual.tex) and [typography](preamble.tex)
- [Build and start](01-build-and-start.tex)
- [First monitor session](03-first-monitor-session.tex)
- [Runtime files](02-run-from-your-own-working-directory.tex)
- [Everyday operation](04-everyday-operation.tex)
- [Processes and command contexts](05-processes-and-command-contexts.tex)
- [Files and Algol programs](06-files-catalog-entries-and-algol-programs.tex)
- [Keep guest files](07-keep-guest-files-across-sessions.tex)
- [Software and files](08-software-and-files-in-the-supplied-disk-image.tex)
- [Host file exchange](09-host-file-exchange.tex)
- [MIPS/TS operation](19-mips-ts.tex)
- [Starting and controlling the emulator](10-command-line-options.tex)
- [Paper tape](11-paper-tape-files.tex)
- [Optional V2 terminals](12-optional-v2-terminal-protocol.tex)
- [Magnetic tape](13-magnetic-tape-files-rc749.tex)
- [Debugging](14-debugging-and-core-files.tex)
- [Throttle](15-execution-throttle.tex)
- [Keyboard and date](16-keyboard-and-date-settings.tex)
- [Telnet](17-telnet-consoles.tex)
- [Configuration and persistence](18-configuration-and-disk-persistence.tex)

For hardware boundaries and known limitations, see
[Compatibility and Limitations](../COMPATIBILITY.md). Implementation notes
remain in [doc/impl](../impl/README.md), separate from this user manual.
