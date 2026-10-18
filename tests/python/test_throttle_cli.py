#!/usr/bin/env python3
"""Check runtime pacing, CPU budgeting, AM counting, and responsive waiting."""

import os
import hashlib
import pathlib
import re
import select
import signal
import subprocess
import sys
import tempfile
import time

from test_operator_cli import Session
from test_operator_acceptance import Remote, ports, require


def calls(session):
    output = session.command(b"show cpu\n")
    return int(re.search(r"calls=(\d+)", output).group(1))


def cpu_seconds(pid):
    # /proc stat fields 14 and 15, after the parenthesized command name.
    text = pathlib.Path(f"/proc/{pid}/stat").read_text()
    fields = text[text.rindex(")") + 2:].split()
    return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")


def paced_run(session, duration):
    before = calls(session)
    os.write(session.master, b"run\n")
    start = time.monotonic()
    time.sleep(duration)
    os.write(session.master, b"\x05")
    session.until(b"\nsim4000> ")
    elapsed = time.monotonic() - start
    after = calls(session)
    return after - before, elapsed


def blocked_input(executable, directory):
    # The guest loop never reads the console. More than one queue's worth of
    # pasted bytes must not turn the pacing wait into a readable-stdin spin.
    session = Session(executable, directory)
    try:
        require(session, "load-core loop.bin 0x200", "loaded")
        require(session, "deposit ic 0x200", "IC")
        require(session, "set throttle 20%", "throttle=20%")
        os.write(session.master, b"run\n")
        time.sleep(0.1)
        os.write(session.master, b"x" * 256)
        before = cpu_seconds(session.process.pid)
        start = time.monotonic()
        time.sleep(0.9)
        elapsed = time.monotonic() - start
        used = cpu_seconds(session.process.pid) - before
        assert used / elapsed <= 0.4, ("full input queue CPU use", used, elapsed)
        start = time.monotonic()
        os.write(session.master, b"\x05")
        session.until(b"\nsim4000> ")
        assert time.monotonic() - start < 1.0
        require(session, "show cpu", "stopped=yes")
        status = require(session, "show input", "host-buffer=225")
        assert "guest-queue=31" in status and "pending-byte=0" in status, status
        require(session, "discard-input extra", "usage: discard-input")
        require(session, "show input", "host-buffer=225")
        require(session, "show input extra", "usage: show input")
        require(session, "send 41 ZZ", "invalid 7-bit hexadecimal byte")
        require(session, "show input", "host-buffer=225")
        require(session, "send " + " ".join(["41"] * 40), "queued input: 40 bytes")
        require(session, "sd", "queued input: 18 bytes")
        require(session, "show input", "host-buffer=283")
        require(session, "set throttle 1ips", "throttle=1 ips")
        os.write(session.master, b"continue\n")
        time.sleep(0.1)
        start = time.monotonic()
        os.write(session.master, b"\x05")
        session.until(b"\nsim4000> ")
        assert time.monotonic() - start < 1.0
        require(session, "di", "discarded input: host=283 pending=0 guest=31")
        status = require(session, "show input", "host-buffer=0")
        assert "guest-queue=0" in status and "pending-byte=0" in status, status
        require(session, "discard-input", "discarded input: host=0 pending=0 guest=0")
        # A new paste after freeing the buffer must still work and be discardable.
        os.write(session.master, b"continue\n")
        time.sleep(0.1)
        os.write(session.master, b"y" * 64 + b"\x05")
        session.until(b"\nsim4000> ")
        require(session, "show input", "host-buffer=33")
        require(session, "discard-input", "discarded input: host=33 pending=0 guest=31")
        os.write(session.master, b"continue\n")
        time.sleep(0.1)
        os.write(session.master, b"z" * 256)
        time.sleep(0.1)
        # An out-of-band host signal must remain responsive even when the
        # guest intentionally never consumes the queued input.
        start = time.monotonic()
        session.process.send_signal(signal.SIGTERM)
        session.finish(command=None, expected=128 + signal.SIGTERM)
        assert time.monotonic() - start < 1.0
    finally:
        session.close()


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(__file__).resolve().parents[2]
    port, _ = ports()
    with tempfile.TemporaryDirectory(prefix="sim4000-throttle-") as temporary:
        directory = pathlib.Path(temporary)
        config = directory / "RC4000.cfg"
        config.write_text(
            f"MONITOR FILE={root / 'src/monitor/MONITOR'}\n"
            "CORESIZE=0,1048576\nTHROTTLE VALUE=ORIGINAL\n"
            f"DATE YEAROFFSET={1988 - time.localtime().tm_year}\n"
            "DEFINE CONSOLE DEVNO=2 INTNO=6,3\n"
            "DEFINE CLOCK DEVNO=3 INTNO=11\n"
            "DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
            f"TELNET DEVNO=4 ADDRESS=127.0.0.1 PORT={port} ATTENTION=CTRL-A\n",
            encoding="ascii")
        syntax = subprocess.run([str(executable), "--check-config", str(config)],
                                capture_output=True, text=True, timeout=10)
        assert syntax.returncode == 0 and "approx. 250000 ips" in syntax.stdout
        (directory / "loop.bin").write_bytes(bytes.fromhex("348000"))
        (directory / "prefixes.bin").write_bytes(
            bytes.fromhex("240000240000348ffc"))
        session = Session(executable, directory)
        try:
            require(session, "show throttle", "original (approx. 250000 ips)")
            require(session, "load-core loop.bin 0x200", "loaded")
            require(session, "deposit ic 0x200", "IC")
            # Contextual completion for the new setting and its value.
            output = session.command(b"set thr\t ori\t\n")
            assert "throttle=original" in output, output
            require(session, "set throttle 1000ips", "throttle=1000 ips")
            count, elapsed = paced_run(session, 0.7)
            assert 0.6 * elapsed * 1000 <= count <= 1.3 * elapsed * 1000 + 10, (
                count, elapsed)
            require(session, "load-core prefixes.bin 0x200", "loaded")
            require(session, "deposit ic 0x200", "IC")
            require(session, "set throttle 3000ips", "throttle=3000 ips")
            count, elapsed = paced_run(session, 0.7)
            # Each interpreter call executes two AM prefixes plus one JL.
            assert 0.6 * elapsed * 1000 <= count <= 1.3 * elapsed * 1000 + 10, (
                count, elapsed)
            require(session, "load-core loop.bin 0x200", "loaded")
            require(session, "deposit ic 0x200", "IC")
            require(session, "set throttle 20%", "throttle=20%")
            cpu_before = cpu_seconds(session.process.pid)
            count, elapsed = paced_run(session, 0.9)
            used = cpu_seconds(session.process.pid) - cpu_before
            assert count > 0 and used / elapsed <= 0.4, (used, elapsed, count)
            print(f"Measured 20% CPU budget: {used / elapsed:.1%} of one host CPU")
            # Invalid settings must not alter the active limiter.
            require(session, "set throttle 0ips", "usage:")
            require(session, "set throttle 101%", "usage:")
            require(session, "show throttle", "throttle=20%")
            require(session, "set throttle 1ips", "throttle=1 ips")
            os.write(session.master, b"run\n")
            time.sleep(0.1)
            remote = Remote(port)
            try:
                # Negotiation progresses during the one-second instruction wait.
                remote.until(b"\xff\xfd\x03")
                remote.send(b"\x01")
                start = time.monotonic()
                os.write(session.master, b"\x05")
                session.until(b"\nsim4000> ")
                assert time.monotonic() - start < 1.0
                require(session, "show telnet", "state=connected")
            finally:
                remote.close()
            require(session, "set throttle off", "throttle=off")
            require(session, "set throttle original", "approx. 250000 ips")
            count, elapsed = paced_run(session, 0.5)
            assert 0.6 * elapsed * 250000 <= count <= 1.3 * elapsed * 250000 + 1500, (
                count, elapsed)
            require(session, "set throttle 1ips", "throttle=1 ips")
            before = calls(session)
            require(session, "step 3", "stopped=yes")
            assert calls(session) - before == 3  # Debugging is not paced.
            os.write(session.master, b"run\n")
            time.sleep(0.1)
            session.process.send_signal(signal.SIGTERM)
            session.finish(command=None, expected=128 + signal.SIGTERM)
        finally:
            session.close()
        blocked_input(executable, directory)
        # Alternative and disabled escape settings must work under backpressure.
        original_config = config.read_text(encoding="ascii")
        for setting, escape in (("CTRL-B", b"\x02"), ("NONE", None)):
            config.write_text(original_config + f"OPERATOR ESCAPE={setting}\n",
                              encoding="ascii")
            session = Session(executable, directory)
            try:
                require(session, "load-core loop.bin 0x200", "loaded")
                require(session, "deposit ic 0x200", "IC")
                require(session, "set throttle 1ips", "throttle=1 ips")
                os.write(session.master, b"run\n")
                time.sleep(0.1)
                # Force buffer growth and more than one bounded scan batch.
                os.write(session.master, b"x" * 8192 + b"\x05")
                time.sleep(0.1)
                while select.select([session.process.stdout], [], [], 0)[0]:
                    session.pending += os.read(session.process.stdout.fileno(), 65536)
                assert b"execution suspended" not in session.pending, session.pending
                assert session.process.poll() is None
                if escape:
                    start = time.monotonic()
                    os.write(session.master, escape)
                    session.until(b"\nsim4000> ")
                    assert time.monotonic() - start < 1.0
                    session.finish()
                else:
                    session.process.send_signal(signal.SIGTERM)
                    session.finish(command=None, expected=128 + signal.SIGTERM)
            finally:
                session.close()
        # Boot the real supplied system with the shipped 50% default, on COW media.
        image = root / "src/disks/UNIT0"
        image_hash = hashlib.sha256(image.read_bytes()).digest()
        text = (root / "src/config/RC4000.cfg").read_text()
        text = text.replace("MONITOR FILE=MONITOR\n",
                            f"MONITOR FILE={root / 'src/monitor/MONITOR'}\n")
        text = text.replace("DEFINE RC433X  DEVNO=15 FILE=UNIT0\n",
                            f"DEFINE RC433X DEVNO=15 FILE={image}\n")
        config.write_text(text, encoding="ascii")
        (directory / "exchange").mkdir()
        session = Session(executable, directory, arguments=("-a",),
                          startup=b"initialize date:")
        try:
            os.write(session.master, b"86 09 30 08 00 00\r")
            session.until(b"pause startproc finis fp")
            os.write(session.master, b"\x05")
            session.until(b"\nsim4000> ")
            require(session, "show throttle", "50% of one host CPU")
            session.finish()
        finally:
            session.close()
            assert hashlib.sha256(image.read_bytes()).digest() == image_hash
    print("Throttle configuration, rate/AM pacing, CPU budget and wait cleanup passed")


if __name__ == "__main__":
    main()
