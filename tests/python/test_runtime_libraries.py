#!/usr/bin/env python3
"""Inspect existing Algol libraries without modifying or replacing guest media."""

import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    builder = Path(sys.argv[1]).resolve()
    media = [ROOT / "src/monitor/MONITOR", ROOT / "src/disks/UNIT0"]
    before = {path: digest(path) for path in media}
    with tempfile.TemporaryDirectory(prefix="sim4000-library-inspection-") as temporary:
        work = Path(temporary)
        result = subprocess.run(
            [str(builder), "--inspect-libraries", str(media[0]), str(media[1]), str(work)],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=90)
        assert result.returncode == 0, result.stdout
        old = (work / "oldruntime").read_bytes()
        new = (work / "newruntime").read_bytes()
        assert len(old) == 38400 and len(new) == 52992
        assert hashlib.sha256(old).hexdigest() == \
            "3006c1589ff38b16005ad0ce75c920892857f996b7a24c96cf453fc62969a043"
        assert hashlib.sha256(new).hexdigest() == \
            "c95314f9ec95d97eec7a64d897ae2ef42c486442a17cb9b9ce7ae2634db7d557"

        def label(data, block):
            return data[block * 768 + 756:(block + 1) * 768].rstrip(b"\0")

        assert label(old, 14) == b"tofrom"
        assert label(new, 17) == b"tofrom1"
        assert label(new, 18) == b"tofrom2"
        assert label(new, 29) == b"system0"
        assert old[14 * 768:14 * 768 + 3] == bytes.fromhex("00c00c")
        assert new[17 * 768:17 * 768 + 3] == bytes.fromhex("00e00e")
        # The older module retains the same parameter-entry prologue before
        # the implementation diverges; this alone is not an ABI proof.
        assert old[14 * 768 + 33:14 * 768 + 87] == \
            new[17 * 768 + 36:17 * 768 + 90]
        # Check word alignment, not arbitrary byte-pattern matches in packed data.
        probe = bytes.fromhex("730022 a3004f a3003b")
        old_hits = [i for i in range(0, len(old), 3) if old.startswith(probe, i)]
        new_hits = [i for i in range(0, len(new), 3) if new.startswith(probe, i)]
        assert old_hits == []
        assert new_hits == [0x339c, 0x590d]
        # Installed programs are linked binaries, not dynamically rebound by
        # changing a catalog entry after compilation.
        for filename, size, block, hits in (
                ("frombinary", 42240, 40, [0x500d, 0x789c]),
                ("tobinary", 36096, 39, [0x4d0d, 0x759c])):
            data = (work / filename).read_bytes()
            assert len(data) == size
            assert label(data, block) == b"tofrom1"
            assert label(data, block + 1) == b"tofrom2"
            assert [i for i in range(0, len(data), 3)
                    if data.startswith(probe, i)] == hits
        metadata = (work / "libmetadata").read_text(encoding="ascii")
        assert "tofrom     =set bs algollib 2048.24 83.2665 0 49.16 2.0" in metadata
        assert "system     =set bs oldalgollib 2048.52 233.1235 0 57.38 3.0" in metadata
        assert "nsystem    =set bs algollib 2048.46 233.1235 0 61.32 3.0" in metadata
    assert {path: digest(path) for path in media} == before
    print("Algol library identities, module labels and GG/PL probe sequences verified")
    print("Installed transfer binaries contain linked copies of the newer modules")


if __name__ == "__main__":
    main()
