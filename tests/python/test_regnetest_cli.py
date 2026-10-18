#!/usr/bin/env python3
"""Opt-in: run one historical regnetest iteration in a selected monitor profile."""

import argparse
from contextlib import nullcontext
import hashlib
import os
from pathlib import Path
import re
import tempfile
import time

from test_operator_cli import Session


def verify_iteration(output):
    output = output.replace("\r", "")
    for expression in (
        r"sum af tal=\s*360\.700643456\s+test ok",
        r"sum af tal=\s*1\.85598980864'3\s+test ok",
        r"antal okruns\s*=\s*1\s*\n",
        r"antal fejlruns\s*=\s*0\s*\n",
    ):
        assert re.search(expression, output), output
    assert "pause test break" not in output and "***fp reinitialized" not in output, output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("simulator", type=Path)
    parser.add_argument("--protected", action="store_true")
    parser.add_argument("--output", type=Path, help="new directory for retained logs")
    args = parser.parse_args()
    simulator = args.simulator.resolve()
    root = Path(__file__).resolve().parents[2]
    profile = "protected" if args.protected else "compatibility"
    monitor = root / ("src/monitor/MONITOR.protected" if args.protected else "src/monitor/MONITOR")
    disk = root / ("src/disks/UNIT0.protected" if args.protected else "src/disks/UNIT0")
    fixtures = (root / "src/monitor/MONITOR", root / "src/disks/UNIT0",
                root / "src/disks/UNIT0.original", monitor, disk)
    before = [hashlib.sha256(path.read_bytes()).digest() for path in fixtures]
    if args.output:
        args.output = args.output.resolve()
        args.output.mkdir()  # Refuse existing work and preserve earlier diagnostics.
    context = nullcontext(str(args.output)) if args.output else \
        tempfile.TemporaryDirectory(prefix="sim4000-regnetest-cli-")
    with context as tmp:
        directory = Path(tmp)
        (directory / "RC4000.cfg").write_text(
            ("PROTECTION MODE=RC4000\n" if args.protected else "") +
            f"MONITOR FILE={monitor}\nCORESIZE=0,1048576\n"
            "THROTTLE VALUE=OFF\n"
            "DEFINE CONSOLE DEVNO=2 INTNO=6,3\n"
            "DEFINE CLOCK DEVNO=3 INTNO=11\n"
            f"DEFINE RC433X DEVNO=15 FILE={disk}\n", encoding="ascii")
        # No CPU setting or IX override: accept the normal software default.
        session = None
        log = (directory / "session.log").open("x", encoding="ascii")

        def record(text):
            log.write(text)
            log.flush()
            return text

        try:
            session = Session(simulator, directory, ("-a",), startup=b"initialize date:")
            record("initialize date:\n")
            os.write(session.master, b"86 09 30 08 00 00\r")
            record(session.until(b"pause startproc finis fp"))
            os.write(session.master, b"\x1b")
            record(session.until(b"att"))
            os.write(session.master, b"s\rnew test size 100000 run\r")
            record(session.until(b"to test"))
            if args.protected:
                record(session.until(b"\x07"))
                os.write(session.master, b"claim\r")
                claim = record(session.until(b"\x07"))
                match = re.search(r"size\s+100000\s+first core\s+(\d+)", claim)
                assert match, claim
                first = int(match[1])
            os.write(session.master, b"i regnetest\r")
            deadline = time.monotonic() + 120
            output = ""
            while True:
                line = record(session.until(b"\n", timeout=max(0.01, deadline - time.monotonic())))
                output += line
                assert "pause test break" not in line and "***fp reinitialized" not in line, output
                if "antal fejlruns" in line:
                    break
                assert time.monotonic() < deadline, output
            verify_iteration(output)
            if args.protected:
                assert re.search(r"algol end\s+61\b", output), output
                assert "version 83.04.12" in output, output
            # Suspend rather than waiting for a second self-test iteration.
            os.write(session.master, b"\x05")
            record(session.until(b"\nsim4000> "))
            cpu = record(session.command(b"show cpu\n"))
            assert "ix=on" in cpu, cpu
            assert f"protection={'rc4000' if args.protected else 'off'} " in cpu, cpu
            assert "mode=volatile-overlay" in record(session.command(b"show media\n"))
            if args.protected:
                for address in (first, first + 50000, first + 99998):
                    keys = record(session.command(f"show keys {address}\n".encode("ascii")))
                    assert f"{address:06x}: key=1 " in keys, keys
            session.finish()
            assert [hashlib.sha256(path.read_bytes()).digest() for path in fixtures] == before
            (directory / "result.txt").write_text(
                f"regnetest profile={profile}: both numerical checks passed; 1 successful/0 failed runs.\n"
                f"monitor-sha256={hashlib.sha256(monitor.read_bytes()).hexdigest()}\n"
                f"disk-sha256={hashlib.sha256(disk.read_bytes()).hexdigest()}\n",
                encoding="ascii")
        finally:
            if session is not None:
                if session.pending:
                    record(session.pending.decode("ascii", errors="backslashreplace"))
                session.close()
            log.close()
            assert [hashlib.sha256(path.read_bytes()).digest() for path in fixtures] == before
    assert [hashlib.sha256(path.read_bytes()).digest() for path in fixtures] == before
    print(f"{profile} regnetest: both numerical checks passed, 1 successful/0 failed runs")


if __name__ == "__main__":
    main()
