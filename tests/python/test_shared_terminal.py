#!/usr/bin/env python3
"""Check two idle ordinary FPs sharing one console, with explicit input routing."""

import hashlib
import os
from pathlib import Path
import re
import select
import sys
import tempfile
import time

from test_operator_cli import Session


def clean(output):
    assert "***" not in output and "break" not in output, output


def settle_output(session):
    # BEL precedes completion of some monitor console bookkeeping. Model
    # interactive turn-taking, retaining late output rather than dropping it.
    for _ in range(20):
        readable, _, _ = select.select([session.process.stdout], [], [], 0.15)
        if not readable:
            return
        data = os.read(session.process.stdout.fileno(), 65536)
        assert data
        session.pending += data
    raise AssertionError("console output did not settle")


def select_process(session, name):
    # Use the explicit operator request rather than timing bare Escape's
    # frontend lookahead against the next guest console read.
    os.write(session.master, b"\x05")
    session.until(b"\nsim4000> ")
    assert "operator interrupt requested" in session.command(b"attention\n")
    os.write(session.master, b"continue\n")
    output = session.until(b"att ") + session.until(b"\x07")
    clean(output)
    os.write(session.master, name.encode("ascii") + b"\r")
    output = session.until(name.encode("ascii") + b"\r\n") + session.until(b"\x07")
    clean(output)
    settle_output(session)
    if os.environ.get("SIM4000_SHARED_DEBUG"):
        print("SELECT", name, repr(output), flush=True)


def reply_frame(session, system):
    markers = (b"\x07", b"ready") if system else (b"\x07",)
    deadline = time.monotonic() + 10
    while True:
        endings = [session.pending.index(marker) + len(marker)
                   for marker in markers if marker in session.pending]
        if endings:
            end = min(endings)
            output, session.pending = session.pending[:end], session.pending[end:]
            return output.decode("ascii")
        remaining = deadline - time.monotonic()
        assert remaining > 0, session.pending
        readable, _, _ = select.select([session.process.stdout], [], [], remaining)
        assert readable, session.pending
        data = os.read(session.process.stdout.fileno(), 65536)
        assert data, (session.process.poll(), session.pending)
        session.pending += data


def call(session, name, command, expected):
    select_process(session, name)
    os.write(session.master, command.encode("ascii") + b"\r")
    output = ""
    bridges = []
    for _ in range(64):
        frame = reply_frame(session, name == "s")
        if os.environ.get("SIM4000_SHARED_DEBUG"):
            print("FRAME", name, command, repr(frame), flush=True)
        clean(frame)
        output += frame
        routed = re.search(r"\bto (one|two)\s", frame)
        complete = expected(output) if callable(expected) else expected in output
        if complete and (name == "s" or routed is None or routed[1] == name):
            return output, bridges
        # Only these two processes are known to be idle ordinary FPs. An empty
        # command completes their pending read; never apply this to another tool.
        if routed is None:
            # A read-completion/output bell can precede the rest of the reply.
            # Only an explicit to-name transition authorizes another input.
            continue
        # The inspected utilities do not request interactive input. A read by
        # either named process here belongs to its outstanding ordinary FP read.
        bridges.append(routed[1])
        settle_output(session)
        os.write(session.master, b"\r")
    raise AssertionError((name, command, expected, output))


def listed_processes(output):
    return {name: int(first) for name, first in re.findall(
        r"\b(one|two)\s+(\d+)\s+100000\s+\d+\s+0\s+(?:run|wait)\s+s\b", output)}


def lookup_complete(output):
    return "; system" in output and re.search(
        r";\s+\d+\s+\d+\s+\d+\s+-?\d+\s+-?\d+\s*\r\n", output)


