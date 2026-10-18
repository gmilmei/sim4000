# Implementation Documentation

Contributor reference for the native emulator. Runtime operation belongs in
the ready-built [User Manual](../usermanual.pdf); its editable sources are
described in [the manual source guide](../usermanual/README.md).
[Compatibility and Limitations](../COMPATIBILITY.md) defines the product boundary.

## Start here

1. Read [Development](DEVELOPMENT.md) for source layout, build profiles, focused
   checks, media-generation entry points and documentation maintenance.
2. Use [Architecture](ARCHITECTURE.md) for machine/device semantics,
   [Input Policy](INPUT_POLICY.md) for frontend contracts and
   [Trace Format](TRACE_FORMAT.md) for trace records.
3. Consult [Test Fixtures](TEST_FIXTURES.md) for independent expectations and
   explicit verification limits.
4. Use [Roadmap](PORTING_PLAN.md) and [Current Status](PROGRESS.md) to continue
   development without reconstructing a conversation or work log.

The roadmap's P0--P6 sequence is the consolidated follow-up plan, including
deferred printer extensions, broader MIPS/TS operating coverage, the protected
profile gate and deferred release/portability work. The agreed printer block
is complete; protected runtime checks and preparation limits are consolidated
in [the assessment decision](MIPS_TS_PLAN.md#assessment-decision-and-coverage).

## Specialized workflows

- [RC610 Printer](RC610.md): hardware/guest contract, live monitor inventory,
  maintained monitors, append-mode output and guest/PRIMO acceptance gates.
- [Manual MIPS/TS Manufacturing](../MIPS-TS.md): complete host/guest procedure
  for preparing a private disk pair, clean bases and two-boot acceptance;
  separate from the User Manual's finished-environment operating guide.
- [MIPS/TS Plan](MIPS_TS_PLAN.md): isolated TEM/PRIMO/SOS bring-up, user-catalog
  prerequisites, terminal verification, separate protection assessment and
  private protected-candidate acceptance. Candidate commands are in
  [Development](DEVELOPMENT.md#experimental-protected-mipsts-candidate).
- [Protection and Monitor Plan](PROTECTION_PLAN.md): staged optional protection
  and V2 integration, stable checkpoint, acceptance gates and recovery.
- [Disk Workflow](DISK_WORKFLOW.md): fixture identities, verified working copies,
  image regeneration, candidate promotion and recovery.
- [Binary Distribution](BINARY_DISTRIBUTION.md): explicit archive target,
  runtime manifest, packaging checks and license boundaries.
- [Manual Evidence](MANUAL_REVIEW.md): documented guest workflows and the tests
  that justify their claims.
- [Provenance and Licensing](PROVENANCE_REVIEW.md): original basis, retained
  attribution, material ownership boundaries and open publication questions.
- [Historical Permission Notice](V_1.2_DB_LICENSE.txt): retained third-party terms.

Each document has one primary responsibility. Keep current commands in
Development, assertion detail in Test Fixtures and completion state in Progress.
Do not duplicate those into chronological plans, private correspondence or
machine-local session reports. Generated output stays in ignored build trees.

Historical author/copyright names are necessary attribution, not contributor
contact records. These documents contain no personal paths or project-
conversation metadata. They do not need the original host source tree to build,
test or continue the native implementation.
