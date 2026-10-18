#!/usr/bin/env python3
"""Check runtime device rejection and configured terminal interrupt capacity."""

import os
import pathlib
import pty
import re
import subprocess
import sys
import tempfile
import struct

from test_operator_cli import Session


def main() -> None:
    executable = pathlib.Path(sys.argv[1]).resolve()
    cases = (
        ("DEFINE RC433XRAM DEVNO=15 SEGS=10\n", ("RC433XRAM",)),
    )
    with tempfile.TemporaryDirectory(prefix="sim4000-devices-") as directory:
        config = pathlib.Path(directory) / "RC4000.cfg"
        source = pathlib.Path(__file__).resolve().parents[2]
        shipped = (source / "src/config/RC4000.cfg").read_text(encoding="ascii")
        active = "\n".join(line.split(";", 1)[0] for line in shipped.splitlines())
        used_levels = {int(level)
                       for group in re.findall(
                           r"\bINTNO\s*=\s*([0-9]+(?:\s*,\s*[0-9]+)*)",
                           active, re.IGNORECASE)
                       for level in group.split(",")}
        used_devices = {int(device)
                        for device in re.findall(r"\bDEVNO\s*=\s*([0-9]+)",
                                                 active, re.IGNORECASE)}
        free_levels = sorted(set(range(3, 24)) - used_levels)
        free_devices = sorted(set(range(64)) - used_devices)
        assert len(used_levels) == 8 and len(free_levels) == 13
        additions = ""
        for index in range(6):
            first, second = free_levels[index * 2:index * 2 + 2]
            additions += (f"DEFINE CONSOLE DEVNO={free_devices[index]} "
                          f"INTNO={first},{second}\n")
        config.write_text(shipped + additions, encoding="ascii")
        syntax = subprocess.run(
            [str(executable), "--check-config", str(config)],
            capture_output=True, text=True, timeout=10)
        assert syntax.returncode == 0, syntax.stderr
        # Only one level remains. A seventh terminal must reuse an occupied
        # level and is rejected; this is a parser check, not a monitor boot.
        config.write_text(shipped + additions +
                          f"DEFINE CONSOLE DEVNO={free_devices[6]} "
                          f"INTNO={free_levels[-1]},{min(used_levels)}\n",
                          encoding="ascii")
        syntax = subprocess.run(
            [str(executable), "--check-config", str(config)],
            capture_output=True, text=True, timeout=10)
        assert syntax.returncode != 0, syntax.stdout
        assert f"interrupt number {min(used_levels)} is already used" in syntax.stderr
        for declarations, kinds in cases:
            # Missing monitor/media ensure the preflight runs before host I/O.
            config.write_text("MONITOR FILE=missing.monitor\n" + declarations,
                              encoding="ascii")
            syntax = subprocess.run(
                [str(executable), "--check-config", str(config)],
                capture_output=True, text=True, timeout=10)
            assert syntax.returncode == 0, syntax.stderr
            for mode in (("--run", "--steps", "1"), ("-i",), ("-a",)):
                master, slave = pty.openpty()
                try:
                    result = subprocess.run(
                        [str(executable), "-c", str(config), *mode],
                        stdin=slave, capture_output=True, text=True, timeout=10)
                finally:
                    os.close(slave)
                    os.close(master)
                assert result.returncode != 0, (mode, kinds, result.stdout)
                for kind in kinds:
                    expected = (f"device type {kind} is recognized "
                                "but not implemented")
                    assert expected in result.stderr, result.stderr
                assert "missing.monitor" not in result.stderr, result.stderr
                assert "missing.disk" not in result.stderr, result.stderr
                assert "sim4000>" not in result.stdout, result.stdout
        # RC749 is a real runtime device, not just a recognized configuration
        # keyword. Exercise configuration-relative mounting and CLI media control.
        tape = pathlib.Path(directory) / "input.tap"
        contents = struct.pack("<I", 3) + b"abc\0" + struct.pack("<I", 3) + struct.pack("<I", 0)
        tape.write_bytes(contents)
        config.write_text(
            f"MONITOR FILE={source / 'src/monitor/MONITOR'}\n"
            "CORESIZE=0,1048576\nDEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
            "DEFINE RC749 DEVNO=16 INTNO=11 FILE=input.tap ROOT=.\n",
            encoding="ascii")
        syntax = subprocess.run([str(executable), "--check-config", str(config)],
                                capture_output=True, text=True, timeout=10)
        assert syntax.returncode == 0 and "root=." in syntax.stdout, syntax.stderr
        session = Session(executable, directory)
        try:
            output = session.command(b"show media\n")
            assert "type=RC749 mode=read-only state=mounted" in output, output
            assert "records=2" in output, output
            output = session.command(b"rewind 16\n")
            assert "RC749 device 16 rewound" in output, output
            output = session.command(b"detach 16\n")
            assert "tape detached" in output, output
            output = session.command(b"attach 16 input.tap writable\n")
            assert "usage for RC749" in output, output
            output = session.command(b"attach 16 input.tap readonly\n")
            assert "mounted read-only" in output, output
            output = session.command(b"show media\n")
            assert "state=mounted" in output, output
            assert all(len(line) <= 80 for line in output.splitlines()), output
            session.finish()
        finally:
            session.close()
        assert tape.read_bytes() == contents
        # A mixed protocol configuration activates V2 on one terminal only.
        # Execute GET_ATTR through the real CLI/machine to verify runtime wiring.
        config.write_text(
            f"MONITOR FILE={source / 'src/monitor/MONITOR'}\n"
            "CORESIZE=0,1048576\nDEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
            "DEFINE RC851 DEVNO=5 INTNO=18,17 PROTOCOL=V2\n", encoding="ascii")
        session = Session(executable, directory)
        try:
            output = session.command(b"show devices\n")
            assert "device=5 type=RC851 state=attached protocol=v2" in output, output
            classic = next(line for line in output.splitlines() if "device=4 type=" in line)
            assert "protocol=v2" not in classic, output
            for command in (b"deposit 32 0x040145\n", b"deposit W0 80\n",
                            b"deposit IC 32\n", b"step\n"):
                session.command(command)
            output = session.command(b"examine 82\n")
            assert "000801" in output, output
            for command in (b"deposit 82 0x123456\n", b"deposit 32 0x040105\n",
                            b"deposit IC 32\n", b"step\n"):
                session.command(command)
            output = session.command(b"examine 82\n")
            assert "123456" in output, output  # Classic CONTROL never writes attributes.
            session.finish()
        finally:
            session.close()
    print("Runtime device validation passed")


if __name__ == "__main__":
    main()
