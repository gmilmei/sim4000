#!/usr/bin/env python3
"""Prepare clean MIPS/TS distribution candidates, never overwrite shipped media."""

import argparse
import json
from pathlib import Path
import shutil

from prepare_mips_test import ROOT, prepare, digest


def build(builder, simulator, destination):
    destination.mkdir()  # Exclusive destination, including after failed attempts.
    work = destination / "preparation"
    prepare(builder, simulator, work, 4004)
    # Promote only prepared bases, never verification overlays or later files.
    for original, name in (("UNIT0", "UNIT0.mipsts"), ("UNIT1", "UNIT1.mipsts")):
        shutil.copyfile(work / original, destination / name)
    shutil.copyfile(ROOT / "src/config/RC4000.mipsts.cfg",
                    destination / "RC4000.mipsts.cfg")
    (destination / "manifest.json").write_text(json.dumps({
        "profile": "mipsts", "protection": "off", "dummy_account": "test",
        "fixed_boot_date": "1986-09-30", "source_sha256": {
            name: digest(ROOT / path) for name, path in (
                ("UNIT0", "src/disks/UNIT0"), ("MONITOR", "src/monitor/MONITOR"))},
        "artifacts": {name: digest(destination / name) for name in (
            "UNIT0.mipsts", "UNIT1.mipsts", "RC4000.mipsts.cfg")},
    }, indent=2) + "\n", encoding="ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new candidate directory")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    parser.add_argument("--builder", type=Path,
                        default=ROOT / "build-protection/sim4000_build_host_image")
    args = parser.parse_args()
    build(args.builder.resolve(), args.simulator.resolve(), args.destination.resolve())
    print("Clean MIPS/TS candidates prepared; review and verify before promotion.")


if __name__ == "__main__":
    main()
