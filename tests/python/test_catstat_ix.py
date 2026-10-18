#!/usr/bin/env python3
"""Check IX-enabled catstat against the stored catalog, not the live namespace."""
from collections import Counter
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from test_operator_cli import Session


def main():
    builder, simulator = (Path(p).resolve() for p in sys.argv[1:3])
    root = Path(__file__).resolve().parents[2]
    monitor, disk = root / "src/monitor/MONITOR", root / "src/disks/UNIT0"
    fixtures = [monitor, disk, root / "src/disks/UNIT0.original"]
    variants = [("--verify-catstat-ix", monitor)]
    if len(sys.argv) > 3:
        standard = Path(sys.argv[3]).resolve()
        fixtures.append(standard)
        variants.append(("--verify-catstat-ix-dfc", standard))
    before = [hashlib.sha256(p.read_bytes()).digest() for p in fixtures]
    offline = subprocess.check_output(
        [str(simulator), "--list-main-catalog", str(disk)], text=True)
    expected = Counter((int(f[0]), f[2]) for line in offline.splitlines()[2:]
                       if (f := line.split()))
    with tempfile.TemporaryDirectory(prefix="sim4000-ix-") as tmp:
        directory = Path(tmp)
        for mode, boot in variants:
            host = directory / mode.removeprefix("--")
            host.mkdir()
            result = subprocess.run([str(builder), mode, str(boot), str(disk), str(host)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, result.stdout + result.stderr
            assert "break" not in result.stdout and "***fp reinitialized" not in result.stdout
            assert "pause muser" not in result.stdout and "syntax error:" not in result.stdout
            report = result.stdout.split("\x07catstat\n", 1)[1].split("\nend", 1)[0]
            actual, headers = Counter(), []
            for line in report.splitlines():
                match = re.fullmatch(r"segment\s+(\d+)\s+(\d+) entries", line)
                if match:
                    segment = int(match[1]); headers.append((segment, int(match[2])))
                elif line.strip():
                    assert line.startswith(" " * 14), repr(line)
                    for i in range(14, len(line), 12):
                        if name := line[i:i + 12].strip(): actual[(segment, name)] += 1
            assert [s for s, n in headers] == list(range(72))
            assert actual == expected, (actual - expected, expected - actual)
            for s, n in headers: assert sum(c for (seg, name), c in actual.items() if seg == s) == n
            assert sum(actual.values()) == 418
            data = (host / "catraw").read_bytes()
            assert len(data) == 72 * 768 and (host / "again").read_bytes() == data
            assert {p.name for p in host.iterdir()} == {"catraw", "again"}
        config = directory / "RC4000.cfg"
        for setting, options, enabled in (
            ("", (), True), ("YES", (), True), ("YES", ("--no-ix",), False),
            ("NO", ("--ix",), True), ("YES", ("--no-ix", "--ix"), True),
            ("NO", ("--ix", "--no-ix"), False)):
            config.write_text(f"MONITOR FILE={monitor}\nCORESIZE=0,1048576\n" +
                              (f"CPU IX={setting}\n" if setting else ""), encoding="ascii")
            checked = subprocess.run([str(simulator), "--check-config", str(config)],
                                     capture_output=True, text=True, timeout=10)
            assert checked.returncode == 0, checked.stderr
            assert f"ix={'off' if setting == 'NO' else 'on'}" in checked.stdout
            session = Session(simulator, directory, ("-i", *options))
            try:
                shown = session.command(b"show cpu\n")
                assert f"ix={'on' if enabled else 'off'}" in shown
                assert all(len(line) <= 80 for line in shown.splitlines())
                for command in ("deposit 64 0xf50080", "deposit 66 0x004001",
                                "deposit 128 100", "deposit 130 40", "deposit 132 0",
                                "deposit 12 96", "deposit W1 1", "deposit IC 64"):
                    session.command((command + "\n").encode("ascii"))
                decoded = session.command(b"examine /instruction 64\n")
                assert ("ix w1" if enabled else "?? w1") in decoded, decoded
                traced = session.command(b"trace 1\n")
                assert ("ix w1" if enabled else "?? w1") in traced, traced
                assert ("W1=000066" if enabled else "W1=000001") in traced, traced
                if enabled: assert "IC=000044" in traced, traced
                session.command(b"reset\n")
                assert f"ix={'on' if enabled else 'off'}" in session.command(b"show cpu\n")
                session.finish()
            finally: session.close()
        # Accept the actual autostart path, without CPU configuration or --ix.
        config.write_text(
            f"MONITOR FILE={monitor}\nCORESIZE=0,1048576\n"
            "DEFINE CONSOLE DEVNO=2 INTNO=6,3\n"
            "DEFINE CLOCK DEVNO=3 INTNO=11\n"
            f"DEFINE RC433X DEVNO=15 FILE={disk}\n", encoding="ascii")
        session = Session(simulator, directory, ("-a",), startup=b"initialize date:")
        try:
            os.write(session.master, b"86 09 30 08 00 00\r")
            session.until(b"pause startproc finis fp")
            os.write(session.master, b"\x1b")
            session.until(b"att")
            os.write(session.master, b"s\rnew muser size 100000 run\r")
            session.until(b"to muser")
            os.write(session.master, b"catstat\r")
            report = (session.until(b"\nend") + session.until(b"\n")).replace("\r", "")
            assert re.search(r"\nend\s+18\s*\n", report), report
            assert "break" not in report and "***fp reinitialized" not in report
            headers = re.findall(r"^segment\s+(\d+)\s+(\d+) entries$", report, re.M)
            assert [int(s) for s, n in headers] == list(range(72)), headers
            assert sum(int(n) for s, n in headers) == 418
            os.write(session.master, b"lookup catstat\r")
            following = session.until(b"; system") + session.until(b"\x07")
            assert "catstat" in following and "break" not in following, following
            os.write(session.master, b"\x05")
            session.until(b"\nsim4000> ")
            assert "ix=on" in session.command(b"show cpu\n")
            assert "mode=volatile-overlay" in session.command(b"show media\n")
            session.finish()
        finally:
            session.close()
    assert [hashlib.sha256(p.read_bytes()).digest() for p in fixtures] == before
    print("IX-enabled catalog report, default autostart, FP continuation, CLI overrides, and integrity passed")


if __name__ == "__main__": main()
