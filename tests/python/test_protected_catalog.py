#!/usr/bin/env python3
"""Opt-in: check catalog and resource tools with protected/compatibility media."""

import argparse
from collections import Counter
import hashlib
import os
from pathlib import Path
import re
import subprocess

from test_operator_cli import Session

ROOT = Path(__file__).resolve().parents[2]
SAMPLE = b"Catalog allocation sample.\nSecond line.\n"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def packed_catalog(data, retained_only=False):
    assert len(data) == 72 * 768, len(data)
    entries = Counter()
    for segment in range(72):
        for slot in range(15):
            offset = segment * 768 + slot * 51
            if data[offset:offset + 3] == b"\xff\xff\xff":
                continue
            if retained_only and (int.from_bytes(data[offset:offset + 3], "big") & 7) == 0:
                continue
            name = data[offset + 9:offset + 21].split(b"\0", 1)[0].decode("ascii")
            assert name and all(32 <= ord(c) < 127 for c in name), (segment, slot, name)
            entries[(segment, name)] += 1
    return entries


def catstat_report(output):
    text = output.replace("\r", "")
    assert re.search(r"\nend\s+18\s*\n", text), text
    body = text.split("catstat\n", 1)[1].split("\nend", 1)[0]
    entries, headers = Counter(), []
    segment = None
    for line in body.splitlines():
        match = re.fullmatch(r"segment\s+(\d+)\s+(\d+) entries", line)
        if match:
            segment = int(match[1])
            headers.append((segment, int(match[2])))
        elif line.strip():
            assert segment is not None and line.startswith(" " * 14), line
            for offset in range(14, len(line), 12):
                name = line[offset:offset + 12].strip()
                if name:
                    entries[(segment, name)] += 1
    assert [segment for segment, _ in headers] == list(range(72)), headers
    for segment, count in headers:
        assert sum(n for (s, _), n in entries.items() if s == segment) == count
    return entries


def claim_report(output):
    text = output.replace("\r", "")
    header = re.search(r"area (\d+)\s+buf (\d+)\s+size (\d+)\s+first core (\d+)", text)
    assert header and "disc: 3 segm/slice" in text, text
    header = tuple(map(int, header.groups()))
    assert header[2] == 100000 and header[3] >= 8 and header[3] % 2 == 0
    assert header[3] + header[2] <= 1048576
    rows = re.findall(r"(?m)^\s*(temp|login|perm)\s+(\d+) segm\s+(\d+) slices\s+(\d+) entr", text)
    assert [row[0] for row in rows] == ["temp", "login", "perm"], text
    claims = {name: tuple(map(int, values)) for name, *values in rows}
    assert all(segments == slices * 3 and entries > 0 for segments, slices, entries in claims.values())
    return header, claims


def free_space(output):
    matches = re.findall(r"free disc space:\s+disc\s*:\s*(\d+) slices,\s*(\d+) segm\.", output)
    assert len(matches) == 1, output
    slices, segments = map(int, matches[0])
    assert slices > 0 and segments == slices * 3
    return slices, segments


def sorted_rows(output):
    return re.findall(r"(?m)^\w+[ \t]+(?:[a-z]+|[+-]?\d+(?:\.\d+)?)[ \t]+\w+[ \t]+[^\r\n]*", output)


def sorted_names(output):
    return [row.split()[0] for row in sorted_rows(output)]


def descriptor_names(output):
    names = re.findall(r"(?m)^([a-z][a-z0-9]*)\s*=", output)
    assert len(names) == len(set(names)), names
    return set(names)


