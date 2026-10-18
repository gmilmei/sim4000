#!/usr/bin/env python3
"""Extract the verified Datamuseum 30003039 monitor dump and compare listings.

This deliberately handles only the first, complete monitor dump in this
artifact, not damaged later dumps or arbitrary RC backup formats. Record
layout is cross-checked against AutoArchaeologist's rc489k_tape_dump.py.
"""

from __future__ import annotations

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
SIZE = 2_055_818
SHA256 = "262a4ceeccbf92b023313a1e7050117a72be3385557528b034db1d143a64d82c"
SEGMENTS = {"mon8part1": 430, "mon8part2": 278, "mon8part3": 535,
            "mon8part4": 495, "mon8part5": 436, "mon8part6": 364, "mon8list": 1}


class TapeError(ValueError):
    """The container or monitor dump is incomplete or inconsistent."""


def tape_files(data: bytes) -> list[list[bytes]]:
    """Read SIMH record wrappers, validating lengths and trailing markers."""
    files: list[list[bytes]] = []
    records: list[bytes] = []
    offset = 0
    while offset < len(data):
        if len(data) - offset < 4:
            raise TapeError("truncated SIMH record marker")
        size = struct.unpack_from("<I", data, offset)[0]
        offset += 4
        if size == 0:
            files.append(records)
            records = []
            continue
        if size == 0xffffffff:
            if records:
                files.append(records)
            if offset != len(data):
                raise TapeError("data follows SIMH end marker")
            return files
        if size & 0xff000000:
            raise TapeError("unsupported SIMH error/metadata marker")
        end = offset + size
        trailer = offset + ((size + 1) & ~1)
        if trailer + 4 > len(data):
            raise TapeError("truncated SIMH record payload")
        if struct.unpack_from("<I", data, trailer)[0] != size:
            raise TapeError("SIMH record length mismatch")
        records.append(data[offset:end])
        offset = trailer + 4
    if records:
        raise TapeError("last tape file has no terminating mark")
    return files


def word(record: bytes, offset: int) -> int:
    if offset + 3 > len(record):
        raise TapeError("truncated 24-bit field")
    return int.from_bytes(record[offset:offset + 3], "big")


