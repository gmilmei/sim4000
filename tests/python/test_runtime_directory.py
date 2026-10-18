#!/usr/bin/env python3
"""Run only copied runtime files, with persistent guest and host-file data."""

import datetime
import hashlib
import os
import pathlib
import select
import shutil
import sys
import tempfile
import time

from test_operator_cli import Session


def digest(path):
    return hashlib.sha256(path.read_bytes()).digest()


def guest(session, command, marker):
    # Submit each complete line as one write, including lines beyond the
    # console queue capacity, to exercise host-input backpressure.
    os.write(session.master, command.encode("ascii") + b"\r")
    result = session.until(marker) if marker is not None else ""
    # FP has no prompt. Allow final output after the expected marker to settle
    # before entering the next line, just as a person entering separate lines.
    deadline = time.monotonic() + 10
    while select.select([session.process.stdout], [], [], 0.2)[0]:
        assert time.monotonic() < deadline, session.pending
        data = os.read(session.process.stdout.fileno(), 65536)
        assert data, (session.process.poll(), session.pending)
        session.pending += data
    return result


def boot(executable, directory, arguments, date_method="manual"):
    session = Session(executable, directory, arguments=arguments,
                      startup=b"initialize date:")
    try:
        if date_method == "alt":
            os.write(session.master, b"\x1bd")
            session.until(b"pause startproc finis fp")
        elif date_method == "operator":
            os.write(session.master, b"\x05")
            session.until(b"\nsim4000> ")
            assert "queued input: 18 bytes" in session.command(b"send-date\n")
            os.write(session.master, b"continue\n")
            session.until(b"pause startproc finis fp")
        else:
            guest(session, "86 09 30 08 00 00", b"pause startproc finis fp")
        os.write(session.master, b"\x1b")
        session.until(b"att")
        guest(session,
              "s\rnew work size 100000 temp disc 200 10 perm disc 200 10 run",
              b"to work")
        return session
    except BaseException:
        session.close()
        raise


def stop(session):
    os.write(session.master, b"\x05")
    session.until(b"\nsim4000> ")
    session.finish()


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(__file__).resolve().parents[2]
    sources = (executable, root / "src/config/RC4000.cfg",
               root / "src/monitor/MONITOR", root / "src/disks/UNIT0")
    before = [digest(path) for path in sources]
    with tempfile.TemporaryDirectory(prefix="sim4000-runtime-") as temporary:
        parent = pathlib.Path(temporary)
        runtime = parent / "runtime"
        runtime.mkdir()
        for source in sources:
            shutil.copy2(source, runtime / source.name)
        assert {path.name for path in runtime.iterdir()} == {
            "sim4000", "RC4000.cfg", "MONITOR", "UNIT0"}
        exchange = runtime / "exchange"
        exchange.mkdir()
        text = b"Standalone runtime sample.\nSecond line.\n"
        binary = bytes(range(256)) * 3  # All byte values in one RC disk segment.
        (exchange / "input").write_bytes(text)
        (exchange / "bininput").write_bytes(binary)

        # Enable persistence using only a relative path in the copied config.
        config = runtime / "RC4000.cfg"
        config.write_text(config.read_text(encoding="ascii").replace(
            "DEFINE RC433X  DEVNO=15 FILE=UNIT0\n",
            "DEFINE RC433X  DEVNO=15 FILE=UNIT0 OVERLAY=UNIT0.overlay\n").replace(
            "DATE YEAROFFSET=-40\n",
            # A leap target year also accepts February 29 on future test runs.
            f"DATE YEAROFFSET={1988 - datetime.datetime.now().year}\n"),
            encoding="ascii")
        unchanged = [digest(runtime / path.name) for path in sources]
        copied_executable = runtime / "sim4000"

        # Default configuration discovery from the standalone working directory.
        session = boot(copied_executable, runtime, ("-a",), date_method="operator")
        try:
            guest(session, "kepttext=fromhost input", b"bytes")
            # Suspend mid-paste before the guest drains the queue, then resume.
            # The entire binary import must survive, including the host backlog.
            os.write(session.master, b"bincopy=fromhost conv.bin bininpu\x05")
            session.until(b"\nsim4000> ")
            # Append the final filename byte and Return behind the pasted prefix.
            assert "queued input: 2 bytes" in session.command(b"send 74 0d\n")
            os.write(session.master, b"continue\n")
            session.until(b"bytes")
            guest(session, "lookup bincopy", b"; temp")
            for name in ("kepttext", "bincopy"):
                guest(session, "scope user " + name, None)
                guest(session, "lookup " + name, b"; user")
            guest(session, "output=tohost kepttext", b"bytes")
            guest(session, "binoutput=tohost conv.bin bincopy", b"bytes")
            assert (exchange / "output").read_bytes() == text
            exported = (exchange / "binoutput").read_bytes()
            # Binary exports include the allocated RC area, not just input length.
            # Padding may contain old sector contents; do not require zero fill.
            assert exported[:len(binary)] == binary
            assert len(exported) % 768 == 0
            stop(session)
        finally:
            session.close()
        assert (runtime / "UNIT0.overlay").stat().st_size > 0

        # Restart from a different CWD: all configured paths remain config-relative.
        session = boot(copied_executable, parent,
                       ("-a", "-c", "runtime/RC4000.cfg"), date_method="alt")
        try:
            for name in ("kepttext", "bincopy"):
                guest(session, "lookup " + name, b"; user")
            guest(session, "rebooted=tohost kepttext", b"bytes")
            guest(session, "binreboot=tohost conv.bin bincopy", b"bytes")
            assert (exchange / "rebooted").read_bytes() == text
            assert (exchange / "binreboot").read_bytes() == exported
            stop(session)
        finally:
            session.close()
        assert [digest(runtime / path.name) for path in sources] == unchanged
        assert {path.name for path in parent.iterdir()} == {"runtime"}
    assert [digest(path) for path in sources] == before


if __name__ == "__main__":
    main()
