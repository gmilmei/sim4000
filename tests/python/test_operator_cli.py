#!/usr/bin/env python3
"""Exercise Readline, completion, narrow output, and terminal cleanup."""

import os
import pathlib
import pty
import select
import signal
import subprocess
import sys
import tempfile
import termios
import time


class Session:
    def __init__(self, executable, directory, arguments=("-i",),
                 startup=b"\nsim4000> "):
        self.master, self.slave = pty.openpty()
        self.original = termios.tcgetattr(self.slave)
        self.process = subprocess.Popen(
            [str(executable), *arguments], cwd=directory,
            stdin=self.slave, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env={**os.environ, "TERM": "dumb", "INPUTRC": "/dev/null"})
        self.pending = b""
        try:
            self.until(startup)
        except BaseException:
            self.close()
            raise

    def until(self, marker, timeout=10):
        deadline = time.monotonic() + timeout
        while marker not in self.pending:
            remaining = deadline - time.monotonic()
            assert remaining > 0, (marker, self.pending)
            readable, _, _ = select.select([self.process.stdout], [], [], remaining)
            assert readable, (marker, self.pending)
            data = os.read(self.process.stdout.fileno(), 65536)
            assert data, (self.process.poll(), self.pending,
                          self.process.stderr.read())
            self.pending += data
        end = self.pending.index(marker) + len(marker)
        result, self.pending = self.pending[:end], self.pending[end:]
        return result.decode("ascii")

    def command(self, text):
        os.write(self.master, text)
        result = self.until(b"\nsim4000> ")
        # Completion redisplay can produce an intermediate prompt too.
        while select.select([self.process.stdout], [], [], 0.1)[0]:
            result += os.read(self.process.stdout.fileno(), 65536).decode("ascii")
        return result

    def finish(self, command=b"quit\n", expected=0):
        if command:
            os.write(self.master, command)
        status = self.process.wait(timeout=10)
        errors = self.process.stderr.read().decode("ascii")
        assert status == expected, (status, expected, errors)
        assert termios.tcgetattr(self.slave) == self.original
        assert not errors.replace("\a", ""), errors  # Readline's ambiguity bell.

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait()
        self.process.stdout.close()
        self.process.stderr.close()
        os.close(self.master)
        os.close(self.slave)


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="sim4000-operator-") as temporary:
        directory = pathlib.Path(temporary)
        tape_directory = directory / ("long-path-" + "x" * 100)
        tape_directory.mkdir()
        tape = tape_directory / "reader.bin"
        tape.write_bytes(b"\x00")
        (directory / "RC4000.cfg").write_text(
            f"MONITOR FILE={root / 'src/monitor/MONITOR'}\n"
            "CORESIZE=0,1048576\nDEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
            f"DEFINE READER DEVNO=0 INTNO=9 FILE={tape}\n",
            encoding="ascii")
        session = Session(executable, directory)
        try:
            result = session.command(b"sho\t cp\t\n")
            assert "IC=" in result and "exceptions=" in result, result
            # Up-arrow recalls the completed command; Ctrl-A/Ctrl-E edit it.
            result = session.command(b"\x1b[A\x01\x05\n")
            assert "IC=" in result, result
            result = session.command(b"dep\t W0 0x123456\n")
            assert "123456" in result, result
            result = session.command(b"show cpu\n")
            assert "W0=123456" in result, result
            assert "protection=off PR=80" in result, result
            result = session.command(b"sho\t key\t 1 2\n")
            assert "000000: key=0 PR-protected=yes" in result, result
            assert "000002: key=0 PR-protected=yes" in result, result
            for bad in (b"show keys\n", b"show keys 0 0\n",
                        b"show keys 0 257\n", b"show keys 0 1 extra\n"):
                assert "usage: show keys" in session.command(bad)
            assert "outside installed core" in session.command(b"show keys 0x100000\n")
            result = session.command(b"run\n\x05")
            assert "execution suspended by operator escape" in result, result
            result = session.command(b"sho\t cp\t\n")
            assert "IC=" in result, result  # Readline reinstalled after guest input.
            result = session.command(b"sho\t inp\t\n")
            assert "input: local device=4" in result, result
            result = session.command(b"discard-i\t\n")
            assert "discarded input:" in result, result
            for command in (b"help\n", b"show devices\n", b"show media\n",
                            b"show telnet\n", b"show input\n", b"di\n",
                            b"trace 1\n", b"show cpu\n", b"show keys 0 4\n",
                            b"examine /binary 0 3\n",
                            b"examine /instruction 0 3\n"):
                output = session.command(command)
                for line in output.splitlines():
                    assert len(line) <= 80, (command, len(line), line)
            # File completion is contextual; a completed path remains usable.
            result = session.command(b"save-core snapshot.bin 0 2\n")
            assert (directory / "snapshot.bin").stat().st_size == 6, result
            result = session.command(b"load-core snap\t 0\n")
            assert "loaded" in result, result
            # Candidate listings also obey the narrow output policy.
            result = session.command(b"show \t\t\x15show cpu\n")
            assert "watchpoints" in result and "IC=" in result, result

            # An independent unrolled reader stores each received byte as a word.
            # io w0,dev4/read; io w0,dev4/sense; rs w0,address.
            def reader_program(count):
                return b"".join(word.to_bytes(3, "big")
                                for index in range(count)
                                for word in (0x040102, 0x040100, 0x5C0600 + index * 2))

            (directory / "reader-code.bin").write_bytes(reader_program(33))
            assert "loaded" in session.command(b"load-core reader-code.bin 0x400\n")
            session.command(b"deposit ic 0x400\n")
            session.command(b"interrupts off\n")
            session.command(b"di\n")
            assert "usage: send" in session.command(b"send\n")
            for bad in (b"send 41 ZZ\n", b"send 41 80\n", b"send 41 -1\n"):
                assert "invalid 7-bit" in session.command(bad)
                assert "guest-queue=0" in session.command(b"show input\n")
            assert "queued input: 32 bytes" in session.command(
                b"send " + b" ".join([b"41"] * 32) + b"\n")
            assert "queued input: 1 byte" in session.command(b"se 42\n")
            assert "host-buffer=2" in session.command(b"show input\n")
            session.command(b"step 99\n")
            session.command(b"save-core received.bin 0x600 33\n")
            assert (directory / "received.bin").read_bytes() == \
                b"\x00\x00A" * 32 + b"\x00\x00B"
            assert "host-buffer=0" in session.command(b"show input\n")
            # Explicit send bytes bypass host escape detection and DEL mapping.
            session.command(b"deposit ic 0x400\n")
            assert "queued input: 2 bytes" in session.command(b"send 05 7f\n")
            session.command(b"trace 6\n")
            session.command(b"save-core literal.bin 0x600 2\n")
            assert (directory / "literal.bin").read_bytes() == b"\x00\x00\x05\x00\x00\x7f"
            # Terminal framing never reaches the guest or invokes attention.
            # Queue everything before CPU execution, then use the independent
            # reader to verify the exact filtered bytes, including Delete.
            session.command(b"di\n")
            session.command(b"deposit ic 0x400\n")
            result = session.command(
                b"run\n\x1b[200~A\x1b[AB\x1b[3~C\n"
                b"\xc3\xa4\x1b[201~\x1bOP\x1b[1;5D"
                b"\x1b]0;ignored title\x07\x1bPignored\x1b\\"
                b"\x1b(B\x05")
            assert "rejected 2 non-ASCII byte(s)" in result, result
            result = session.command(b"show input\n")
            assert "guest-queue=5" in result, result
            assert "rejected-non-ascii=2" in result, result
            session.command(b"step 15\n")
            session.command(b"save-core filtered.bin 0x600 5\n")
            assert (directory / "filtered.bin").read_bytes() == \
                b"\x00\x00A\x00\x00B\x00\x00\x08\x00\x00C\x00\x00\x0d"
            # Sequence state survives a delayed/split body. Ctrl-E works
            # even inside an unterminated string and clears decoder state.
            session.command(b"di\n")
            (directory / "loop.bin").write_bytes(bytes.fromhex("348000"))
            session.command(b"load-core loop.bin 0x200\n")
            session.command(b"deposit ic 0x200\n")
            os.write(session.master, b"run\n\x1b[")
            time.sleep(0.1)
            os.write(session.master, b"200~Z\x1b[201~\x1b]unfinished\x05")
            result = session.until(b"\nsim4000> ")
            assert "execution suspended by operator escape" in result, result
            result = session.command(b"show input\n")
            assert "terminal-sequence=none" in result, result
            assert "host-buffer=0" in result, result
            assert "guest-queue=1" in result, result
            session.command(b"di\n")
            session.command(b"deposit ic 0x200\n")
            # Bare Escape remains the historical attention byte. It must not
            # become part of a delayed ANSI sequence or an emulator shortcut.
            assert "queued input: 1 byte" in session.command(b"send 41\n")
            os.write(session.master, b"run\n\x1b")
            time.sleep(0.1)
            os.write(session.master, b"\x05")
            assert "execution suspended by operator escape" in session.until(
                b"\nsim4000> ")
            assert "guest-queue=0" in session.command(b"show input\n")
            session.finish()
        finally:
            session.close()

        config = directory / "RC4000.cfg"
        original_config = config.read_text(encoding="ascii")
        # Always select an invalid historical year, independent of host date.
        config.write_text(original_config +
                          f"DATE YEAROFFSET={1900 - time.localtime().tm_year}\n",
                          encoding="ascii")
        session = Session(executable, directory)
        try:
            assert "queued input: 2 bytes" in session.command(b"send 41 42\n")
            assert "usage: send-date" in session.command(b"send-date extra\n")
            assert "queued input:" not in session.command(b"send-date\n")
            assert select.select([session.process.stderr], [], [], 0)[0]
            errors = os.read(session.process.stderr.fileno(), 65536)
            assert errors.startswith(b"\nhost date ") and \
                errors.endswith(b"is outside the monitor date range\n") and \
                errors.count(b"\n") == 2, errors
            output = session.command(b"show input\n")
            assert "host-buffer=0" in output and "guest-queue=2" in output, output
            session.finish()
        finally:
            session.close()
            config.write_text(original_config, encoding="ascii")

        for host_signal in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            session = Session(executable, directory)
            try:
                # Signal during a partially edited line, not just at an empty prompt.
                os.write(session.master, b"show cp")
                session.process.send_signal(host_signal)
                session.finish(command=None, expected=128 + host_signal)
            finally:
                session.close()

        session = Session(executable, directory)
        try:
            session.finish(command=b"\x04")
        finally:
            session.close()
        config.write_text(original_config + "PROTECTION MODE=RC4000\n", encoding="ascii")
        checked = subprocess.run([str(executable), "--check-config", str(config)],
                                 capture_output=True, text=True, check=True)
        assert "protection: rc4000" in checked.stdout, checked.stdout
        session = Session(executable, directory)
        try:
            assert "protection=rc4000 PR=80" in session.command(b"show cpu\n")
            assert "PR-protected=yes" in session.command(b"show keys 0\n")
            # Literal instruction: ks w2 0x180. This is not a monitor boot.
            session.command(b"deposit W2 7\n")
            session.command(b"deposit 0x100 0xce0180\n")
            session.command(b"deposit IC 0x100\n")
            session.command(b"step 1\n")
            assert "000180: key=7 PR-protected=no" in session.command(b"show keys 0x180\n")
            session.command(b"reset\n")
            assert "protection=rc4000" in session.command(b"show cpu\n")
            assert "key=7" in session.command(b"show keys 0x180\n")
            session.finish()
        finally:
            session.close()
    print("Operator Readline, protection inspection and 80-column checks passed")


if __name__ == "__main__":
    main()
