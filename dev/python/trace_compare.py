#!/usr/bin/env python3
"""Compare two version-1 SIM4000 architectural traces."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


FIELD_NAMES = {
    "W": ("event", "call", "address", "previous", "new"),
    "S": (
        "event", "call", "result", "instructions", "delta", "address",
        "raw", "opcode", "mode", "index", "result-register",
        "displacement", "effective", "ic", "w0", "w1", "w2", "w3",
        "ir", "im", "ex21", "ex22", "ex23", "interrupts-enabled",
        "monitor", "reset", "stopped", "io-operations", "io-delta",
        "last-device", "last-command", "last-modifier", "interrupts",
        "interrupt-delta", "ticks",
    ),
}


class TraceError(Exception):
    """A malformed or mismatching trace."""


def read_trace(path: Path) -> list[tuple[int, tuple[str, ...]]]:
    try:
        lines = path.read_text(encoding="ascii").splitlines()
    except (OSError, UnicodeError) as error:
        raise TraceError(f"{path}: cannot read trace: {error}") from error
    if not lines or lines[0] != "SIM4000-TRACE\t1":
        raise TraceError(f"{path}: unsupported or missing trace header")

    records: list[tuple[int, tuple[str, ...]]] = []
    for line_number, line in enumerate(lines[1:], 2):
        if not line or line.startswith("#"):
            continue
        fields = tuple(line.split("\t"))
        names = FIELD_NAMES.get(fields[0])
        if names is None:
            raise TraceError(
                f"{path}:{line_number}: unknown event {fields[0]!r}"
            )
        if len(fields) != len(names):
            raise TraceError(
                f"{path}:{line_number}: {fields[0]} has {len(fields)} fields; "
                f"version 1 requires {len(names)}"
            )
        records.append((line_number, fields))
    return records


def compare(expected_path: Path, actual_path: Path) -> int:
    expected = read_trace(expected_path)
    actual = read_trace(actual_path)
    common = min(len(expected), len(actual))

    for index in range(common):
        expected_line, expected_fields = expected[index]
        actual_line, actual_fields = actual[index]
        if expected_fields == actual_fields:
            continue
        if expected_fields[0] != actual_fields[0]:
            raise TraceError(
                f"record {index + 1}: expected {expected_fields[0]} at "
                f"{expected_path}:{expected_line}, got {actual_fields[0]} at "
                f"{actual_path}:{actual_line}"
            )
        names = FIELD_NAMES[expected_fields[0]]
        for field_index, (expected_value, actual_value) in enumerate(
            zip(expected_fields, actual_fields)
        ):
            if expected_value != actual_value:
                call = expected_fields[1]
                raise TraceError(
                    f"record {index + 1} ({expected_fields[0]} call {call}), "
                    f"field {names[field_index]}: expected {expected_value!r}, "
                    f"got {actual_value!r}"
                )

    if len(expected) != len(actual):
        raise TraceError(
            f"record count: expected {len(expected)}, got {len(actual)}"
        )
    print(f"trace match: {len(actual)} records")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare SIM4000 trace records and identify the first mismatch."
    )
    parser.add_argument("expected", type=Path)
    parser.add_argument("actual", type=Path)
    arguments = parser.parse_args()
    try:
        return compare(arguments.expected, arguments.actual)
    except TraceError as error:
        print(f"trace mismatch: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
