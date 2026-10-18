#!/usr/bin/env python3
"""Reproduce the monitor from local sources and verify a candidate boot."""

import hashlib
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from rebuild_monitor import listing_succeeded, verify_guest_protection_edit


def digest(path: Path) -> bytes:
    return hashlib.sha256(path.read_bytes()).digest()


def verify_build(work: Path, protected: bool = False) -> None:
    reference = ROOT / "src/monitor" / ("MONITOR.protected" if protected else "MONITOR")
    assert (work / "candidate").read_bytes() == reference.read_bytes()
    assert listing_succeeded((work / "buildlist").read_bytes())
    assert len((work / "sources.txt").read_text().splitlines()) == (15 if protected else 14)
    assert (work / "boot.log").is_file()
    if protected:
        assert (work / "montxt10").read_bytes() == (ROOT / "monsrc/montxt10").read_bytes()
        verify_guest_protection_edit((work / "montxt10").read_bytes(),
                                     (work / "pkedited").read_bytes(),
                                     (work / "pkrestored").read_bytes())
        assert b"edit end." in (work / "pkeditlog").read_bytes()
        assert "source-transform=guest-editor-e22-v1" in (work / "profile.txt").read_text()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("builder", type=Path)
    parser.add_argument("--protected", action="store_true")
    parser.add_argument("--printer-test", type=Path)
    args = parser.parse_args()
    sources = [path for path in (ROOT / "monsrc").iterdir() if path.name != "README.md"]
    for source in sources:
        assert b"\r" not in source.read_bytes(), source
        if source.name != "README.md":
            assert source.name == source.name.lower(), source
    fixtures = [ROOT / "src/monitor/MONITOR", ROOT / "src/disks/UNIT0",
                ROOT / "src/disks/UNIT0.original", ROOT / "src/monitor/MONITOR.protected",
                ROOT / "src/disks/UNIT0.protected"] + sources
    before = {path: digest(path) for path in fixtures}
    with tempfile.TemporaryDirectory(prefix="sim4000-monitor-rebuild-") as temporary:
        work = Path(temporary) / "build"
        subprocess.run([sys.executable, str(ROOT / "dev/python/rebuild_monitor.py"),
                        str(work), "--builder", str(args.builder.resolve()),
                        "--verify-boot"] + (["--protected"] if args.protected else []),
                       check=True, timeout=420)
        verify_build(work, args.protected)
        if args.printer_test:
            subprocess.run([str(args.printer_test.resolve())] +
                           (["--protected"] if args.protected else []) +
                           [str(work / "candidate"),
                            str(ROOT / "src/disks" / ("UNIT0.protected" if args.protected else "UNIT0")),
                            str(work / "printer-check.txt")], check=True, timeout=60)
        assert digest(work / "UNIT0") == before[ROOT / "src/disks/UNIT0"]
    assert {path: digest(path) for path in fixtures} == before
    print("Local monitor sources reproduced the shipped binary; candidate boot verified")


if __name__ == "__main__":
    main()
