#!/usr/bin/env python3
"""Prepare separate maintained-monitor runtime profiles from verified artifacts."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

from make_unit0_copy import CopyError, create_copy, verify_source

ROOT = Path(__file__).resolve().parents[2]
MONITOR_SHA256 = "dff975962b89bf87d5dee703701777ad6274c53f550120f9ed9bf42568e42a37"
PROTECTED_SHA256 = "9055b2f5aeaa9a5bc4e7f57a8bd5dd38cca6aceb1c5c98eb4deeeecd936cda62"
PROFILE_SHA256 = "ebc56434088ac0d0a5655924646a45bfd3d739bbb2217f45622c9626641f2483"


def verified_bytes(path, size, digest):
    data = path.read_bytes()
    if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
        raise CopyError(f"unrecognized profile artifact: {path}")
    return data


def configuration(profile):
    if profile not in ("compat", "protected"):
        raise CopyError("unknown monitor profile")
    mode = "RC4000" if profile == "protected" else "OFF"
    suffix = ".protected" if profile == "protected" else ""
    return (
        "; Maintained monitor profile; paths are relative to this configuration.\n"
        f"PROTECTION MODE={mode}\nCPU IX=YES\nCORESIZE=0,1048576\n"
        f"MONITOR FILE=MONITOR{suffix}\n"
        "OPERATOR ESCAPE=CTRL-E\nOPERATOR DATE=ALT-D\nINPUT DEL=CTRL-H\n"
        "; Translate current dates to the monitor's supported historical years.\n"
        "DATE YEAROFFSET=-40\nTHROTTLE VALUE=50%\n"
        "DEFINE CONSOLE DEVNO=2 INTNO=6,3 PROTOCOL=CLASSIC\n"
        "DEFINE CLOCK DEVNO=3 INTNO=11\n"
        f"DEFINE RC610 DEVNO=7 INTNO=10 FILE=printer{suffix}.txt APPEND=YES\n"
        "DEFINE CONSOLE DEVNO=4 INTNO=20,19 PROTOCOL=CLASSIC\n"
        "; Optional independent remote console; enable only when needed.\n"
        "; TELNET DEVNO=4 ADDRESS=127.0.0.1 PORT=4004 ATTENTION=CTRL-A\n"
        "; Base disk is read-only; writes are volatile unless an overlay is set.\n"
        f"DEFINE RC433X DEVNO=15 FILE=UNIT0{suffix}\n"
        f"; Append OVERLAY=UNIT0{suffix}.overlay to retain guest disk writes.\n"
        "DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT=exchange\n"
    )


def prepare(destination, candidate, image):
    # Validate all inputs before creating anything. No binary-only monitor is
    # accepted, and no implicit promotion of a newly generated candidate occurs.
    original = verified_bytes(ROOT / "src/monitor/MONITOR", 38400, MONITOR_SHA256)
    protected = verified_bytes(candidate, 38400, PROTECTED_SHA256)
    disk = verified_bytes(image, 4660224, PROFILE_SHA256)
    source = ROOT / "src/disks/UNIT0"
    _, source_digest = verify_source(source, allow_native=True)
    destination.mkdir()  # Refuse existing work, including earlier partial runs.
    (destination / "exchange").mkdir()
    artifacts = {}
    for profile, monitor in (("compat", original), ("protected", protected)):
        suffix = ".protected" if profile == "protected" else ""
        (destination / f"MONITOR{suffix}").write_bytes(monitor)
        if profile == "compat":
            create_copy(source, destination / "UNIT0", allow_native=True)
        else:
            (destination / "UNIT0.protected").write_bytes(disk)
        (destination / f"RC4000.{profile}.cfg").write_text(
            configuration(profile), encoding="ascii")
        for name in ("MONITOR", "UNIT0"):
            path = destination / (name + suffix)
            artifacts[name + suffix] = hashlib.sha256(path.read_bytes()).hexdigest()
    if verify_source(source, allow_native=True)[1] != source_digest:
        raise CopyError("source disk changed during profile preparation")
    (destination / "README.txt").write_text(
        "Maintained monitor profiles\n\n"
        "Run the existing sim4000 executable with either configuration:\n"
        "  sim4000 -a -c RC4000.compat.cfg\n"
        "  sim4000 -a -c RC4000.protected.cfg\n\n"
        "No Python is needed to run these prepared profiles. The executable,\n"
        "manual and license remain part of the normal emulator distribution;\n"
        "this directory is a developer-prepared workspace, not a distribution.\n"
        "The profile files can be moved together; paths are configuration-relative.\n"
        "Both use CLASSIC terminals and IX. No V2 monitor is included.\n\n"
        "Compatibility uses the unchanged monitor and native disk. Protection\n"
        "uses the separately rebuilt monitor and verified older-tofrom profile.\n"
        "In protected FP sessions use rcin and rcout instead of fromhost/tohost:\n"
        "  sample=rcin input\n"
        "  output=rcout sample\n"
        "Place host input in the shared exchange directory.\n"
        "The original fromhost/tohost remain installed but are not compatible\n"
        "with strict RC4000 protection. Untested installed programs may also\n"
        "use newer-machine instructions; this is not a full RC8000 profile.\n\n"
        "Disk writes are volatile by default. To retain them, append the\n"
        "commented OVERLAY option to that profile's DEFINE RC433X line. Never\n"
        "share an overlay across profiles. Host exports persist independently.\n"
        "To return to compatibility, select its configuration; do not substitute\n"
        "one profile's disk or monitor into the other. No shipped file is replaced.\n",
        encoding="ascii")
    (destination / "manifest.json").write_text(
        json.dumps({"profile_format": 1, "artifacts": artifacts,
                    "source_rebuild": "guarded restore-e22-v1",
                    "protected_software": "derived-system-tofrom-rcin-rcout",
                    "promotion": False}, indent=2, sort_keys=True) + "\n",
        encoding="ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new workspace; must not exist")
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--image", required=True, type=Path,
                        help="verified M3 system-binding disk, not the default UNIT0")
    args = parser.parse_args()
    try:
        prepare(args.destination.resolve(), args.candidate.resolve(), args.image.resolve())
    except (CopyError, OSError) as error:
        print(f"profiles not prepared: {error}", file=sys.stderr)
        return 1
    print(f"Separate monitor profiles prepared: {args.destination}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
