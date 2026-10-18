#!/usr/bin/env python3
"""Opt-in simultaneous SOS sessions on exclusive unprotected working copies."""

import argparse
import json
from pathlib import Path
import re
import shutil

from probe_mips_account import (ROOT, HELLO, ALLOCATED_HALFWORDS, prepare,
                                verify_fp_reply, digest)
from prepare_mips_test import configuration
from probe_mips_ts import verify_component_listing
from test_mips_manual import Dialogue
from test_mips_printer import cli
from test_operator_acceptance import Remote, ports


def verify_project_seed(reply):
    verify_fp_reply(reply)
    assert re.search(r"(?m)^devseed\s+=set 1 disc .*; (?:system|project)\r?$", reply), reply
    assert re.search(r"(?m)^\s*;\s+\d+\s+\d+\s+3\s+0\s+100\s*\r?$", reply), reply


def verify_sessions(listing, active=("test", "develop")):
    verify_component_listing(listing, ("tem", "primo", "sos"))
    parent = re.findall(r"(?m)^sos\s+(\d+)\s+300000\s+\d+\s+0\s+(?:run|wait)\s+s\s*\r?$", listing)
    assert len(parent) == 1, listing
    for name, size in (("test", 30208), ("develop", ALLOCATED_HALFWORDS)):
        rows = re.findall(rf"(?m)^{name}\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\w+)\s+(\w+)\s*\r?$", listing)
        if name not in active:
            assert not rows, listing
            continue
        assert len(rows) == 1, listing
        first, actual, key, stops, state, owner = rows[0]
        assert int(actual) == size and int(key) > 0 and stops == "0", listing
        assert state in ("run", "wait", "stop") and owner == "sos", listing
        low, high = int(first), int(first) + size
        assert int(parent[0]) < low < high <= int(parent[0]) + 300000, listing
    # SOS swaps jobs through a reused slot. Overlapping descriptor bounds do
    # not mean both jobs are resident, and are not an isolation failure.


class Terminal:
    def __init__(self, port, evidence):
        self.remote = Remote(port)
        self.wire = evidence.open("xb")

    def until(self, marker):
        raw = self.remote.until(marker)
        self.wire.write(raw)
        self.wire.flush()
        return raw.replace(b"\r\0", b"\r").decode("ascii")

    def connect(self):
        negotiation = self.remote.until(b"\xff\xfd\x03")
        self.wire.write(negotiation)
        self.wire.flush()
        assert b"\xff\xfb\x01" in negotiation
        self.remote.send(b"\xff\xfd\x01\xff\xfd\x03\xff\xfb\x03")

    def login(self, name, password, accepted=True):
        self.remote.send(b"\x01")
        self.until(b"\x07")
        self.remote.send(b"sos\r\n")
        assert self.until(b"\x07").endswith(">\x07")
        self.remote.send(f"run {name} pass {password}\r\n".encode("ascii"))
        if not accepted:
            reply = self.until(b"bad password\r\n")
            assert "***sos: bad password" in reply, reply
            return
        reply = self.until(b"\x07")
        verify_fp_reply(reply)
        assert "from tem" in reply, reply
        assert len(re.findall(rf"(?m)^\d{{2}}\.\d{{2}} sos: {name} enrolled\r?$", reply)) == 1, reply

    def fp(self, command, success=True):
        self.remote.send(command.encode("ascii") + b"\r\n")
        reply = self.until(b"\x07")
        if success:
            verify_fp_reply(reply)
        return reply

    def absent(self, name):
        reply = self.fp(f"lookup {name}", False)
        assert len(re.findall(rf"(?m)^\*\*\*lookup {name} unknown\r?$", reply)) == 1, reply
        assert reply.count("***") == 1 and "=set" not in reply, reply

    def logout(self, name):
        self.remote.send(b"finis\r\n")
        reply = self.until(f"{name} removed after finis\r".encode("ascii"))
        verify_fp_reply(reply)

    def close(self):
        self.wire.write(self.remote.pending)
        self.wire.close()
        self.remote.close()


