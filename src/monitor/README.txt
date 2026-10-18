Bundled runtime monitor images

All maintained monitors are source-built through monsrc/sim4000.mops and
include printer (kind 14), device 7, interrupt 10. Historical hjmops* and
the original source tree are unchanged. All files below are ready to run;
Python and guest rebuilding are not ordinary-build or runtime dependencies.

MONITOR is the compatibility monitor for RC4000.cfg and RC4000.mipsts.cfg.
Size: 38400 bytes
SHA-256: dff975962b89bf87d5dee703701777ad6274c53f550120f9ed9bf42568e42a37

MONITOR.protected restores key setting through the guarded guest-editor e22
correction in sim4000-protected.mops, followed by the shared sim4000.mops build.
It is byte-identical to the previously verified protected image.
Select RC4000.protected.cfg with UNIT0.protected; this is
not the binary-only V2 monitor. It includes the same RC610 driver/mapping.
Size: 38400 bytes
SHA-256: 9055b2f5aeaa9a5bc4e7f57a8bd5dd38cca6aceb1c5c98eb4deeeecd936cda62

STDMON uses the hardware DFC403/RC4819 disk interface. The guarded --dfc403
build selects disk options from the same maintained generation file.
Select RC4000.dfc403.cfg. Reader/punch use interrupts 9/12, printer uses 10.
Size: 38400 bytes
SHA-256: 07a8a16188fdc065671659399718ae8bb67944f892483d03c58d79e3e31b80fd

The developer-only standard-monitor target now rebuilds STDMON from these
sources and writes generated/STDMON. It no longer exports the older
printer-free disk-resident stdmonitor. That historical disk entry and all
shipped disk images remain unchanged. Rebuilds never promote automatically.

All supplied configs open a profile-specific printer file automatically
with APPEND=YES. Existing contents survive restarts; completed prints are
flushed immediately. Guest-s printing is checked in all three variants;
an Algol source-listing example is checked in compatibility mode. Physical
PRIMO spooling remains an independent acceptance task.

MIPS/TS automatic startup belongs to UNIT0.mipsts/UNIT1.mipsts, not a fourth
monitor. It uses MONITOR. Construction is documented in doc/MIPS-TS.md;
operation belongs in doc/usermanual.pdf (source in doc/usermanual/).

Reproduction, artifact identities and test scope are in doc/impl/RC610.md,
doc/impl/DEVELOPMENT.md and doc/impl/TEST_FIXTURES.md.
Original monitor/emulator adaptations are credited to Henrik A. Jacobsen;
inherited historical-media licensing caveats in LICENSE still apply.
