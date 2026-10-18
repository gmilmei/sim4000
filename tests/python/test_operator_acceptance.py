#!/usr/bin/env python3
"""Accept the CLI control/debug workflows and live two-terminal operation."""

import hashlib
import os
import pathlib
import re
import signal
import socket
import sys
import tempfile
import time

from test_operator_cli import Session


class Remote:
    def __init__(self, port):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=10)
        self.pending = b""

    def send(self, data):
        self.socket.sendall(data)

    def until(self, marker):
        deadline = time.monotonic() + 10
        while marker not in self.pending:
            self.socket.settimeout(max(0.01, deadline - time.monotonic()))
            try:
                data = self.socket.recv(65536)
            except TimeoutError as error:
                raise AssertionError((marker, self.pending)) from error
            assert data, (marker, self.pending)
            self.pending += data
        end = self.pending.index(marker) + len(marker)
        result, self.pending = self.pending[:end], self.pending[end:]
        return result

    def close(self):
        self.socket.close()


def ports():
    # Reserve both simultaneously, then release just before emulator startup.
    listeners = [socket.socket(), socket.socket()]
    try:
        for listener in listeners:
            listener.bind(("127.0.0.1", 0))
        return [listener.getsockname()[1] for listener in listeners]
    finally:
        for listener in listeners:
            listener.close()


def require(session, command, expected):
    output = session.command(command.encode("ascii") + b"\n")
    assert expected in output, (command, expected, output)
    return output


def wait_status(session, expected):
    deadline = time.monotonic() + 10
    while True:
        output = session.command(b"show telnet\n").replace("\n", "")
        if expected in output:
            return output
        assert time.monotonic() < deadline, (expected, output)


def controls(executable, root, directory):
    (directory / "RC4000.cfg").write_text(
        f"MONITOR FILE={root / 'src/monitor/MONITOR'}\n"
            "CORESIZE=0,1048576\nDEFINE CONSOLE DEVNO=2 INTNO=6,3\n"
        "DEFINE READER DEVNO=0 INTNO=9\nDEFINE PUNCH DEVNO=1 INTNO=12\n",
        encoding="ascii")
    # Independent instruction constants: al w0 123; rs w0 768; jl. -4.
    program = b"".join(word.to_bytes(3, "big")
                       for word in (0x2C007B, 0x5C0300, 0x348FFC))
    (directory / "program.bin").write_bytes(program)
    # Four odd-parity tape characters, with low-six-bit values 1, 2, 3, 4.
    (directory / "reader.bin").write_bytes(b"\x01\x02\x83\x04")
    session = Session(executable, directory)
    try:
        require(session, "load-core program.bin 0x200", "loaded")
        require(session, "deposit ic 0x200", "IC")
        require(session, "deposit 0x300 0", "000000")
        require(session, "deposit 14 0x200", "000200")
        require(session, "break 0x200", "breakpoint set")
        require(session, "start", "execution breakpoint hit at 000200")
        require(session, "show cpu", "W0=000000")
        require(session, "continue", "execution breakpoint hit at 000200")
        require(session, "show cpu", "W0=00007b")
        require(session, "examine 0x300 1", "00007b")
        require(session, "show breakpoints", "000200")
        require(session, "nobreak all", "removed")
        require(session, "watch 0x300", "breakpoint set")
        require(session, "deposit 0x300 0", "000000")
        require(session, "run", "000000 -> 00007b")
        require(session, "show cpu", "IC=000204")
        require(session, "show watchpoints", "000300")
        assert "stopped=yes" in require(session, "step", "IC=000200")
        assert "stopped=yes" in require(session, "trace", "IC=000202")
        require(session, "nowatch all", "removed")
        require(session, "interrupts off", "interrupts=off")
        require(session, "show cpu", "interrupts=off")
        require(session, "interrupts on", "interrupts=on")
        require(session, "show cpu", "interrupts=on")
        require(session, "reset", "machine reset")
        require(session, "examine 10 1", "000202")
        require(session, "deposit 14 0x200", "000200")
        require(session, "break 0x200", "breakpoint set")
        require(session, "start", "execution breakpoint hit at 000200")
        require(session, "show cpu", "reset=no")
        require(session, "stop", "execution is suspended")
        require(session, "attach 0 reader.bin", "mounted")
        require(session, "rewind 0", "rewound")
        require(session, "show media", "reader.bin")
        require(session, "reset", "machine reset")
        require(session, "nobreak all", "removed")
        require(session, "break 0", "breakpoint set")
        require(session, "autoload", "execution breakpoint hit at 000000")
        require(session, "show cpu", "W0=0420c4")
        require(session, "show media", "position=4 characters=4")
        require(session, "detach 0", "detached")
        require(session, "attach 1 punch.bin", "mounted")
        require(session, "detach 1", "detached")
        assert (directory / "punch.bin").read_bytes() == b""
        require(session, "save-core saved.bin 0x200 3", "saved")
        assert (directory / "saved.bin").read_bytes() == program
        session.finish()
    finally:
        session.close()


