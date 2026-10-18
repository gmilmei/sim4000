#!/usr/bin/env python3
"""Opt-in two-boot Telnet acceptance for the shipped flat MIPS/TS profile."""

import argparse
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from prepare_mips_test import digest, verify_profile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="exclusive evidence workspace")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    parser.add_argument("--runtime-root", type=Path,
                        help="optional extracted binary distribution to test")
    args = parser.parse_args()
    names = {"MONITOR": ROOT / "src/monitor/MONITOR",
             "UNIT0.mipsts": ROOT / "src/disks/UNIT0.mipsts",
             "UNIT1.mipsts": ROOT / "src/disks/UNIT1.mipsts",
             "RC4000.mipsts.cfg": ROOT / "src/config/RC4000.mipsts.cfg"}
    if args.runtime_root:
        names = {name: args.runtime_root.resolve() / name for name in names}
    before = {name: digest(path) for name, path in names.items()}
    destination = args.destination.resolve()
    destination.mkdir()  # Never reuse an active profile or existing evidence.
    (destination / "exchange").mkdir()
    for name, path in names.items():
        shutil.copyfile(path, destination / name)
    simulator = (args.runtime_root / "sim4000" if args.runtime_root else args.simulator).resolve()
    try:
        verify_profile(simulator, destination, 4004, "UNIT0.mipsts", "UNIT1.mipsts")
    finally:
        assert {name: digest(path) for name, path in names.items()} == before
    print("Flat MIPS/TS profile: two boots, TEM login/logout and persistence passed.")


if __name__ == "__main__":
    main()
