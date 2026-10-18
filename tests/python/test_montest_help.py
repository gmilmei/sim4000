#!/usr/bin/env python3
"""Check montest help and limited process/disk fields, never changing commands."""

import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from test_operator_cli import Session


def main():
    simulator = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[2]
    fixtures = (root / "src/monitor/MONITOR", root / "src/disks/UNIT0",
                root / "src/disks/UNIT0.original")
    standard = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
    monitor = standard or fixtures[0]
    if standard:
        fixtures += (standard,)
    before = [hashlib.sha256(path.read_bytes()).digest() for path in fixtures]
    offline = subprocess.check_output(
        [str(simulator), "--list-catalog", str(fixtures[1])], text=True, timeout=10)
    header = dict(re.findall(r"([a-z-]+)=([^ ]+)", offline.splitlines()[0]))
    with tempfile.TemporaryDirectory(prefix="sim4000-montest-help-") as tmp:
        directory = Path(tmp)
        (directory / "RC4000.cfg").write_text(
            f"MONITOR FILE={monitor}\nCORESIZE=0,1048576\n"
            "DEFINE CONSOLE DEVNO=2 INTNO=6,3\nDEFINE CLOCK DEVNO=3 INTNO=11\n"
            + (f"DEFINE DFC403 DEVNO=15 INTNO=23,22\n"
               f"DEFINE RC4819 DFC=15 UNITNO=0 FILE={fixtures[1]}\n" if standard else
               f"DEFINE RC433X DEVNO=15 FILE={fixtures[1]}\n"), encoding="ascii")
        session = Session(simulator, directory, ("-a",), startup=b"initialize date:")
        try:
            os.write(session.master, b"86 09 30 08 00 00\r")
            session.until(b"pause startproc finis fp")
            os.write(session.master, b"\x1b")
            session.until(b"att")
            os.write(session.master, b"s\rnew mtest size 200000 run\r")
            session.until(b"to mtest")
            for topic, expected in (
                ("", ("info <command>", "how to execute")),
                ("internal", ("internal all", "name.<name>", "process descriptions")),
                ("external", ("external all", "devno.<devno>", "process descriptions")),
                ("buf", ("buf all", "sender.<sender>", "message buffers")),
                ("chain", ("chain all", "docname.<docname>", "chaintables")),
                ("typein", ("montest typein", "conversational mode", "repeat", "finis")),
                ("end", ("end", "terminates conversational mode", "returns to fp")),
            ):
                os.write(session.master, f"montest info {topic}\r".encode("ascii"))
                output = session.until(b"call:") + session.until(b"\x07")
                normalized = " ".join(output.split())
                for text in expected:
                    assert text in normalized, (topic, text, output)
                assert "***" not in output and "break" not in output, output
            os.write(session.master, b"lookup montest\r")
            session.until(b"; system")
            session.until(b"\x07")
            # Enter the guest tool's own dialog; info is not an FP command here.
            os.write(session.master, b"montest typein\r")
            session.until(b"montest typein\r\n")
            session.until(b"\x07")
            record_fields = []
            for command in (b"internal name.mtest\r", b"repeat\r"):
                os.write(session.master, command)
                record = (session.until(b"start of internal proces : core.") +
                          session.until(b">\x07")).replace("\r", "")
                assert "***" not in record and "break" not in record, record
                assert re.search(r"\nname\s*:\s*mtest\s*\n", record), record
                address = int(re.search(r"core\.(\d+)", record)[1])
                limits = re.search(r"first,top\s*:\s*(\d+)\s+(\d+)", record)
                low, high = (int(value) for value in limits.groups())
                assert high - low == 200000, record
                record_fields.append((address, low, high))
            assert record_fields[0] == record_fields[1], record_fields
            os.write(session.master, b"finis\r")
            session.until(b"finis\r\n")
            session.until(b"\x07")
            # Finis leaves record navigation, but stays inside montest.
            os.write(session.master, b"info chain\r")
            dialog = session.until(b"call:") + session.until(b"\x07")
            assert "chain all" in dialog and "docname.<docname>" in dialog, dialog
            assert "***" not in dialog and "break" not in dialog, dialog
            os.write(session.master, b"end\r")
            session.until(b"end\r\n")
            session.until(b"\x07")
            # The same lookup must now be handled by FP, not by montest.
            os.write(session.master, b"lookup montest\r")
            session.until(b"; system")
            session.until(b"\x07")
            os.write(session.master, b"montest internal name.mtest\r")
            display = (session.until(b"start of internal proces : core.") +
                       session.until(b"\x07")).replace("\r", "")
            assert "break" not in display and "***" not in display, display
            descriptor = int(re.search(r"core\.(\d+)", display)[1])
            bounds = re.search(r"first,top\s*:\s*(\d+)\s+(\d+)", display)
            first, top = (int(value) for value in bounds.groups())
            assert top - first == 200000, display
            assert (descriptor, first, top) == record_fields[0]
            assert re.search(r"\nname\s*:\s*mtest\s*\n", display), display
            # Return to FP after the display, not a paused guest process.
            os.write(session.master, b"lookup montest\r")
            session.until(b"; system")
            session.until(b"\x07")
            os.write(session.master, b"montest internal all\r")
            listing = (session.until(b"start of internal proces : core.") +
                       session.until(b"\x07")).replace("\r", "")
            assert "break" not in listing and "***" not in listing, listing
            listed = []
            for record in listing.split("start of internal proces : core.")[1:]:
                address = int(re.match(r"\s*(\d+)", record)[1])
                name = " ".join(re.search(r"^name\s*:[ \t]*([^\n]*)", record, re.M)[1].split())
                kind = int(re.search(r"^kind\s*:\s*(\d+)", record, re.M)[1])
                limits = re.search(r"^first,top\s*:\s*(\d+)\s+(\d+)", record, re.M)
                low, high = (int(value) for value in limits.groups())
                parent = int(re.search(r"^parent descr\s*:\s*(\d+)", record, re.M)[1])
                listed.append((address, name, kind, low, high, parent))
            assert len({item[0] for item in listed}) == len(listed), listed
            own = [item for item in listed if item[1] == "mtest"]
            supervisor = [item for item in listed if item[1] == "s"]
            assert len(own) == len(supervisor) == 1, listed
            assert own[0][0] == descriptor and own[0][3:5] == (first, top), own
            assert own[0][5] == supervisor[0][0], (own, supervisor)
            # All traverses descriptor slots, including unused unnamed entries.
            assert any(item[1] == "" and item[3:5] == (0, 0) for item in listed), listed
            os.write(session.master, b"lookup montest\r")
            session.until(b"; system")
            session.until(b"\x07")
            os.write(session.master, b"montest external name.disc\r")
            disk_display = (session.until(b",device number :") +
                            session.until(b"\x07")).replace("\r", "")
            disk_header = re.search(r"core\.\s*(\d+)\s*,device number\s*:\s*(\d+)", disk_display)
            disk_descriptor, device = (int(value) for value in disk_header.groups())
            assert device == 15 and "break" not in disk_display and "***" not in disk_display
            disk_kind = int(re.search(r"\nkind\s*:\s*(\d+)", disk_display)[1])
            assert re.search(r"\nname\s*:\s*disc\s*\n", disk_display), disk_display
            # The tool labels the peripheral +10 field as main, with a warning.
            assert re.search(r"\nmain\s*:\s*960\s*\(!\)", disk_display), disk_display
            os.write(session.master, b"montest external devno.15\r")
            by_device = (session.until(b",device number :") +
                         session.until(b"\x07")).replace("\r", "")
            assert "break" not in by_device and "***" not in by_device, by_device
            selected = re.search(r"core\.\s*(\d+)\s*,device number\s*:\s*(\d+)", by_device)
            assert tuple(int(value) for value in selected.groups()) == (disk_descriptor, device)
            assert re.search(r"\nname\s*:\s*disc\s*\n", by_device), by_device
            assert int(re.search(r"\nkind\s*:\s*(\d+)", by_device)[1]) == disk_kind
            assert re.search(r"\nmain\s*:\s*960\s*\(!\)", by_device), by_device
            os.write(session.master, b"lookup montest\r")
            session.until(b"; system")
            session.until(b"\x07")
            os.write(session.master, b"montest chain docname.disc\r")
            chain = (session.until(b"rel. addr. of claims in i. p.") +
                     session.until(b"\x07")).replace("\r", "")
            for label, field in (("first slice in auxcat", "first-slice"),
                                 ("name of auxcat", "name"),
                                 ("size of auxcat", "segments"),
                                 ("document name", "document"),
                                 ("slice length", "slice-length"),
                                 ("last slice of document", "last-slice")):
                value = re.search(r"^" + re.escape(label) + r"\s*:\s*(\S+)", chain, re.M)
                assert value and value[1] == header[field], (label, header[field], chain)
            assert "break" not in chain and "***" not in chain, chain
            os.write(session.master, b"lookup montest\r")
            session.until(b"; system")
            session.until(b"\x07")
            os.write(session.master, b"\x05")
            session.until(b"\nsim4000> ")
            raw = session.command(f"examine {descriptor} 15\n".encode("ascii"))
            words = {int(address, 16): int(word, 16) for address, word in
                     re.findall(r"^[ >]([0-9a-f]{6}): ([0-9a-f]{6})$", raw, re.M)}
            # Independent offsets from montxt1, not the guest display program.
            assert words[descriptor] == 0  # a10: internal process kind.
            name = b"".join(words[descriptor + offset].to_bytes(3, "big")
                            for offset in (2, 4, 6, 8)).rstrip(b"\x00 ")
            assert name == b"mtest", (name, raw)
            assert words[descriptor + 22] == first and words[descriptor + 24] == top
            for address, name, kind, low, high, parent in listed:
                record_raw = session.command(f"examine {address} 26\n".encode("ascii"))
                record_words = {int(location, 16): int(word, 16) for location, word in
                                re.findall(r"^[ >]([0-9a-f]{6}): ([0-9a-f]{6})$", record_raw, re.M)}
                raw_name = b"".join(record_words[address + offset].to_bytes(3, "big")
                                    for offset in (2, 4, 6, 8))
                decoded_name = " ".join(raw_name.replace(b"\x00", b" ").decode("ascii").split())
                assert decoded_name == name and record_words[address] == kind, record_raw
                assert (record_words[address + 22], record_words[address + 24]) == (low, high)
                assert record_words[address + 50] == parent  # a34: parent descriptor.
            # Claims/state/queue fields can change as montest returns to FP;
            # only stable identity and allocation are compared across snapshots.
            disk_raw = session.command(f"examine {disk_descriptor} 6\n".encode("ascii"))
            disk_words = {int(address, 16): int(word, 16) for address, word in
                          re.findall(r"^[ >]([0-9a-f]{6}): ([0-9a-f]{6})$", disk_raw, re.M)}
            assert disk_words[disk_descriptor] == disk_kind
            disk_name = b"".join(disk_words[disk_descriptor + offset].to_bytes(3, "big")
                                 for offset in (2, 4, 6, 8)).rstrip(b"\x00 ")
            assert disk_name == b"disc"
            assert disk_words[disk_descriptor + 10] == device * 64
            assert "mode=volatile-overlay" in session.command(b"show media\n")
            # An outstanding FP read can precede the system process's output.
            assert "operator interrupt requested" in session.command(b"attention\n")
            os.write(session.master, b"run\n")
            session.until(b"att ")
            session.until(b"\x07")
            os.write(session.master, b"s\r")
            session.until(b"s\r\n")
            session.until(b"\x07")
            os.write(session.master, b"list\r")
            diverted = session.until(b"to mtest") + session.until(b"\x07")
            assert "***" not in diverted and "break" not in diverted, diverted
            # This is only safe because mtest is an idle FP, not another tool.
            os.write(session.master, b"\r")
            report = session.until(b"from s") + session.until(b"ready")
            assert "***" not in report and "break" not in report, report
            assert re.search(rf"\bmtest\s+{first}\s+200000\s+\d+\s+0\s+wait\s+s\b", report), report
            os.write(session.master, b"\x05")
            session.until(b"\nsim4000> ")
            session.command(b"attention\n")
            os.write(session.master, b"run\n")
            session.until(b"att ")
            session.until(b"\x07")
            os.write(session.master, b"mtest\r")
            session.until(b"mtest\r\n")
            session.until(b"\x07")
            os.write(session.master, b"lookup montest\r")
            following = session.until(b"; system") + session.until(b"\x07")
            assert "***" not in following and "break" not in following, following
            os.write(session.master, b"finis\r")
            ended = session.until(b"pause mtest finis fp")
            assert "***" not in ended and "break" not in ended, ended
            os.write(session.master, b"\x05")
            session.until(b"\nsim4000> ")

            def core_record(address):
                output = session.command(f"examine {address} 26\n".encode("ascii"))
                return {int(location, 16): int(word, 16) for location, word in
                        re.findall(r"^[ >]([0-9a-f]{6}): ([0-9a-f]{6})$", output, re.M)}

            def finished_system_list():
                session.command(b"attention\n")
                os.write(session.master, b"run\n")
                session.until(b"att ")
                session.until(b"\x07")
                os.write(session.master, b"s\r")
                session.until(b"s\r\n")
                session.until(b"\x07")
                os.write(session.master, b"list\r")
                output = session.until(b"ready")
                assert "***" not in output and "break" not in output, output
                os.write(session.master, b"\x05")
                session.until(b"\nsim4000> ")
                return output

            # A pause notice is emitted before removal necessarily completes.
            no_mtest = finished_system_list()
            assert not re.search(r"\bmtest\s+\d+\s+200000\b", no_mtest), no_mtest
            released = core_record(descriptor)
            assert released[descriptor + 2] == 0  # Removal clears name(0), not the whole slot.
            assert (released[descriptor + 22], released[descriptor + 24]) == (first, top)
            session.command(b"attention\n")
            os.write(session.master, b"run\n")
            session.until(b"att ")
            session.until(b"\x07")
            os.write(session.master, b"s\r")
            session.until(b"s\r\n")
            session.until(b"\x07")
            os.write(session.master, b"new mpeer size 100000 run\r")
            created = session.until(b"to mpeer") + session.until(b"\x07")
            assert "***" not in created and "break" not in created, created
            os.write(session.master, b"montest internal name.mpeer\r")
            peer = (session.until(b"start of internal proces : core.") +
                    session.until(b"\x07")).replace("\r", "")
            assert "***" not in peer and "break" not in peer, peer
            peer_descriptor = int(re.search(r"core\.(\d+)", peer)[1])
            limits = re.search(r"first,top\s*:\s*(\d+)\s+(\d+)", peer)
            peer_first, peer_top = (int(value) for value in limits.groups())
            assert peer_top - peer_first == 100000 and peer_descriptor == descriptor, peer
            assert re.search(r"\nname\s*:\s*mpeer\s*\n", peer), peer
            parent = int(re.search(r"\nparent descr\s*:\s*(\d+)", peer)[1])
            assert parent == supervisor[0][0], peer
            os.write(session.master, b"\x05")
            session.until(b"\nsim4000> ")
            peer_words = core_record(peer_descriptor)
            assert peer_words[peer_descriptor] == 0
            peer_name = b"".join(peer_words[peer_descriptor + offset].to_bytes(3, "big")
                                 for offset in (2, 4, 6, 8)).rstrip(b"\x00 ")
            assert peer_name == b"mpeer"
            assert (peer_words[peer_descriptor + 22], peer_words[peer_descriptor + 24]) == (peer_first, peer_top)
            assert peer_words[peer_descriptor + 50] == parent
            session.command(b"attention\n")
            os.write(session.master, b"run\n")
            session.until(b"att ")
            session.until(b"\x07")
            os.write(session.master, b"s\r")
            session.until(b"s\r\n")
            session.until(b"\x07")
            os.write(session.master, b"proc mpeer remove\r")
            removed = session.until(b"from s") + session.until(b"ready")
            assert "***" not in removed and "break" not in removed, removed
            os.write(session.master, b"\x05")
            session.until(b"\nsim4000> ")
            no_peer = finished_system_list()
            assert not re.search(r"\bmpeer\s+\d+\s+100000\b", no_peer), no_peer
            released_peer = core_record(peer_descriptor)
            assert released_peer[peer_descriptor + 2] == 0
            assert (released_peer[peer_descriptor + 22], released_peer[peer_descriptor + 24]) == (peer_first, peer_top)
            session.finish()
        finally:
            session.close()
    assert [hashlib.sha256(path.read_bytes()).digest() for path in fixtures] == before
    profile = "STDMON/DFC403" if standard else "MONITOR/RC433X"
    print(f"Montest {profile}: help/dialog, process slots/parents/lifecycle, disk fields, chain header, system-list return and integrity passed")


if __name__ == "__main__":
    main()
