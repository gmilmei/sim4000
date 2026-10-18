Bundled runtime configurations

RC4000.cfg
  Default compatibility MONITOR/UNIT0, volatile guest disk writes and no active
  Telnet listener. The build also creates the identical RC4000.compat.cfg alias.

RC4000.protected.cfg
  Source-built MONITOR.protected and matching UNIT0.protected. RC4000 protection
  on, IX enabled, volatile writes by default. Use protected guest rcin/rcout.

RC4000.mipsts.cfg
  Compatibility MONITOR, UNIT0.mipsts and UNIT1.mipsts. Protection off, IX on,
  loopback Telnet on port 4004, independent persistent overlays for both disks.
  Its disk startarea starts TEM/PRIMO/SOS and sets the fixed historical date.
  The provisioned demonstration job/password is test/test; do not expose it.

RC4000.dfc403.cfg
  Optional STDMON/UNIT0 DFC403/RC4819 hardware-interface configuration, supplied
  in the source/build tree, not the binary archive. Not the MIPS/TS profile.

Paths are relative to the selected configuration. All supplied runtime
profiles use exchange/ for host transfers. Never mix profile disks/overlays.
Ordinary builds copy ready-made files; they do not prepare disk images.

All maintained configs include RC610 DEVNO=7 INTNO=10. FILE selects a
profile-specific printer text file; APPEND=YES opens it automatically and
preserves existing contents across restarts. Completed prints are flushed
immediately, including CR/VT/FF without LF. APPEND=NO requires a new file.
Omitting FILE keeps the device installed but offline until CLI attach.
Host printer output is independent of disk overlays and survives their removal.

The User Manual documents operation of the finished profiles. The separate
doc/MIPS-TS.md gives the complete manual manufacturing procedure and its
temporary write-through preparation configuration. Never replace a shipped
runtime configuration with that manufacturing configuration or point its
write-through declarations at shipped/historical images.
