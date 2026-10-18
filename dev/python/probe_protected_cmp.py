#!/usr/bin/env python3
"""Localize the installed comparator's known protected-profile incompatibility."""

import argparse
import hashlib
from pathlib import Path
import re
import subprocess

from make_unit0_copy import CopyError
from probe_rc4000_runtime import PROBE, labels

ROOT = Path(__file__).resolve().parents[2]
TEXT = b"Monitor manual sample.\nSecond line.\n"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_evidence(work):
    transcript = (work / "session.log").read_text(encoding="ascii")
    for marker in (
            "Candidate verification: RC4000 protection enabled",
            "CMP probe: task PL address=34 IRQ0 saved-next=yes PR=bf key=1",
            "CMP known incompatibility localized; FP recovered, not a passing comparison"):
        if transcript.count(marker) != 1:
            raise CopyError(f"missing/duplicate comparator observation: {marker}")
    faults = re.findall(
        r"IRQ0 pc=([0-9a-f]{6}) raw=730022 saved=([0-9a-f]{6}) "
        r"ea=000022 PR=bf key=1 monitor=0 W=", transcript)
    if len(faults) != 1 or int(faults[0][1], 16) != int(faults[0][0], 16) + 2:
        raise CopyError("comparator task PL/IRQ-0 saved-PC evidence differs")
    if not re.search(r"break\s+0\s+tofrom1", transcript) or \
            transcript.count("pause hostcheck break fp") != 1 or \
            transcript.count("***fp reinitialized") != 1 or \
            "files are identical" in transcript or "DISCREPANCY NO." in transcript:
        raise CopyError("comparator break/reinitialization differs or comparison unexpectedly passed")
    if re.findall(r"(?m)^\*\*\*[^\n]*", transcript) != ["***fp reinitialized"]:
        raise CopyError("unexpected FP diagnostics")
    binary = (work / "cmpbinary").read_bytes()
    if len(binary) != 80 * 768:
        raise CopyError("unexpected comparator binary allocation")
    metadata = (work / "cmpmetadata").read_text(encoding="ascii")
    if not re.search(r"(?m)^cmp\s+=set 80 disc .*; system$", metadata):
        raise CopyError("unexpected comparator catalog descriptor")
    names = labels(binary)
    hits = [i for i in range(0, len(binary), 3) if binary.startswith(PROBE, i)]
    if names.count(b"tofrom1") != 1 or names.count(b"tofrom2") != 1 or \
            not any(names[i // 768] == b"tofrom1" for i in hits):
        raise CopyError("installed comparator does not contain the expected linked tofrom1 probe")
    for name in ("input", "origafter", "copyafter"):
        if (work / name).read_bytes() != TEXT:
            raise CopyError(f"comparator input/recovered export differs: {name}")
    return hits


def probe(builder, work):
    monitor = ROOT / "src/monitor/MONITOR.protected"
    disk = ROOT / "src/disks/UNIT0.protected"
    sources = {monitor, disk, ROOT / "src/monitor/MONITOR", ROOT / "src/disks/UNIT0",
               ROOT / "src/disks/UNIT0.original"}
    before = {path: digest(path) for path in sources}
    work.mkdir()  # Preserve existing work and failed investigation evidence.
    (work / "input").write_bytes(TEXT)
    try:
        with (work / "session.log").open("xb") as log:
            result = subprocess.run(
                [str(builder), "--probe-protected-cmp", str(monitor), str(disk), str(work)],
                stdout=log, stderr=subprocess.STDOUT, timeout=120)
        if result.returncode != 0:
            raise CopyError(f"comparator probe failed; inspect {work / 'session.log'}")
        hits = verify_evidence(work)
        if {path: digest(path) for path in sources} != before:
            raise CopyError("source media changed")
        (work / "result.txt").write_text(
            "Known protected cmp incompatibility localized, not a passing comparison.\n"
            "Task-mode PL of address 34 raises IRQ 0; saved PC advances by two.\n"
            "Installed binary contains linked newer tofrom1/tofrom2; no runtime rebinding.\n"
            f"Aligned probe byte offsets: {','.join(hex(i) for i in hits)}\n"
            "FP reinitialization preserves both comparison inputs. No CPU/library/image patch.\n"
            f"monitor-sha256={digest(monitor)}\ndisk-sha256={digest(disk)}\n"
            f"cmp-sha256={digest(work / 'cmpbinary')}\n", encoding="ascii")
    finally:
        if {path: digest(path) for path in sources} != before:
            raise CopyError("source media changed during comparator investigation")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("builder", type=Path)
    parser.add_argument("destination", type=Path, help="new exclusive evidence directory")
    args = parser.parse_args()
    probe(args.builder.resolve(), args.destination.resolve())
    print("Known protected cmp incompatibility localized; no compatibility fix applied")


if __name__ == "__main__":
    main()
