#!/usr/bin/env python3
"""Test the narrow monitor tape reader without downloading archive fixtures."""

from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from inspect_monitor_tape import TapeError, extract_dump, listing_lines, tape_files


def be24(value: int) -> bytes:
    return value.to_bytes(3, "big")


def wrapped(data: bytes) -> bytes:
    return (struct.pack("<I", len(data)) + data + b"\0" * (len(data) % 2) +
            struct.pack("<I", len(data)))


def dump() -> list[bytes]:
    label = b"dump  " + bytes(144)
    entry = b"".join(be24(i) for i in (1, 48, 1, 1)) + b"sample" + bytes(132)
    segment = b"".join(be24(i) for i in (2, 520, 1, 0)) + b"payload" + bytes(761)
    end = b"".join(be24(i) for i in (3, 8, 1, 1)) + bytes(138)
    return [label, entry, segment, end]


class TapeTests(unittest.TestCase):
    def test_wrappers(self) -> None:
        image = wrapped(b"odd") + bytes(4) + wrapped(b"even") + bytes(4)
        self.assertEqual(tape_files(image), [[b"odd"], [b"even"]])
        for malformed in (image[:-1], wrapped(b"odd")[:-1],
                          wrapped(b"odd")[:-4] + struct.pack("<I", 8),
                          struct.pack("<I", 0x80000001), wrapped(b"unmarked")):
            with self.assertRaises(TapeError):
                tape_files(malformed)

    def test_complete_dump(self) -> None:
        records = dump()
        result = extract_dump(records)
        self.assertEqual(result, {"sample": b"payload" + bytes(761)})
        for malformed in (records[:-1], [records[0], records[1], records[-1]],
                          records + [records[-1]],
                          [records[0], records[1], records[2][:9] + be24(1) +
                           records[2][12:], records[-1]]):
            with self.assertRaises(TapeError):
                extract_dump(malformed)

    def test_listing_derivative(self) -> None:
        data = (b"    1     0  b.a800,b200 w.\n"
                b"unlisted diagnostic\n"
                b"    2    14       rl w0 0 ; comment\n"
                b"  a1 def 1\n\x19padding")
        self.assertEqual(listing_lines(data),
                         ["b.a800,b200 w.", "     rl w0 0 ; comment"])


if __name__ == "__main__":
    unittest.main()
