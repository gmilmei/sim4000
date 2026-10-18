SIM4000 tutorial inputs

Copy this directory beside sim4000, RC4000.cfg, MONITOR and UNIT0 if you
want to follow the manual's development examples without a source checkout.
It is optional: neither the emulator nor normal startup reads these files.

From the emulator's working directory, use a second host terminal to copy
the named example into exchange/ under the input name given in the manual.
For example:
    cp examples/hello.alg exchange/alginput

print_source.alg prints the installed cpuregtest Algol source through the
RC610. Copy it to exchange/print_source.alg and follow the line-printer
chapter. It does not execute cpuregtest. The same source is checked with both
compatibility and protected libraries; use fromhost or rcin respectively.

primo_print.alg submits the guest area pin to PRIMO through the printer
descriptor plp, then waits for completion. Copy it to exchange/prbsource
and follow the MIPS/TS printing section. Use its administrative FP resource
claim, not the small demonstration login. It does not start PRIMO itself.

The .alg and .sl files are guest compiler/assembler sources. The .fp files
contain guest FP commands; review them before importing and executing them.
They are not host shell scripts. Error examples deliberately contain invalid
program names or an unresolved assembler identifier. The integer assembler
example loops forever if executed; the manual explains how to suspend it.

Guest result names and host export names must be unused. Host export refuses
to overwrite existing files. Cleanup in the manual removes only its named
guest entries; exported host results survive guest cleanup.

These inputs are exercised by the corresponding guest acceptance tests.
Edit working copies, not the bundled originals.