def live_monitor(executable, root, directory):
    console_port, screen_port = ports()
    image = root / "src/disks/UNIT0"
    original = root / "src/disks/UNIT0.original"
    monitor = root / "src/monitor/MONITOR"
    fixtures = (image, original, monitor)
    before = [hashlib.sha256(path.read_bytes()).digest() for path in fixtures]
    (directory / "RC4000.cfg").write_text(
            f"MONITOR FILE={monitor}\nCORESIZE=0,1048576\n"
        "DEFINE CONSOLE DEVNO=2 INTNO=6,3\nDEFINE CLOCK DEVNO=3 INTNO=11\n"
        "DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
        f"TELNET DEVNO=4 ADDRESS=127.0.0.1 PORT={console_port} ATTENTION=CTRL-A\n"
        "DEFINE RC851 DEVNO=5 INTNO=16,15\n"
        f"TELNET DEVNO=5 ADDRESS=127.0.0.1 PORT={screen_port} ATTENTION=CTRL-A\n"
        f"DEFINE RC433X DEVNO=15 FILE={image}\n", encoding="ascii")
    session = Session(executable, directory, arguments=("-a",),
                      startup=b"initialize date:")
    clients = []
    try:
        os.write(session.master, b"86 09 30 08 00 00\r")
        session.until(b"pause startproc finis fp")
        os.write(session.master, b"\x1b")
        session.until(b"att")
        os.write(session.master, b"s\rnew local size 100000 run\r")
        session.until(b"to local")
        os.write(session.master, b"\x05")
        session.until(b"\nsim4000> ")
        assert "reset=no" in require(session, "show cpu", "stopped=yes")
        require(session, "start", "start ignored: machine is not in reset state")

        # Keep a partially edited Readline command pending during network traffic.
        os.write(session.master, b"show te")
        remote = Remote(console_port)
        clients.append(remote)
        negotiation = remote.until(b"\xff\xfd\x03")
        assert b"\xff\xfb\x01" in negotiation
        remote.send(b"\xff\xfd\x01\xff\xfd\x03\xff\xfb\x03")
        remote.send(b"\x01")  # Remote attention, while CPU is stopped.
        busy = Remote(console_port)
        try:
            busy.until(b"Terminal is already connected.")
            assert busy.socket.recv(4096) in (b"", b"\r\n")
        finally:
            busy.close()
        status = session.command(b"l\t\n").replace("\n", "")
        assert "state=connected" in status and "accepted=1" in status, status
        status = wait_status(session, "rejected=1")
        assert "attention-requests=1" in status, status

        # The second endpoint is independently reachable and speaks ANSI.
        screen = Remote(screen_port)
        clients.append(screen)
        assert b"\x1b[2J\x1b[H" in screen.until(b"\x1b[H")

        os.write(session.master, b"continue\n")
        remote.until(b"att")
        remote.send(b"s\r\nnew remote size 100000 run\r\n")
        remote.until(b"to remote")

        def descriptor(name, send, until):
            send(f"montest internal name.{name}\r".encode("ascii"))
            output = until(b"start of internal proces : core.") + until(b"\x07")
            if isinstance(output, bytes):
                output = output.decode("ascii")
            output = output.replace("\r", "")
            assert "***" not in output and "break" not in output, output
            assert re.search(rf"^name\s*:\s*{name}\s*$", output, re.M), output
            address = int(re.search(r"core\.(\d+)", output)[1])
            bounds = re.search(r"^first,top\s*:\s*(\d+)\s+(\d+)", output, re.M)
            low, high = (int(value) for value in bounds.groups())
            parent = int(re.search(r"^parent descr\s*:\s*(\d+)", output, re.M)[1])
            assert high - low == 100000 and parent != 0, output
            return address, low, high, parent

        local_record = descriptor("local", lambda data: os.write(session.master, data), session.until)
        # Telnet NVT needs CR LF (or CR NUL); a bare CR remains buffered.
        remote_record = descriptor("remote", lambda data: remote.send(data + b"\n"), remote.until)
        assert local_record[0] != remote_record[0]
        assert local_record[2] <= remote_record[1] or remote_record[2] <= local_record[1]
        assert local_record[3] == remote_record[3]
        os.write(session.master, b"\x05")
        session.until(b"\nsim4000> ")

        def core_words(address):
            output = session.command(f"examine {address} 26\n".encode("ascii"))
            return {int(location, 16): int(word, 16) for location, word in
                    re.findall(r"^[ >]([0-9a-f]{6}): ([0-9a-f]{6})$", output, re.M)}

        def check_record(name, record):
            address, low, high, parent = record
            words = core_words(address)
            raw_name = b"".join(words[address + offset].to_bytes(3, "big")
                                for offset in (2, 4, 6, 8)).rstrip(b"\x00 ")
            assert raw_name == name.encode("ascii") and words[address] == 0
            assert (words[address + 22], words[address + 24]) == (low, high)
            assert words[address + 50] == parent

        check_record("local", local_record)
        check_record("remote", remote_record)
        os.write(session.master, b"continue\n")
        remote.send(b"\x01")
        remote.until(b"att")
        remote.until(b"\x07")
        remote.send(b"s\r\n")
        remote.until(b"\x07")
        remote.send(b"proc remote remove\r\n")
        removed = remote.until(b"ready")
        assert b"***" not in removed and b"break" not in removed, removed
        # A completed list confirms removal, not just an interim input prompt.
        remote.send(b"\x01")
        remote.until(b"att")
        remote.until(b"\x07")
        remote.send(b"s\r\n")
        remote.until(b"\x07")
        remote.send(b"list\r\n")
        listing = remote.until(b"ready").decode("ascii").replace("\r", "")
        assert re.search(rf"\blocal\s+{local_record[1]}\s+100000\b", listing), listing
        assert not re.search(r"\bremote\s+\d+\s+100000\b", listing), listing
        assert "***" not in listing and "break" not in listing, listing
        os.write(session.master, b"\x05")
        session.until(b"\nsim4000> ")
        check_record("local", local_record)
        assert core_words(remote_record[0])[remote_record[0] + 2] == 0
        os.write(session.master, b"continue\n")
        remote.send(b"\x01")
        remote.until(b"att")
        remote.until(b"\x07")
        remote.send(b"s\r\n")
        remote.until(b"\x07")
        remote.send(b"new remote size 100000 run\r\n")
        created = remote.until(b"to remote") + remote.until(b"\x07")
        assert b"***" not in created and b"break" not in created, created
        assert descriptor("remote", lambda data: remote.send(data + b"\n"), remote.until) == remote_record
        assert descriptor("local", lambda data: os.write(session.master, data), session.until) == local_record
        os.write(session.master, b"\x05")
        session.until(b"\nsim4000> ")
        check_record("local", local_record)
        check_record("remote", remote_record)
        os.write(session.master, b"continue\n")
        remote.send(b"geo\r\n")
        remote.until(b"dansk/English")
        remote.send(b"e\r\n")
        remote.until(b"ARE YOU READY?")
        os.write(session.master, b"geo\r")
        session.until(b"dansk/English")
        os.write(session.master, b"e\r")
        session.until(b"ARE YOU READY?")
        # Both independently attached processes survive suspension/resumption.
        os.write(session.master, b"\x05")
        session.until(b"\nsim4000> ")
        assert "reset=no" in require(session, "show cpu", "stopped=yes")
        remote.close()
        clients.remove(remote)
        status = wait_status(session, "accepted=1 rejected=1")
        # Distinguish device 4 from the independently connected RC851 endpoint.
        deadline = time.monotonic() + 10
        while "device=4 type=CONSOLE" not in status or \
                "state=listening" not in status.split("device=5")[0]:
            assert time.monotonic() < deadline, status
            status = session.command(b"show telnet\n").replace("\n", "")
        remote = Remote(console_port)
        clients.append(remote)
        remote.until(b"\xff\xfd\x03")
        wait_status(session, "accepted=2")
        # Reconnecting does not reset the guest process or its pending question.
        # Deliberately give an unaccepted answer and verify geo's validation.
        os.write(session.master, b"continue\n")
        remote.send(b"y\r\n")
        remote.until(b"Again please")
        os.write(session.master, b"\x05")
        session.until(b"\nsim4000> ")
        session.finish()
    finally:
        for client in clients:
            client.close()
        session.close()
        assert [hashlib.sha256(path.read_bytes()).digest()
                for path in fixtures] == before

    # Check cleanup in raw guest input mode as well as the Readline prompt.
    for host_signal in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        session = Session(executable, directory, arguments=("-a",),
                          startup=b"initialize date:")
        try:
            session.process.send_signal(host_signal)
            session.finish(command=None, expected=128 + host_signal)
        finally:
            session.close()
    assert [hashlib.sha256(path.read_bytes()).digest()
            for path in fixtures] == before


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="sim4000-acceptance-") as temporary:
        directory = pathlib.Path(temporary)
        controls(executable, root, directory)
        live_monitor(executable, root, directory)
    print("CLI control/debug and live Telnet monitor acceptance passed")


if __name__ == "__main__":
    main()
