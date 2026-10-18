#!/usr/bin/env python3
"""Exercise live Telnet on an explicit experimental protected monitor/profile."""

import argparse
import hashlib
import os
from pathlib import Path
import re

from test_operator_acceptance import Remote, ports, require, wait_status
from test_operator_cli import Session

ROOT = Path(__file__).resolve().parents[2]
CANDIDATE_SHA256 = "baf14abb99d582ad845478034167265087e1aec9d3785da2bd88cdc78298047b"
PROFILE_SHA256 = "ebc56434088ac0d0a5655924646a45bfd3d739bbb2217f45622c9626641f2483"
LOCAL_TEXT = b"Protected local terminal.\n"
REMOTE_TEXT = b"Protected remote terminal.\n"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def clean(output):
    if isinstance(output, bytes):
        output = output.decode("ascii")
    assert "***" not in output and "break" not in output, output
    return output


def claim(output):
    match = re.search(r"size\s+(\d+)\s+first core\s+(\d+)", clean(output))
    assert match, output
    size, first = map(int, match.groups())
    assert size == 100000 and first >= 8 and first % 2 == 0
    assert first + size < 1048576
    return first, first + size


def verify_exports(exchange):
    for name in ("localout", "localagain", "localfinal"):
        assert (exchange / name).read_bytes() == LOCAL_TEXT, name
    for name in ("remoteout", "recreated", "reconnected"):
        assert (exchange / name).read_bytes() == REMOTE_TEXT, name


