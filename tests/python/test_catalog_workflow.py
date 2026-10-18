#!/usr/bin/env python3
"""Verify guest file scope and reboot persistence on exclusive working copies."""

import hashlib
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def digest(path: Path) -> bytes:
    return hashlib.sha256(path.read_bytes()).digest()


def main() -> None:
    builder = Path(sys.argv[1]).resolve()
    simulator = Path(sys.argv[2]).resolve()
    root = Path(__file__).resolve().parents[2]
    monitor = root / "src/monitor/MONITOR"
    native = root / "src/disks/UNIT0"
    original = root / "src/disks/UNIT0.original"
    before = (digest(native), digest(original))
    sample = b"Saved catalog sample.\nSecond line.\n"

    def catalog(image: Path, main: bool = False) -> dict[str, list[str]]:
        text = subprocess.check_output(
            [str(simulator), "--list-main-catalog" if main else "--list-catalog",
             str(image)], text=True)
        return {fields[2]: fields for line in text.splitlines()[2:]
                if (fields := line.split())}

    with tempfile.TemporaryDirectory(prefix="sim4000-catalog-") as temporary:
        directory = Path(temporary)
        installed = set(catalog(native))
        main_entries = catalog(native, main=True)
        assert set(main_entries) == installed | {"c", "v", "catdisc"}
        assert main_entries["slang"][10] == "disc", main_entries["slang"]
        assert catalog(original, main=True)["fromdos"][10] == "disc"

        def check_invalid_main_catalogs() -> None:
            pristine = native.read_bytes()
            descriptor = catalog(native)["catalog"]
            # Fixture device-catalog segments occupy sectors 6 through 41.
            offset = (6 + int(descriptor[0])) * 768 + int(descriptor[1]) * 51
            first_slice = int(descriptor[5])
            assert first_slice == 964
            assert int.from_bytes(pristine[42:45], "little") & 4095 == 0
            invalid = directory / "invalid-main.img"

            def word(data: bytearray, position: int, value: int) -> None:
                data[position:position + 3] = value.to_bytes(3, "little")

            def link(data: bytearray, slice_number: int, value: int) -> None:
                halfword = 34 + slice_number
                position = (halfword // 2) * 3
                old = int.from_bytes(data[position:position + 3], "little")
                shift = 0 if halfword % 2 else 12
                word(data, position, (old & ~(4095 << shift)) | (value << shift))

            cases = []
            for label, value in [("zero size", 0), ("negative size", 0xffffff),
                                 ("too many segments", 6069),
                                 ("premature area end", 73)]:
                data = bytearray(pristine)
                word(data, offset + 7 * 3, value)
                cases.append((label, data))
            data = bytearray(pristine)
            data[offset + 3 * 3:offset + 7 * 3] = bytes(12)
            cases.append(("missing main catalog", data))
            data = bytearray(pristine)
            word(data, offset + 8 * 3, int.from_bytes(b"bad", "big"))
            cases.append(("other document", data))
            data = bytearray(pristine)
            old = int.from_bytes(data[offset:offset + 3], "little")
            word(data, offset, (4095 << 12) | (old & 4095))
            cases.append(("out of range first slice", data))
            for label, slice_number, value in [
                    ("premature link", first_slice, 0),
                    ("out of range link", first_slice, 2048),
                    ("cycle", first_slice + 1, 4095)]:
                data = bytearray(pristine)
                link(data, slice_number, value)
                cases.append((label, data))
            for label, data in cases:
                invalid.write_bytes(data)
                unchanged = digest(invalid)
                result = subprocess.run(
                    [str(simulator), "--list-main-catalog", str(invalid)],
                    capture_output=True, text=True, timeout=10)
                assert result.returncode != 0, (label, result.stdout)
                assert result.stderr and not result.stdout, (label, result)
                assert digest(invalid) == unchanged, label

        check_invalid_main_catalogs()

        def check_listing(host: Path) -> None:
            text = (host / "listout").read_text(encoding="ascii")
            # Eleven-character names can touch '=' without a separating space.
            names = re.findall(r"(?m)^([a-z][a-z0-9]*)\s*=", text)
            assert len(names) == len(set(names)), names
            assert installed <= set(names), sorted(installed - set(names))
            assert {"saved", "gone"} <= set(names)
            assert re.search(r"(?m)^saved\s*=.*; user$", text), text
            assert re.search(r"(?m)^gone\s*=.*; temp$", text), text
            assert re.search(r"(?m)^algol\s*=.*; system$", text), text
            assert re.search(r"(?m)^lookup\s*=.*; system\.disc$", text), text

        def session(mode: str, image: Path, label: str,
                    success: bool = True) -> tuple[str, Path]:
            host = directory / label
            host.mkdir()
            (host / "input").write_bytes(sample)
            result = subprocess.run(
                [str(builder), mode, str(monitor), str(image), str(host)],
                capture_output=True, text=True, timeout=120)
            print(result.stdout, end="")
            if success:
                assert result.returncode == 0, result.stderr
            else:
                assert result.returncode != 0, result.stdout
            return result.stdout, host

        # Guest permanence alone cannot preserve a volatile host overlay.
        def check_sorted_scopes(text: str) -> None:
            def reports(command: str) -> list[str]:
                return [part.split("\x07", 1)[0]
                        for part in text.split(f"\x07{command}\n")[1:]]

            def names(report: str) -> list[str]:
                return re.findall(r"(?m)^(\w+)[ \t]+(?:tw|\d+)[ \t]+\w+[ \t]+", report)

            temp_before, temp_after = reports("catsort scope.temp")
            default, = reports("catsort")
            user, = reports("catsort scope.user")
            assert names(temp_before) == ["c", "gone", "saved", "v"], temp_before
            assert names(temp_after) == ["c", "gone", "v"], temp_after
            for report, segments, entries in ((temp_before, 72, 4), (temp_after, 36, 3),
                                               (default, 72, 4), (user, 72, 4)):
                assert re.search(rf"total:\s+{segments} segm\.\s+{entries} entr\.", report), report
                assert "***" not in report, report
            assert "base: 8388605 8388605 temp" in temp_before, temp_before
            assert "base: 8388605 8388605 temp" in temp_after, temp_after
            for report, label in ((default, "project"), (user, "perm")):
                assert names(report) == ["saved", "c", "gone", "v"], report
                retained, temporary = report.split("base: 8388605 8388605 temp", 1)
                assert f"base: 8388605 8388605 {label}" in retained, retained
                assert names(retained) == ["saved"], retained
                assert names(temporary) == ["c", "gone", "v"], temporary
                assert re.search(r"(?m)^saved\s+36 disc\s", retained), retained
                assert re.search(r"disc\s+:\s+24 slices \*\s+3 =\s+72 segments\s+2 entries",
                                 report), report

        text, host = session("--verify-catalog", native, "volatile")
        check_sorted_scopes(text)
        assert (host / "afterproc").read_bytes() == sample
        assert "; temp" in text and "; user" in text
        assert "indgange ialt:" in text and "frie indgange:" in text
        assert re.search(r"antal segmenter:\s+72", text), text
        assert re.search(r"indgange ialt:\s+1080", text), text
        raw = (host / "catraw").read_bytes()
        assert len(raw) == 72 * 768
        occupied = 0
        for segment in range(72):
            for slot in range(15):
                position = (segment * 256 + slot * 17) * 3
                if raw[position:position + 3] != b"\xff\xff\xff":
                    occupied += 1
        free = 1080 - occupied
        assert re.search(rf"frie indgange:\s+{free}\b", text), (free, text)
        check_listing(host)
        details = (host / "detailsout").read_text(encoding="ascii")
        assert re.search(r"(?m)^saved\s+=set .*; user$", details), details
        assert re.search(r"(?m)^algol\s+=set ", details), details
        assert re.search(r"(?m)^slang\s+=set ", details), details
        assert "saved" not in catalog(native)
        text, _ = session("--verify-catalog-read", native, "lost", success=False)
        assert re.search(r"saved\s+unknown", text), text

        # Create exclusively and verify provenance before any writable session.
        working = directory / "UNIT0.work"
        subprocess.run([sys.executable, str(root / "dev/python/make_unit0_copy.py"),
                        str(working)], check=True, timeout=30)
        text, host = session("--catalog-create", working, "create")
        check_sorted_scopes(text)
        assert (host / "afterproc").read_bytes() == sample
        check_listing(host)
        entries = catalog(working)
        main_entries = catalog(working, main=True)
        assert len(main_entries) == 419
        saved = entries["saved"]
        assert saved[7:10] == ["3", "7ffffd", "7ffffd"], saved
        assert "gone" not in entries
        assert "saved" in main_entries, main_entries
        assert main_entries["saved"][7:10] == saved[7:10]
        # The stored main area can retain obsolete temporary descriptors even
        # though the device catalog has reclaimed the corresponding file.
        assert "gone" in main_entries and "gone" not in entries
        assert digest(working) != before[0]

        # A separate process constructs a fresh machine, not just a new FP.
        committed = digest(working)
        _, host = session("--verify-catalog-read", working, "reboot")
        assert (host / "rebooted").read_bytes() == sample
        assert digest(working) == committed

        # Demoting the saved entry allows normal process removal to reclaim it.
        _, _ = session("--catalog-cleanup", working, "cleanup")
        assert "saved" not in catalog(working)

    assert (digest(native), digest(original)) == before
    print("Device/main catalogs, malformed main areas, statistics, scopes, reboot persistence, and cleanup passed")


if __name__ == "__main__":
    main()
