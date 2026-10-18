# Provenance and Licensing

## Original basis and attribution

The native emulator grew from the original DOS emulator designed and programmed
by Henrik A. Jacobsen (Henrik Ascanius Jacobsen). It is a new C implementation,
not his original program. Historical behavior and preserved media remain its
compatibility basis. Detailed comparisons with the old host implementation are
not part of the maintained developer documentation.

Retain the original author credit and applicable copyright notices.
Permission to reuse Jacobsen's own work with attribution is recorded for the
project. The public permission notice is retained in
[V_1.2_DB_LICENSE.txt](V_1.2_DB_LICENSE.txt); its source is
[the pinned upstream license](https://github.com/henrikascanius/SIM4000/blob/01a2a0247972bf305529f4fabc21241155a213de/LICENSE.md).
That notice does not assign terms to every historical component.

## Native project license

The root [LICENSE](../../LICENSE) offers MIT terms for new project contributions
only within their contributors' rights. Its caveats exclude historical and
third-party material from that grant. Source adaptation, a change of language
or a successful rebuild is not proof of independent authorship or permission
to relicense inherited expression.

The GNU Readline dependency has separate GPL version 3-or-later terms.
[GPL-3.0.txt](../../licenses/GPL-3.0.txt) retains the license text. System linking
does not itself settle the conditions for publishing a combined executable.
The packaging workflow includes both notices and the full root caveats; it
does not issue a publication clearance.

## Materials and boundaries

| Material | Maintained identity and rights boundary |
| --- | --- |
| Native runtime, tests and tooling | C/Python/CMake project contributions; the new-contribution MIT grant does not override inherited or dependency rights. |
| src/monitor/MONITOR | Preserved modified monitor binary; guest compatibility fixture, not newly authored host software. |
| src/monitor/STDMON | Verified export of the disk-resident standard monitor; reproduced media, not a new monitor implementation. |
| src/disks/UNIT0.original | Unchanged historical system disk with inherited guest software. |
| src/disks/UNIT0 | Derived historical disk containing adapted native host-transfer utilities; not an independently created operating system. |
| src/guest/ | Maintained Algol transfer utilities with substantial inherited code. Attribution is recorded in their README. |
| monsrc/ | Historical SLANG monitor sources normalized to LF and lowercase filenames, with later maintenance and emulator-specific changes. |
| Documentation and examples | Native explanations alongside historical names, transcripts and reference fragments; third-party content retains its own rights. |

Historical RC/ICL software and other parties' material must be assessed separately.
Availability in a public archive is access/provenance evidence, not by itself
a redistribution grant. The root MIT selection cannot resolve such questions.
Copyright and attribution names are retained where required; private
correspondence, personal paths and project-conversation details are not
documentation inputs.

[Disk Workflow](DISK_WORKFLOW.md) owns the bundled disk identities and
candidate promotion process. [Development](DEVELOPMENT.md#rebuild-the-historical-monitor)
documents monitor reproduction; [Architecture](ARCHITECTURE.md#source-provenance)
records its lineage and authenticity limits.

## Monitor-source authenticity

The maintained source identifies Monitor 3 version 10.11.77 and contains later
maintenance and emulator-specific modifications. The included generation
configuration reproduces the complete MONITOR byte for byte through the guest
SLANG assembler. This establishes a matching source/build pair, not an
untouched vendor release or the original 1969 Monitor 1 source.

The independently archived RC8000 monitor listing has substantial common
system/catalog text, but is a different artifact and not a directly assemblable
replacement. Artifact identities and comparison limits are documented in
[the monitor listing findings](ARCHITECTURE.md#independent-monitor-listing-from-datamuseum).
No such archive replaces the bundled source or monitor automatically.

## Publication checklist

Before a public source or binary release:

- retain author credit, the scoped MIT grant and applicable third-party notices;
- determine the rights and allowed distribution scope for inherited guest media,
  monitor sources, adapted utilities and reference fragments;
- review the Readline-linked executable's applicable distribution obligations;
- choose which media, source and history can be published, including material
  present in earlier Git objects;
- if needed, design a separate verified external-media acquisition workflow
  rather than assuming a source-only release solves every rights question;
- validate the selected release revision and archive manifest.

No history rewrite is implied. Removing a file from the current tree does not
remove it from Git history. This document records engineering provenance and
open rights boundaries; it is not legal advice or an authorization from other
rights holders.