def run(executable, candidate, image, directory):
    # Accept only the identified experiment, never an arbitrary personal disk.
    assert digest(candidate) == CANDIDATE_SHA256, "unexpected protected monitor"
    assert digest(image) == PROFILE_SHA256, "unexpected derived system-binding profile"
    media = (candidate, image, ROOT / "src/monitor/MONITOR", ROOT / "src/disks/UNIT0")
    before = {path: digest(path) for path in media}
    directory.mkdir()  # Never reuse logs, guest files or a prior configuration.
    exchange = directory / "exchange"
    exchange.mkdir()
    (exchange / "localin").write_bytes(LOCAL_TEXT)
    (exchange / "remotein").write_bytes(REMOTE_TEXT)
    port = ports()[0]
    (directory / "RC4000.cfg").write_text(
        f"PROTECTION MODE=RC4000\nMONITOR FILE={candidate}\nCORESIZE=0,1048576\n"
        "DEFINE CONSOLE DEVNO=2 INTNO=6,3\nDEFINE CLOCK DEVNO=3 INTNO=11\n"
        "DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
        f"TELNET DEVNO=4 ADDRESS=127.0.0.1 PORT={port} ATTENTION=CTRL-A\n"
        f"DEFINE RC433X DEVNO=15 FILE={image}\n"
        f"DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT={exchange}\n", encoding="ascii")
    session = None
    remote = None
    with (directory / "transcript.log").open("x", encoding="ascii") as log:
        def record(label, output):
            log.write(f"{label}: {output!r}\n")
            log.flush()
            return output

        def local_call(line, expected):
            os.write(session.master, line.encode("ascii") + b"\r")
            output = session.until(expected)
            if expected not in (b"\x07", b"ready"):
                output += session.until(b"\x07")
            return clean(record("local " + line, output))

        def remote_call(line, expected):
            remote.send(line.encode("ascii") + b"\r\n")
            output = remote.until(expected)
            if expected not in (b"\x07", b"ready"):
                output += remote.until(b"\x07")
            return clean(record("remote " + line, output))

        def remote_system():
            remote.send(b"\x01")
            clean(record("remote attention", remote.until(b"att") + remote.until(b"\x07")))
            remote.send(b"s\r\n")
            clean(record("remote select s", remote.until(b"\x07")))

        def suspend():
            os.write(session.master, b"\x05")
            record("operator", session.until(b"\nsim4000> "))
            record("cpu", require(session, "show cpu", "protection=rc4000"))

        def resume():
            os.write(session.master, b"continue\n")

        def inspect(stage, expected, removed=None):
            suspend()
            filename = f"{stage}.bin"
            record("snapshot", require(session, f"save-core {filename} 0 524288", "saved 524288 words"))
            data = (directory / filename).read_bytes()
            assert len(data) == 524288 * 3

            def word(address):
                offset = address // 2 * 3
                return int.from_bytes(data[offset:offset + 3], "big")

            def name_at(address):
                offset = address // 2 * 3
                return data[offset:offset + 12].rstrip(b"\0 ")

            records = {}
            for name, (low, high, expected_key) in expected.items():
                encoded = name.encode("ascii")
                candidates = []
                offset = data.find(encoded)
                while offset >= 0:
                    if offset >= 3 and offset % 3 == 0:
                        address = (offset // 3 - 1) * 2
                        if (address + 52 < 1048576 and word(address) == 0 and
                                name_at(address + 2) == encoded and
                                word(address + 22) == low and word(address + 24) == high):
                            parent = word(address + 50)
                            if parent + 10 < 1048576 and name_at(parent + 2) == b"s":
                                pr_key = word(address + 32)
                                pr, key = pr_key >> 12, pr_key & 4095
                                assert key == expected_key and pr & 128
                                assert not (pr & (128 >> key)), (name, pr, key)
                                candidates.append((address, low, high, parent, pr, key))
                    offset = data.find(encoded, offset + 1)
                assert len(candidates) == 1, (stage, name, candidates)
                records[name] = candidates[0]
                for address in (low, low + 50000, high - 2):
                    output = record("keys", session.command(f"show keys {address}\n".encode("ascii")))
                    assert f"{address:06x}: key={expected_key} " in output, output
            if len(records) == 2:
                one, two = records.values()
                assert one[0] != two[0] and one[3] == two[3]
                assert one[2] <= two[1] or two[2] <= one[1]
                assert one[4] & (128 >> two[5]) and two[4] & (128 >> one[5])
            if removed is not None:
                assert word(removed[0] + 2) == 0, "removed process name remains"
                for address in (removed[1], removed[1] + 50000, removed[2] - 2):
                    output = record("removed keys", session.command(f"show keys {address}\n".encode("ascii")))
                    assert f"{address:06x}: key=0 " in output, output
            record("descriptors " + stage, records)
            resume()
            return records

        try:
            session = Session(executable, directory, arguments=("-a",), startup=b"initialize date:")
            os.write(session.master, b"86 09 30 08 00 00\r")
            record("boot", session.until(b"pause startproc finis fp"))
            suspend()
            record("attention", require(session, "attention", "operator interrupt requested"))
            resume()
            record("attention", session.until(b"att") + session.until(b"\x07"))
            os.write(session.master, b"s\r")
            record("select s", session.until(b"\x07"))
            local_call("new local size 100000 run", b"to local")
            local_bounds = claim(local_call("claim", b"first core"))
            local_call("lsrc=rcin localin", b"bytes")
            local_call("localout=rcout lsrc", b"bytes")

            remote = Remote(port)
            negotiation = record("negotiation", remote.until(b"\xff\xfd\x03"))
            assert b"\xff\xfb\x01" in negotiation
            remote.send(b"\xff\xfd\x01\xff\xfd\x03\xff\xfb\x03")
            remote_system()
            remote_call("new remote size 100000 run", b"to remote")
            remote_bounds = claim(remote_call("claim", b"first core"))
            remote_call("rsrc=rcin remotein", b"bytes")
            remote_call("remoteout=rcout rsrc", b"bytes")
            records = inspect("running", {"local": (*local_bounds, 1), "remote": (*remote_bounds, 2)})

            remote_call("clear temp rsrc", b"\x07")
            remote_system()
            remote_call("proc remote remove", b"ready")
            remote_system()
            listing = remote_call("list", b"ready")
            assert re.search(r"\blocal\s+\d+\s+100000\b", listing), listing
            assert not re.search(r"\bremote\s+\d+\s+100000\b", listing), listing
            surviving = inspect("removed", {"local": (*local_bounds, 1)}, removed=records["remote"])
            assert surviving["local"] == records["local"]
            local_call("localagain=rcout lsrc", b"bytes")

            remote_system()
            remote_call("new remote size 100000 run", b"to remote")
            recreated_bounds = claim(remote_call("claim", b"first core"))
            remote_call("rsrc=rcin remotein", b"bytes")
            remote_call("recreated=rcout rsrc", b"bytes")
            recreated = inspect("recreated", {"local": (*local_bounds, 1), "remote": (*recreated_bounds, 2)})
            assert recreated["local"] == records["local"]

            remote.close()
            remote = None
            suspend()
            record("disconnect", wait_status(session, "state=listening"))
            remote = Remote(port)
            record("reconnect negotiation", remote.until(b"\xff\xfd\x03"))
            status = record("reconnect", wait_status(session, "accepted=2"))
            assert "dropped-input=0 dropped-output=0" in status, status
            resume()
            remote_call("reconnected=rcout rsrc", b"bytes")
            local_call("localfinal=rcout lsrc", b"bytes")
            assert claim(remote_call("claim", b"first core")) == recreated_bounds
            reconnected = inspect("reconnected", {"local": (*local_bounds, 1),
                                                   "remote": (*recreated_bounds, 2)})
            assert reconnected == recreated
            suspend()
            session.finish()
            verify_exports(exchange)
            assert {path: digest(path) for path in media} == before
            (directory / "result.txt").write_text(
                "Protected live Telnet lifecycle and reconnect checkpoint passed.\n"
                "Device 2 local key=1; device 4 Telnet key=2.\n"
                "This is not complete M3 acceptance.\n", encoding="ascii")
        finally:
            if remote is not None:
                remote.close()
            if session is not None:
                session.close()
            assert {path: digest(path) for path in media} == before, "source media changed"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("simulator", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("profile_image", type=Path)
    parser.add_argument("destination", type=Path, help="new private directory; must not exist")
    args = parser.parse_args()
    run(args.simulator.resolve(), args.candidate.resolve(), args.profile_image.resolve(),
        args.destination.resolve())
    print("Protected live Telnet checkpoint passed")


if __name__ == "__main__":
    main()