def extract_dump(records: list[bytes]) -> dict[str, bytes]:
    """Extract sequential one-segment records from a complete RC dump."""
    if not records or not records[0].startswith(b"dump  ") or len(records[0]) != 150:
        raise TapeError("missing RC dump label")
    entries: dict[str, bytes] = {}
    name = ""
    entry_number = 0
    allocated = 0
    chunks: list[bytes] = []
    ended = False

    def finish() -> None:
        if not name:
            return
        if len(chunks) != allocated:
            raise TapeError(f"incomplete area {name}")
        entries[name] = b"".join(chunks)

    for index, record in enumerate(records[1:], 1):
        kind = word(record, 0)
        if kind == 1:
            finish()
            if len(record) != 150:
                raise TapeError("invalid dump entry record size")
            try:
                name = record[12:24].rstrip(b"\0 ").decode("ascii")
            except UnicodeDecodeError as error:
                raise TapeError("non-ASCII dump name") from error
            if not re.fullmatch(r"[a-z][a-z0-9]*", name) or name in entries:
                raise TapeError("invalid or duplicate area name")
            entry_number = word(record, 6)
            allocated = word(record, 9)
            chunks = []
        elif kind == 2:
            if (not name or len(record) != 780 or word(record, 3) != 520 or
                    word(record, 6) != entry_number or word(record, 9) != len(chunks)):
                raise TapeError("invalid or nonsequential dump segment")
            chunks.append(record[12:])
        elif kind == 3:
            finish()
            if (len(record) != 150 or index != len(records) - 1 or
                    word(record, 6) != len(entries) or
                    word(record, 9) != sum(len(value) // 768 for value in entries.values())):
                raise TapeError("dump end totals or position mismatch")
            ended = True
        else:
            raise TapeError(f"unsupported dump record type {kind}")
    if not ended:
        raise TapeError("missing complete dump end record")
    return entries


def text_bytes(data: bytes) -> bytes:
    """Drop padding only after the guest text's first end-medium character."""
    return data.split(b"\x19", 1)[0].replace(b"\r\n", b"\n").replace(b"\x00", b"")


def listing_lines(data: bytes) -> list[str]:
    """Recover numbered source-looking lines, not an assemblable source file."""
    result = []
    for line in text_bytes(data).decode("ascii").split("\n"):
        match = re.match(r"^\s*\d+\s+\d+  (.*)$", line)
        if match:
            result.append(match.group(1).rstrip())
    return result


def normalized_lines(lines: list[str]) -> list[str]:
    return [" ".join(line.split()) for line in lines if line.strip()]


def inspect(image: Path, destination: Path, sources: Path) -> None:
    data = image.read_bytes()
    if len(data) != SIZE or hashlib.sha256(data).hexdigest() != SHA256:
        raise TapeError("input is not verified Datamuseum artifact 30003039")
    files = tape_files(data)
    if len(files) < 2 or len(files[0]) != 1 or not files[0][0].startswith(b"VOL1"):
        raise TapeError("missing volume label and first dump")
    entries = extract_dump(files[1])
    if {name: len(value) // 768 for name, value in entries.items()} != SEGMENTS:
        raise TapeError("monitor file inventory differs from archived artifact")
    destination.mkdir()  # Refuse overwrites, including earlier inspection results.
    manifest = {"artifact": "30003039", "size": SIZE, "sha256": SHA256,
                "scope": "first complete monitor8000 dump only", "files": {}}
    archive = []
    for name, value in sorted(entries.items()):
        (destination / f"{name}.bin").write_bytes(value)
        (destination / f"{name}.listing.txt").write_bytes(text_bytes(value))
        manifest["files"][name] = {"bytes": len(value),
                                    "sha256": hashlib.sha256(value).hexdigest()}
        if name.startswith("mon8part"):
            lines = listing_lines(value)
            (destination / f"{name}.numbered-lines.txt").write_text(
                "\n".join(lines) + "\n", encoding="ascii")
            archive.extend(normalized_lines(lines))
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    archive_lines = set(archive)
    report = ["Datamuseum 30003039 comparison", "",
              f"Verified tape: {SIZE} bytes; SHA-256 {SHA256}",
              "Six assembler listings and mon8list; not plain source modules.",
              "Only the first complete monitor8000 dump is extracted.",
              "Numbered-line derivatives omit unnumbered messages and tables;",
              "they are comparison aids, not verified rebuild inputs.", "",
              "Whitespace-normalized nonempty lines are compared, retaining comments.",
              "Matches count occurrences in each local file, not unique lines.",
              "The longest block is measured across concatenated mon8part1..6.",
              "These textual measures do not prove direct ancestry or compatibility.", ""]
    for path in sorted(sources.iterdir()):
        if path.name == "README.md" or not path.is_file():
            continue
        local = normalized_lines(path.read_bytes().decode("latin-1").splitlines())
        shared = sum(line in archive_lines for line in local)
        block = difflib.SequenceMatcher(None, local, archive).find_longest_match()
        report.append(f"{path.name}: {shared}/{len(local)} shared line occurrences; "
                      f"longest matching block {block.size} nonempty lines")
        if block.size >= 5:
            report.append("  Block begins: " + local[block.a])
    (destination / "comparison.txt").write_text("\n".join(report) + "\n", encoding="utf-8")
    print(f"Extracted {len(entries)} monitor areas to {destination}")
    print(f"Comparison: {destination / 'comparison.txt'}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("destination", type=Path, help="new output directory")
    parser.add_argument("--sources", type=Path, default=ROOT / "monsrc")
    args = parser.parse_args()
    try:
        inspect(args.image, args.destination, args.sources)
    except (OSError, ValueError) as error:
        print(f"monitor tape inspection failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