def snapshot(session, directory, expected):
    os.write(session.master, b"\x05")
    session.until(b"\nsim4000> ")
    assert "saved 524288 words" in session.command(b"save-core state.bin 0 524288\n")
    data = (directory / "state.bin").read_bytes()
    assert len(data) == 524288 * 3

    def word(address):
        offset = address // 2 * 3
        return int.from_bytes(data[offset:offset + 3], "big")

    def name_at(address):
        offset = address // 2 * 3
        return data[offset:offset + 12].rstrip(b"\x00 ")

    records = {}
    for name, first in expected.items():
        encoded = name.encode("ascii")
        candidates = []
        offset = data.find(encoded)
        while offset >= 0:
            # Name begins at descriptor +2; distinguish descriptor records
            # from copies of process names in program/console buffers.
            if offset >= 3 and offset % 3 == 0:
                address = (offset // 3 - 1) * 2
                if (address + 52 <= 1048576 and word(address) == 0 and
                        name_at(address + 2) == encoded and
                        word(address + 22) == first and
                        word(address + 24) - first == 100000):
                    parent = word(address + 50)  # montxt1 a34.
                    if (parent + 10 <= 1048576 and word(parent) == 0 and
                            name_at(parent + 2) == b"s"):
                        candidates.append((address, first, first + 100000, parent))
            offset = data.find(encoded, offset + 1)
        assert len(candidates) == 1, (name, first, candidates)
        records[name] = candidates[0]
    os.write(session.master, b"continue\n")
    return records, word


def main():
    simulator = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[2]
    fixtures = (root / "src/monitor/MONITOR", root / "src/disks/UNIT0",
                root / "src/disks/UNIT0.original")
    before = [hashlib.sha256(path.read_bytes()).digest() for path in fixtures]
    with tempfile.TemporaryDirectory(prefix="sim4000-shared-terminal-") as temporary:
        directory = Path(temporary)
        (directory / "RC4000.cfg").write_text(
            f"MONITOR FILE={fixtures[0]}\nCORESIZE=0,1048576\n"
            "DEFINE CONSOLE DEVNO=2 INTNO=6,3\nDEFINE CLOCK DEVNO=3 INTNO=11\n"
            f"DEFINE RC433X DEVNO=15 FILE={fixtures[1]}\n", encoding="ascii")
        session = Session(simulator, directory, ("-a",), startup=b"initialize date:")
        try:
            os.write(session.master, b"86 09 30 08 00 00\r")
            session.until(b"pause startproc finis fp")
            call(session, "s", "new one size 100000 run", "ready")
            call(session, "one", "lookup slang", lookup_complete)
            listing, _ = call(session, "s", "list", "ready")
            assert set(listed_processes(listing)) == {"one"}, listing
            records, _ = snapshot(session, directory, listed_processes(listing))
            one = records["one"]

            created, bridges = call(session, "s", "new two size 100000 run", "ready")
            assert bridges and set(bridges) == {"one"}, (created, bridges)
            call(session, "two", "lookup slang", lookup_complete)
            listing, _ = call(session, "s", "list", "ready")
            assert set(listed_processes(listing)) == {"one", "two"}, listing
            records, _ = snapshot(session, directory, listed_processes(listing))
            assert records["one"] == one
            two = records["two"]
            assert one[0] != two[0] and one[3] == two[3], (one, two)
            assert one[2] <= two[1] or two[2] <= one[1], (one, two)

            call(session, "s", "proc two remove", "ready")
            listing, _ = call(session, "s", "list", "ready")
            assert set(listed_processes(listing)) == {"one"}, listing
            records, word = snapshot(session, directory, listed_processes(listing))
            assert records["one"] == one
            assert word(two[0] + 2) == 0  # Removed name(0), not a wiped slot.
            call(session, "one", "lookup slang", lookup_complete)

            call(session, "s", "new two size 100000 run", "ready")
            call(session, "two", "lookup slang", lookup_complete)
            listing, _ = call(session, "s", "list", "ready")
            assert set(listed_processes(listing)) == {"one", "two"}, listing
            records, _ = snapshot(session, directory, listed_processes(listing))
            assert records["one"] == one
            recreated = records["two"]
            assert recreated[3] == one[3]
            assert recreated[2] <= one[1] or one[2] <= recreated[1], (one, recreated)

            call(session, "s", "proc two remove", "ready")
            call(session, "s", "proc one remove", "ready")
            listing, _ = call(session, "s", "list", "ready")
            assert not listed_processes(listing), listing
            _, word = snapshot(session, directory, {})
            assert word(one[0] + 2) == 0 and word(recreated[0] + 2) == 0
            os.write(session.master, b"\x05")
            session.until(b"\nsim4000> ")
            session.finish()
        finally:
            session.close()
    assert [hashlib.sha256(path.read_bytes()).digest() for path in fixtures] == before
    print("Shared-console FP routing, concurrent descriptors, remove/recreate and integrity passed")


if __name__ == "__main__":
    main()