def verify_runtime(simulator, destination):
    frozen = {name: digest(destination / name) for name in ("MONITOR", "UNIT0", "UNIT1")}
    runtime = destination / "sessions"
    runtime.mkdir()
    (runtime / "exchange").mkdir()
    for name in frozen:
        shutil.copyfile(destination / name, runtime / name)
    first, second = ports()
    text = configuration(first)
    text += ("DEFINE RC851 DEVNO=5 INTNO=16,15 PROTOCOL=CLASSIC\n"
             f"TELNET DEVNO=5 ADDRESS=127.0.0.1 PORT={second} ATTENTION=CTRL-A\n")
    (runtime / "RC4000.sessions.cfg").write_text(text, encoding="ascii")
    dialogue = None
    clients = []
    with (runtime / "session.log").open("x", encoding="ascii") as log:
        try:
            dialogue = Dialogue(simulator, runtime, "RC4000.sessions.cfg", b"from s", log)
            dialogue.until(b"message sos started\r\n")
            assert "protection=off " in cli(dialogue, "show cpu")
            for port, name in ((first, "test"), (second, "develop")):
                terminal = Terminal(port, runtime / f"{name}-wire.bin")
                clients.append(terminal)
                terminal.connect()
                terminal.login(name, "wrong", accepted=False)
                terminal.login(name, name)
            demo, develop = clients
            listing = dialogue.system("list")
            verify_sessions(listing)
            (runtime / "both-active.txt").write_text(listing, encoding="ascii")
            for terminal, name in zip(clients, ("test", "develop")):
                (runtime / f"{name}-claim.txt").write_text(terminal.fp("claim"), encoding="ascii")
                for target in ("joint", "shared", "ownfile", "too", "room"):
                    terminal.absent(target)
                seed = terminal.fp("lookup devseed")
                verify_project_seed(seed)
                marker = "testsession" if name == "test" else "devsession"
                reply = terminal.fp(f"message {marker}")
                assert len(re.findall(rf"(?m)^ {marker}\r?$", reply)) == 1, reply
                terminal.fp("joint=copy devseed")
                assert re.search(r"(?m)^joint\s+=set 1 disc .*; temp\r?$", terminal.fp("lookup joint"))
            demo.fp("clear temp joint")
            demo.absent("joint")
            assert re.search(r"(?m)^joint\s+=set 1 disc .*; temp\r?$", develop.fp("lookup joint"))
            rejected = demo.fp("too=set 33 disc", False)
            assert "***" in rejected and "no resources" in rejected, rejected
            demo.absent("too")
            develop.fp("room=set 33 disc")
            room = develop.fp("lookup room")
            assert re.search(r"(?m)^room\s+=set 33 disc .*; temp\r?$", room), room
            develop.fp("clear temp room joint")
            demo.fp("shared=copy devseed")
            demo.fp("scope user shared")
            develop.absent("shared")
            develop.fp("ownfile=copy devseed")
            develop.fp("scope user ownfile")
            demo.absent("ownfile")
            demo.logout("test")
            verify_sessions(dialogue.system("list"), ("develop",))
            develop.fp("message survivor")
            develop.absent("shared")
            assert re.search(r"(?m)^ownfile\s+=set 1 disc .*; user\r?$", develop.fp("lookup ownfile"))
            demo.login("test", "test")
            verify_sessions(dialogue.system("list"))
            demo.absent("joint")
            assert re.search(r"(?m)^shared\s+=set 1 disc .*; user\r?$", demo.fp("lookup shared"))
            demo.fp("clear user shared")
            develop.absent("shared")
            develop.fp("clear user ownfile")
            develop.absent("ownfile")
            develop.logout("develop")
            verify_sessions(dialogue.system("list"), ("test",))
            demo.fp("message secondalive")
            demo.logout("test")
            verify_sessions(dialogue.system("list"), ())
            dialogue.system("all cleanup base 0 0 login 0 0 user 0 0 project 0 100 run")
            dialogue.until(b"\x07")
            dialogue.fp("finalcheck=tohost devseed")
            assert (runtime / "exchange/finalcheck").read_bytes() == HELLO
            dialogue.fp("clear project devseed bigseed")
            dialogue.send("finis")
            dialogue.until(b"pause cleanup finis fp")
            dialogue.stop("quit\n")
        finally:
            for terminal in clients:
                terminal.close()
            if dialogue:
                dialogue.close()
            assert {name: digest(destination / name) for name in frozen} == frozen
            assert {name: digest(runtime / name) for name in frozen} == frozen
    (runtime / "result.json").write_text(json.dumps({
        "protection": "off", "simultaneous_accounts": ["test", "develop"],
        "terminal_devices": [4, 5], "separate_loopback_ports": True,
        "wrong_passwords_rejected": True, "memory_bounds_checked": True,
        "temporary_names_independent": True, "different_quotas_checked": True,
        "distinct_standard_bases": [0, 100], "user_scope_visibility_separate": True,
        "project_seed_visible_to_both": True, "normal_logout_independent": True,
        "demo_relogin_while_develop_active": True, "prepared_bases_unchanged": frozen,
    }, indent=2) + "\n", encoding="ascii")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new exclusive private workspace")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    args = parser.parse_args()
    prepare(args.simulator.resolve(), args.destination.resolve(), two_terminals=True)
    verify_runtime(args.simulator.resolve(), args.destination.resolve())
    print("Two simultaneous unprotected SOS sessions passed.")
