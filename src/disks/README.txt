Bundled disk images

UNIT0
  Default compatibility disk. Derived from UNIT0.original by rebuilding the
  Algol host-transfer programs as fromhost/tohost. No MIPS/TS autostart.
  SHA-256: e5d293bd01c39385218c11531412156ae4b65afa542c1263b2dd3c919f630a1e

UNIT0.original
  Preserved historical disk, including fromdos/todos. Available in the source
  distribution only; not needed for ordinary use. Never edited in place.

UNIT0.protected
  Derived system-binding disk for MONITOR.protected and RC4000.protected.cfg.
  The older tofrom descriptor is system-scoped with matching catalog ranges;
  separately linked rcin/rcout provide protected text/binary host transfers.
  Original libraries and incompatible newer-machine fromhost/tohost remain
  installed; not all software is protection-compatible. Guest cmp still
  pauses in tofrom1. This is not a complete RC8000 compatibility guarantee.
  Size: 4660224 bytes
  SHA-256: ebc56434088ac0d0a5655924646a45bfd3d739bbb2217f45622c9626641f2483
  Reproduce using probe_rc4000_runtime.py --algol --system-binding after the
  guarded monitor rebuild; see doc/impl/DEVELOPMENT.md.

UNIT0.mipsts
  Compatibility disk derived from UNIT0 on private media through guest FP.
  startarea mounts device 16, sets 30 September 1986 and directly starts TEM,
  PRIMO and SOS. startsos moves its normal 2500/200 claim to disc1. soscat
  contains one demonstration account test/test, terminal console1 and small
  temporary/retained disk claims. Protection is off; use RC4000.mipsts.cfg.
  Size: 4660224 bytes
  SHA-256: c3200b837e09ee0af054b47e7ac1b447f61c46c0e34a9fa4eae080da4c0e4c75

UNIT1.mipsts
  Prepared companion disc1 for UNIT0.mipsts, device 16. Created by the
  integrated mkdisc and initialized with guest kitlabel. Do not relabel it.
  The starter image grows through its overlay as guest sectors are written.
  Size: 138240 bytes
  SHA-256: 63e941705e51bb9b61bc005af54929e5798b57e78bdb188506934140273c47ba

All variants are prebuilt: ordinary builds and binary packaging need no
Python, guest compilation or image-generation step. Never mix overlays or
base images between profiles. The MIPS/TS profile enables two separate
persistent overlays; neither runtime sidecar belongs here or in the archive.
These MIPS/TS bases were prepared before the two-boot verification, not
flattened from its runtime overlays: no verification keepsake file is included.

For the complete manual manufacturing procedure, see doc/MIPS-TS.md. It covers
private write-through preparation, guest disk/catalog/startup commands, clean
base-image freezing and a separate two-boot Telnet acceptance copy. This is
not part of the User Manual, which describes operating the ready-made images.
The guide ends with an optional automated candidate-generation alternative.
Preserve transcripts/manifests, review and verify before intentional promotion.
Preparation timestamps can vary; matching the old image hash is not a claim
of bit-reproducibility. Details and evidence: doc/impl/MIPS_TS_PLAN.md.

Original emulator/monitor adaptations are credited to Henrik A. Jacobsen.
Inherited historical-software licensing caveats in LICENSE apply to all
derived disks. No historical library binary is patched by these preparations.
