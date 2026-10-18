#!/usr/bin/env python3
"""Localize the installed SOS catalog updater's protected preparation failure."""

import argparse
import json
from pathlib import Path
import re
import subprocess

from probe_mips_protection import digest
from probe_rc4000_runtime import PROBE, labels

ROOT = Path(__file__).resolve().parents[2]
INPUT = b"maxp 4\n"


def verify_evidence(work):
    transcript = (work / "session.log").read_text(encoding="ascii")
    for marker in ("Candidate verification: RC4000 protection enabled",
                   "UPSOSCAT probe: task PL address=34 IRQ0 saved-next=yes PR=bf key=1",
                   "UPSOSCAT known incompatibility captured; FP recovered, not a catalog build",
                   "pause hostcheck break fp", "***fp reinitialized"):
        assert transcript.count(marker) == 1, marker
    faults = re.findall(
        r"(?m)^IRQ0 pc=([0-9a-f]{6}) raw=([0-9a-f]{6}) saved=([0-9a-f]{6}) "
        r"ea=([0-9a-f]{6}) PR=([0-9a-f]{2}) key=(\d+) monitor=(\d+) W=([^\n]+)$",
        transcript)
    # IRQ 0 also implements ordinary guest monitor calls; identify the actual
    # task PL, not every legitimate JD/service entry in the diagnostic ring.
    faults = [fault for fault in faults if fault[1] == "730022"]
    assert len(faults) == 1, faults
    pc, raw, saved, effective, pr, key, monitor, registers = faults[0]
    assert (raw, effective, pr, key, monitor) == ("730022", "000022", "bf", "1", "0"), faults
    assert int(saved, 16) == int(pc, 16) + 2, faults
    assert len(re.findall(r"break\s+0\s+tofrom1", transcript)) == 1, transcript
    assert len(re.findall(r"called from\s+line 1094-1099", transcript)) == 1, transcript
    assert re.findall(r"(?m)^\*\*\*[^\n]*", transcript) == ["***fp reinitialized"], transcript
    binary = (work / "upbinary").read_bytes()
    metadata = (work / "upmetadata").read_text(encoding="ascii")
    assert len(binary) == 114 * 768, len(binary)
    assert re.search(r"(?m)^upsoscat\s+=set 114 disc .*; system$", metadata), metadata
    names = labels(binary)
    hits = [offset for offset in range(0, len(binary), 3) if binary.startswith(PROBE, offset)]
    assert names.count(b"tofrom1") == names.count(b"tofrom2") == 1, names
    assert any(names[offset // 768] == b"tofrom1" for offset in hits), hits
    assert (work / "input").read_bytes() == (work / "inputafter").read_bytes() == INPUT
    return {"fault_pc": int(pc, 16), "saved_next_ic": int(saved, 16),
            "raw_instruction": raw, "instruction": "pl w3 34", "effective_address": 34,
            "pr": int(pr, 16), "code_key": int(key), "task_mode": True,
            "registers_hex": registers, "linked_probe_offsets": hits,
            "updater_sha256": digest(work / "upbinary")}


def probe(builder, destination):
    monitor = ROOT / "src/monitor/MONITOR.protected"
    disk = ROOT / "src/disks/UNIT0.protected"
    sources = (monitor, disk, ROOT / "src/config/RC4000.protected.cfg")
    before = {str(path.relative_to(ROOT)): digest(path) for path in sources}
    destination.mkdir()  # Never overwrite runtime files or previous evidence.
    (destination / "input").write_bytes(INPUT)
    try:
        with (destination / "session.log").open("xb") as log:
            result = subprocess.run([str(builder), "--probe-protected-upsoscat", str(monitor),
                                     str(disk), str(destination)], stdout=log,
                                    stderr=subprocess.STDOUT, timeout=120)
        assert result.returncode == 0, f"inspect {destination / 'session.log'}"
        observation = verify_evidence(destination)
    finally:
        assert {str(path.relative_to(ROOT)): digest(path) for path in sources} == before
    (destination / "result.json").write_text(json.dumps({
        "assessment": "known-protected-updater-incompatibility", "catalog_build_succeeded": False,
        "protection": "rc4000", "observation": observation, "fp_recovered": True,
        "input_preserved": True, "source_sha256": before,
        "storage": "volatile overlay; source disk unchanged", "binary_patched": False,
    }, indent=2) + "\n", encoding="ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("builder", type=Path)
    parser.add_argument("destination", type=Path, help="new exclusive evidence workspace")
    args = parser.parse_args()
    probe(args.builder.resolve(), args.destination.resolve())
    print("Protected upsoscat failure localized; FP recovered, no catalog build or fix claimed.")


if __name__ == "__main__":
    main()
