# Adapted Historical Host-Transfer Utilities

The original author of these Algol programs is **Henrik A. Jacobsen**, credited
as `HJ` in the historical source headers. These are adaptations, not wholly new
programs. Their names and host-facing diagnostics do not change that origin.

| Maintained source | Historical source in the original collection |
| --- | --- |
| `fromhost.alg` | `src/CTRL/PC/TFROMDOS.ALG` |
| `tohost.alg` | `src/CTRL/PC/TTODOS.ALG` |

Both historical headers record an initial entry dated 1993-04-21 mentioning
`connect.no`, and an `HJ` revision dated 1993-10-20 concerning `conv.dos` and
`conv.iso`, with DOS conversion as the original default. The exporter's original
header also uses the name `tfromdos`; its file is nevertheless `TTODOS.ALG`.

The native adaptations change diagnostics to host terminology, initialize
conversion state, select byte-preserving text mode 2, remove DOS text-conversion
aliases, and rename the exporter's zone from `zdos` to `zhost`. Most program
bodies remain historical. The monitor process name `dos` is retained for
compatibility with the supplied monitor.

Attribution and revision information are recorded here without changing the
tested Algol source snapshot. During image generation, sources are imported as
temporary guest files; even extra comments change the generated disk bytes.
The maintained source files therefore remain unchanged, preserving the
byte-for-byte native-image rebuild.

This notice grants no redistribution permission and applies no new license.
Historical permissions and the publication scope remain unresolved. See the
[Source Provenance Review](../../doc/impl/PROVENANCE_REVIEW.md) for the evidence
and outstanding decisions.
