#!/usr/bin/env python3
"""Bound the catstat failure without assuming its cause or changing media."""

import hashlib
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main() -> None:
    builder = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[2]
    monitor = root / "src/monitor/MONITOR"
    disk = root / "src/disks/UNIT0"
    fixtures = [monitor, disk, root / "src/disks/UNIT0.original"]
    variants = [("--verify-catstat", monitor)]
    if len(sys.argv) > 2:
        standard = Path(sys.argv[2]).resolve()
        fixtures.append(standard)
        variants.append(("--verify-catstat-dfc", standard))
    before = [hashlib.sha256(path.read_bytes()).digest() for path in fixtures]

    with tempfile.TemporaryDirectory(prefix="sim4000-catstat-") as temporary:
        for mode, boot in variants:
            directory = Path(temporary) / mode.removeprefix("--")
            directory.mkdir()
            result = subprocess.run(
                [str(builder), mode, str(boot), str(disk), str(directory)],
                capture_output=True, text=True, encoding="ascii", timeout=30)
            assert result.returncode == 0, result.stdout + result.stderr
            assert result.stderr.count(
                "catstat catalog lookup: result=0 segments=72 "
                "contents=4095 entry=4095\n") == 1, result.stderr
            assert result.stderr.count(
                "catstat first unassigned: opcode=61 word=f52fed "
                "extension=004001 interrupt=0\n") == 1, result.stderr
            assert result.stderr.count(
                "catstat index descriptor: index=1 lower=0 upper=40 "
                "target-is-tail=1\n") == 1, result.stderr
            assert result.stderr.index("catstat catalog lookup:") < result.stderr.index(
                "catstat first unassigned:"), result.stderr
            assert len(re.findall(r"break\s+0\s+line\s+18-19", result.stdout)) == 1
            assert result.stdout.count("pause muser break fp") == 1
            assert result.stdout.count("***fp reinitialized") == 1
            assert result.stdout.index("pause muser break fp") < result.stdout.index(
                "proc muser start") < result.stdout.index("***fp reinitialized")
            assert "att \x07s" in result.stdout
            assert "syntax error:" not in result.stdout
            exported = (directory / "catraw").read_bytes()
            assert len(exported) == 72 * 768
            assert (directory / "again").read_bytes() == exported
            assert {path.name for path in directory.iterdir()} == {"catraw", "again"}

    assert [hashlib.sha256(path.read_bytes()).digest() for path in fixtures] == before
    print("Catstat lookup, opcode-61 exception, FP recovery, and catalog integrity passed")


if __name__ == "__main__":
    main()