def run(simulator, directory, compatibility=False):
    profile = "compatibility" if compatibility else "protected"
    monitor = ROOT / ("src/monitor/MONITOR" if compatibility else "src/monitor/MONITOR.protected")
    disk = ROOT / ("src/disks/UNIT0" if compatibility else "src/disks/UNIT0.protected")
    media = {monitor, disk, ROOT / "src/monitor/MONITOR", ROOT / "src/disks/UNIT0",
             ROOT / "src/disks/UNIT0.original"}
    before = {path: digest(path) for path in media}
    directory.mkdir()  # Preserve earlier diagnostics or personal working files.
    exchange = directory / "exchange"
    exchange.mkdir()
    (exchange / "input").write_bytes(SAMPLE)
    offline = subprocess.check_output([str(simulator), "--list-main-catalog", str(disk)],
                                      text=True, timeout=10)
    (directory / "offline.txt").write_text(offline, encoding="ascii")
    offline_rows = [line.split() for line in offline.splitlines()[2:] if line.split()]
    retained = Counter((int(fields[0]), fields[2]) for fields in offline_rows if int(fields[7]) != 0)
    assert retained
    device = subprocess.check_output([str(simulator), "--list-catalog", str(disk)],
                                     text=True, timeout=10)
    (directory / "device-catalog.txt").write_text(device, encoding="ascii")
    device_rows = [line.split() for line in device.splitlines()[2:] if line.split()]
    system_names = {f[2] for f in device_rows if f[8:10] == ["800001", "7ffffd"]}
    user_names = {f[2] for f in device_rows if f[8:10] == ["7ffffd", "7ffffd"]}
    assert system_names and system_names | user_names | {"catalog"} == {f[2] for f in device_rows}
    (directory / "RC4000.cfg").write_text(
        f"PROTECTION MODE={'OFF' if compatibility else 'RC4000'}\n"
        f"MONITOR FILE={monitor}\nCORESIZE=0,1048576\nTHROTTLE VALUE=OFF\n"
        "DEFINE CONSOLE DEVNO=2 INTNO=6,3\nDEFINE CLOCK DEVNO=3 INTNO=11\n"
        f"DEFINE RC433X DEVNO=15 FILE={disk}\n"
        f"DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT={exchange}\n", encoding="ascii")
    transfer_in, transfer_out = ("fromhost", "tohost") if compatibility else ("rcin", "rcout")
    session = None
    with (directory / "session.log").open("x", encoding="ascii") as log:
        def record(output):
            log.write(output)
            log.flush()
            return output

        def guest(line, expected_text=None, diagnostic=None, special_catalog=False):
            os.write(session.master, line.encode("ascii") + b"\r")
            output = record(session.until(b"\x07", timeout=30))
            assert " break fp" not in output and "***fp reinitialized" not in output, output
            checked = output
            if special_catalog:
                assert re.search(r"(?m)^catalog\s+=set 72 disc [^\r\n]*; \*\*\*\r?$", output), output
                assert re.search(r"(?m)^\s*;\s+964 23 1 -8388607 8388606\r?$", output), output
                # The special catalog interval has no printable scope label;
                # this inline marker is not a leading FP error diagnostic.
                checked = output.replace("; ***", "; special-catalog", 1)
            if diagnostic is None:
                assert "***" not in checked and "syntax error:" not in checked, output
            else:
                assert re.findall(r"(?m)^\*\*\*[^\r\n]*", output) == [diagnostic], output
            if expected_text:
                assert expected_text in output, output
            return output

        try:
            session = Session(simulator, directory, ("-a",), startup=b"initialize date:")
            record("initialize date:\n")
            os.write(session.master, b"86 09 30 08 00 00\r")
            record(session.until(b"pause startproc finis fp"))
            os.write(session.master, b"\x1b")
            record(session.until(b"att") + session.until(b"\x07"))
            guest("s")
            guest("new catcheck size 100000 temp disc 200 10 perm disc 200 10 run", "to catcheck")
            initial_claim = claim_report(guest("claim"))
            guest(f"catbefore={transfer_out} conv.bin catalog", "bytes")
            raw = (exchange / "catbefore").read_bytes()
            packed = packed_catalog(raw)
            # Boot reclaims stale temporary entries and creates console entries.
            # Compare retained entries against the offline base, and compare
            # catstat against the independent byte decode of this session.
            actual_retained = packed_catalog(raw, retained_only=True)
            assert actual_retained == retained, (actual_retained - retained, retained - actual_retained)
            actual = catstat_report(guest("catstat"))
            assert actual == packed, (actual - packed, packed - actual)
            cata = guest("cata")
            for label, value in (("antal segmenter", 72), ("indgange ialt", 1080),
                                 ("frie indgange", 1080 - sum(packed.values()))):
                assert re.search(rf"{label}:\s*{value}\b", cata), cata
            initial_space = free_space(guest("show"))
            assert free_space(guest("show")) == initial_space
            assert claim_report(guest("claim")) == initial_claim
            guest(f"catafter={transfer_out} conv.bin catalog", "bytes")
            assert (exchange / "catafter").read_bytes() == raw

            guest(f"spacetmp={transfer_in} input", "bytes")
            lookup = guest("lookup spacetmp", "; temp")
            match = re.search(r"(?m)^spacetmp\s+=set (\d+) disc .*; temp", lookup)
            assert match and int(match[1]) == 36, lookup
            allocated_claim = claim_report(guest("claim"))
            allocated_space = free_space(guest("show"))
            assert allocated_claim[0] == initial_claim[0]
            for scope in ("login", "perm"):
                assert allocated_claim[1][scope] == initial_claim[1][scope]
            assert tuple(a - b for a, b in zip(initial_claim[1]["temp"], allocated_claim[1]["temp"])) == (36, 12, 1)
            assert tuple(a - b for a, b in zip(initial_space, allocated_space)) == (12, 36)
            guest(f"sampleout={transfer_out} spacetmp", "bytes")
            assert (exchange / "sampleout").read_bytes() == SAMPLE
            guest("clear temp spacetmp")
            guest("lookup spacetmp", diagnostic="***lookup spacetmp unknown")
            assert claim_report(guest("claim")) == initial_claim
            assert free_space(guest("show")) == initial_space
            guest("lookup catstat", "; system")

            # Compare each scope against offline descriptors plus controlled
            # guest changes, not against another guest listing of the same scope.
            for name in ("saved", "gone"):
                guest(f"{name}={transfer_in} input", "bytes")
            assert sorted_names(guest("catsort scope.temp")) == ["c", "gone", "saved", "v"]
            guest("scope user saved")
            guest("lookup saved", "; user")
            guest("lookup gone", "; temp")
            assert sorted_names(guest("catsort scope.temp")) == ["c", "gone", "v"]
            for command, label in (("catsort", "project"), ("catsort scope.user", "perm")):
                report = guest(command)
                kept, temporary = report.split("base: 8388605 8388605 temp", 1)
                assert f"base: 8388605 8388605 {label}" in kept, report
                assert sorted_names(kept) == sorted(user_names | {"saved"}), report
                assert sorted_names(temporary) == ["c", "gone", "v"], report
            system_report = guest("catsort scope.system")
            assert sorted_names(system_report) == sorted(system_names), system_report
            assert "\x0c" in system_report
            guest("o sortreport")
            guest("catsort scope.system")
            guest("o c")
            guest(f"sortout={transfer_out} sortreport", "bytes")
            exported = (exchange / "sortout").read_bytes()
            assert all(c not in exported for c in (b"\r", b"\0", b"\x19"))
            assert sorted_rows(exported.decode("ascii")) == sorted_rows(system_report)
            guest("clear temp sortreport")
            for scope, expected in (("temp", {"c", "gone", "v"}),
                                    ("user", user_names | {"saved"}),
                                    ("system", system_names)):
                report = guest(f"search {scope}")
                assert descriptor_names(report) == expected, (scope, descriptor_names(report) ^ expected)
                # Redirected search may include its own newly created result.
                guest(f"listreport=search {scope}")
                guest(f"{scope}out={transfer_out} listreport", "bytes")
                exported = (exchange / f"{scope}out").read_bytes()
                assert b"\r" not in exported and b"\0" not in exported
                expected_export = expected | ({"listreport"} if scope == "temp" else set())
                exported_names = descriptor_names(exported.decode("ascii"))
                if scope == "temp":
                    scratch = exported_names - expected_export
                    assert len(scratch) <= 1 and all(re.fullmatch(r"wrk\d{6}", name) for name in scratch), scratch
                    expected_export |= scratch
                assert exported_names == expected_export, (scope, exported_names ^ expected_export)
                labels = re.findall(r"(?m)^[a-z][a-z0-9]*\s*=.*; ([a-z.]+)$", exported.decode("ascii"))
                assert len(labels) == len(exported_names)
                assert all(label == scope or label.startswith(scope + ".") for label in labels), labels
                guest("clear temp listreport")
                assert descriptor_names(guest("search temp")) == {"c", "gone", "v"}
            guest("lookup catalog", special_catalog=True)
            guest(f"savedout={transfer_out} saved", "bytes")
            guest(f"goneout={transfer_out} gone", "bytes")
            assert (exchange / "savedout").read_bytes() == SAMPLE
            assert (exchange / "goneout").read_bytes() == SAMPLE
            guest("scope temp saved")
            guest("clear temp saved gone")
            assert claim_report(guest("claim")) == initial_claim
            assert free_space(guest("show")) == initial_space
            os.write(session.master, b"\x05")
            record(session.until(b"\nsim4000> "))
            cpu = record(session.command(b"show cpu\n"))
            assert f"protection={'off' if compatibility else 'rc4000'} " in cpu and "ix=on" in cpu
            shown_media = record(session.command(b"show media\n"))
            assert "mode=volatile-overlay" in shown_media.replace("\n", "").replace("\r", "")
            if not compatibility:
                first = initial_claim[0][3]
                for address in (first, first + 50000, first + 99998):
                    keys = record(session.command(f"show keys {address}\n".encode("ascii")))
                    assert f"{address:06x}: key=1 " in keys, keys
            session.finish()
            assert {path: digest(path) for path in media} == before
            assert (exchange / "input").read_bytes() == SAMPLE
            (directory / "result.txt").write_text(
                f"Catalog/resource profile={profile}: catstat, cata, show and claim passed.\n"
                f"Stored main catalog after boot: 72 segments, {sum(packed.values())} entries; unchanged by read-only tools.\n"
                "Temporary allocation/reclamation: 36 segments, 12 slices, one entry.\n"
                f"Scope reports: {len(system_names)} system entries, {len(user_names)} pre-existing user entries; catsort/search and exported rows passed.\n"
                f"monitor-sha256={digest(monitor)}\ndisk-sha256={digest(disk)}\n",
                encoding="ascii")
        finally:
            if session is not None:
                if session.pending:
                    record(session.pending.decode("ascii", errors="backslashreplace"))
                session.close()
            assert {path: digest(path) for path in media} == before, "source media changed"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("simulator", type=Path)
    parser.add_argument("destination", type=Path, help="new exclusive workspace")
    parser.add_argument("--compatibility", action="store_true")
    args = parser.parse_args()
    run(args.simulator.resolve(), args.destination.resolve(), args.compatibility)
    print(f"{'Compatibility' if args.compatibility else 'Protected'} catalog/resource checkpoint passed")


if __name__ == "__main__":
    main()
