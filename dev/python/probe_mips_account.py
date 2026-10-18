#!/usr/bin/env python3
"""Assess an optional larger SOS account on exclusive unprotected guest media."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from prepare_mips_test import configuration, autostart_text
from probe_mips_ts import (ROOT, TERMINAL_INPUT, ACCOUNT_RESOURCE_CLAUSE,
                           terminal_catalog_bytes, private_sos_start, verify_second_disc,
                           verify_component_listing)
from probe_mips_protection import digest
from test_mips_manual import Dialogue
from test_mips_printer import terminal as demo_terminal, cli
from test_operator_acceptance import Remote, ports

HELLO = (ROOT / "doc/usermanual/examples/hello.alg").read_bytes()
ALLOCATED_HALFWORDS = 150016  # Observed SOS allocation for this fixed maxs-150000 fixture.
LARGE_TEXT = b"Optional SOS account import data.\n" * 1000


def verify_fp_reply(reply):
    assert not any(marker in reply for marker in (
        "***", "break", "giveup", "abnormal end", "syntax error",
        "hard error", "reserve host-file process result", "host-file process not found",
    )), reply


def verify_develop_listing(listing):
    verify_component_listing(listing, ("tem", "primo", "sos"))
    parents = re.findall(r"(?m)^sos\s+(\d+)\s+300000\s+\d+\s+0\s+(?:run|wait)\s+s\s*\r?$", listing)
    children = re.findall(rf"(?m)^develop\s+(\d+)\s+{ALLOCATED_HALFWORDS}\s+3\s+0\s+(?:run|wait|stop)\s+sos\s*\r?$", listing)
    assert len(parents) == len(children) == 1, listing
    assert len(re.findall(r"(?m)^develop\s+\d+", listing)) == 1, listing
    assert int(parents[0]) < int(children[0]) and int(children[0]) + ALLOCATED_HALFWORDS <= int(parents[0]) + 300000, listing


def verify_host_inclusion(reply):
    verify_fp_reply(reply)
    assert reply.startswith("include 63\r\n"), reply
    assert re.fullmatch(r"include 63\r\n\d{2}\.\d{2} sos: ready\r\n", reply), reply


def include_host(dialogue):
    """Use the documented SOS operator command, not monitor memory edits."""
    os.write(dialogue.session.master, b"\x1b")
    dialogue.until(b"att")
    dialogue.until(b"\x07")
    dialogue.send("sos")
    assert dialogue.until(b"\x07").endswith(">\x07")
    dialogue.send("include 63")
    reply = dialogue.until(b"sos: ready\r\n")
    verify_host_inclusion(reply)
    return reply


def develop_session(dialogue, runtime, evidence, port, boot, host_inclusion=False):
    remote = Remote(port)
    with (evidence / "develop-wire.bin").open("xb") as wire:
        def until(marker):
            raw = remote.until(marker)
            wire.write(raw)
            wire.flush()
            return raw.replace(b"\r\0", b"\r").decode("ascii")

        def login(password):
            remote.send(b"\x01")
            until(b"\x07")
            remote.send(b"sos\r\n")
            assert until(b"\x07").endswith(">\x07")
            remote.send(f"run develop pass {password}\r\n".encode("ascii"))

        def fp(command):
            remote.send(command.encode("ascii") + b"\r\n")
            reply = until(b"\x07")
            verify_fp_reply(reply)
            return reply

        try:
            negotiation = remote.until(b"\xff\xfd\x03")
            wire.write(negotiation)
            assert b"\xff\xfb\x01" in negotiation
            remote.send(b"\xff\xfd\x01\xff\xfd\x03\xff\xfb\x03")
            login("wrong")
            assert "***sos: bad password" in until(b"bad password\r\n")
            login("develop")
            enrolled = until(b"\x07")
            assert len(re.findall(r"(?m)^\d{2}\.\d{2} sos: develop enrolled\r?$", enrolled)) == 1 and "from tem" in enrolled and "***" not in enrolled, enrolled
            listing = dialogue.system("list")
            verify_develop_listing(listing)
            (evidence / "develop-active.txt").write_text(listing, encoding="ascii")
            (evidence / "develop-claim.txt").write_text(fp("claim"), encoding="ascii")
            if boot == 1 or host_inclusion:
                remote.send(b"denied=fromhost devinput\r\n")
                denied = until(b"\x07")
                assert "reserve host-file process result 2" in denied, denied
                (evidence / "host-reservation-denied.txt").write_text(denied, encoding="ascii")
                fp("clear temp denied")
            if host_inclusion:
                (evidence / "host-inclusion.txt").write_text(include_host(dialogue), encoding="ascii")
            if boot == 1:
                fp("bigsrc=fromhost biginput" if host_inclusion else "bigsrc=copy bigseed")
                big_descriptor = fp("lookup bigsrc")
                segments = re.findall(r"(?m)^bigsrc\s+=set (\d+) disc .*; temp\r?$", big_descriptor)
                assert len(segments) == 1 and 30 < int(segments[0]) <= 501, big_descriptor
                (evidence / "large-import.txt").write_text(big_descriptor, encoding="ascii")
                if host_inclusion:
                    fp("bigdirect=tohost bigsrc")
                    assert (runtime / "exchange/bigdirect").read_bytes() == LARGE_TEXT
                fp("clear temp bigsrc")
                fp("devsrc=fromhost devinput" if host_inclusion else "devsrc=copy devseed")
                descriptor = fp("lookup devsrc")
                assert re.search(r"(?m)^devsrc\s+=set \d+ disc .*; temp\r?$", descriptor), descriptor
                compiled = fp("devprog=algol devsrc connect.no")
                assert len(re.findall(r"(?m)^algol end \d+\s*\r?$", compiled)) == 1, compiled
                fp("devkeep=copy devsrc")
                (evidence / "compiled-descriptor.txt").write_text(fp("lookup devprog devkeep"), encoding="ascii")
                fp("scope user devkeep devprog")
            executed = fp("devprog")
            assert len(re.findall(r"(?m)^Hello from Algol\r?$", executed)) == 1, executed
            assert len(re.findall(r"(?m)^end\s+\d+\s*\r?$", executed)) == 1, executed
            if host_inclusion:
                fp(f"direct{boot}=tohost devkeep")
                assert (runtime / f"exchange/direct{boot}").read_bytes() == HELLO
            dialogue.system("all inspect base 0 0 login 0 0 user 0 0 project 0 0 run")
            dialogue.until(b"\x07")
            dialogue.fp(f"devcheck{boot}=tohost devkeep")
            assert (runtime / f"exchange/devcheck{boot}").read_bytes() == HELLO
            if boot == 1:
                dialogue.fp("bigcheck=tohost bigseed")
                assert (runtime / "exchange/bigcheck").read_bytes() == LARGE_TEXT
            dialogue.send("finis")
            dialogue.until(b"pause inspect finis fp")
            if boot == 2:
                fp("clear user devkeep devprog")
                for name in ("devkeep", "devprog"):
                    remote.send(f"lookup {name}\r\n".encode("ascii"))
                    absent = until(b"\x07")
                    assert len(re.findall(rf"(?m)^\*\*\*lookup {name} unknown\r?$", absent)) == 1, absent
            remote.send(b"finis\r\n")
            removed = until(b"develop removed after finis\r")
            assert "***" not in removed, removed
            listing = dialogue.system("list")
            verify_component_listing(listing, ("tem", "primo", "sos"))
            assert not re.search(r"(?m)^develop\s+\d+", listing), listing
        finally:
            wire.write(remote.pending)
            remote.close()


def verify_runtime(simulator, destination, host_inclusion=False):
    bases = {name: digest(destination / name) for name in ("MONITOR", "UNIT0", "UNIT1")}
    runtime = destination / "verification"
    runtime.mkdir()
    (runtime / "exchange").mkdir()
    for name in bases:
        shutil.copyfile(destination / name, runtime / name)
    (runtime / "exchange/devinput").write_bytes(HELLO)
    (runtime / "exchange/biginput").write_bytes(LARGE_TEXT)
    port = ports()[0]
    (runtime / "RC4000.mipsts.cfg").write_text(configuration(port), encoding="ascii")
    try:
        for boot in (1, 2):
            evidence = runtime / f"boot{boot}"
            evidence.mkdir()
            dialogue = None
            with (evidence / "session.log").open("x", encoding="ascii") as log:
                try:
                    dialogue = Dialogue(simulator, runtime, "RC4000.mipsts.cfg", b"from s", log)
                    dialogue.until(b"message sos started\r\n")
                    assert "protection=off " in cli(dialogue, "show cpu")
                    demo = evidence / "demo"
                    demo.mkdir()
                    if boot == 1:
                        demo_terminal(dialogue, port, demo)
                        verify_component_listing(dialogue.system("list"), ("tem", "primo", "sos"))
                    develop_session(dialogue, runtime, evidence, port, boot, host_inclusion)
                    if boot == 2:
                        verify_component_listing(dialogue.system("list"), ("tem", "primo", "sos"))
                        demo_terminal(dialogue, port, demo)
                        dialogue.system("all cleanup base 0 0 login 0 0 user 0 0 project 0 0 run")
                        dialogue.until(b"\x07")
                        dialogue.fp("clear user devseed bigseed")
                        for name in ("devseed", "bigseed", "devkeep", "devprog"):
                            dialogue.send(f"lookup {name}")
                            absent = dialogue.until(b"\x07")
                            assert len(re.findall(rf"(?m)^\*\*\*lookup {name} unknown\r?$", absent)) == 1, absent
                        dialogue.send("finis")
                        dialogue.until(b"pause cleanup finis fp")
                    dialogue.stop("quit\n")
                finally:
                    if dialogue:
                        dialogue.close()
            assert {name: digest(runtime / name) for name in bases} == bases
    finally:
        assert {name: digest(destination / name) for name in bases} == bases
    (runtime / "result.json").write_text(json.dumps({
        "protection": "off", "account": "develop", "boots": 2,
        "remote_allocation_exceeds_demo_quota": True, "administrative_import_bytes": len(LARGE_TEXT),
        "direct_sos_host_import": host_inclusion, "host_reservation_before_inclusion": 2,
        "explicit_sos_include_63": host_inclusion,
        "remote_algol_compile_and_run": True,
        "retained_program_runs_after_restart": True, "source_exports_equal": True,
        "cleanup_verified": True, "unchanged_demo_login_before_and_after": True,
        "prepared_bases_unchanged": bases,
    }, indent=2) + "\n", encoding="ascii")


def catalog_text(two_terminals=False):
    demo = TERMINAL_INPUT.replace('\npass "test"', '\npass "test"' +
        ACCOUNT_RESOURCE_CLAUSE.replace("key1 0 0 key2 0 0 key3 0 0", "key1 2 6 key2 2 6 key3 2 6"))
    process, terminal = demo.split("\nterm ")
    bases = "stdbase 100 100\nuserbase 100 100\nmaxbase 0 100\n" if two_terminals else ""
    return (process + '\nproc develop\n' + bases + 'mins 100000\nmaxs 150000\npass "develop"\n'
            'bs disc key0 20 500 key1 4 100 key2 4 100 key3 4 100\nterm ' + terminal + "\n" +
            ('term terminal1 "2" "develop" 60 2\n' if two_terminals else ""))


def verify_catalog(data, descriptor, two_terminals=False):
    """Fixed two-account fixture; not a general SOS catalog writer."""
    assert len(data) == 3 * 768, len(data)
    assert re.search(r"(?m)^soscat\s+=set 3 disc .*; temp\r?$", descriptor), descriptor
    assert data[759:768] == bytes.fromhex("000002 000032 ffffff"), data[759:768].hex()
    directory = {}
    for offset in (0, 15):
        name = data[offset:offset + 12].rstrip(b"\0 ").decode("ascii")
        directory[name] = int.from_bytes(data[offset + 12:offset + 15], "big")
    assert directory == {"test": 1, "develop": 2}, directory
    assert data[30:759] == b"\xff" * 729, "unexpected additional directory records"
    original = terminal_catalog_bytes(30, True)
    assert data[768:1314] == original[768:1314], "demo account changed"
    expected = bytearray(original[768:1314])
    if two_terminals:
        for offset in (3, 6, 9, 12, 18):
            expected[offset:offset + 3] = (100).to_bytes(3, "big")
    expected[21:33] = b"develop" + bytes(5)
    for offset, value in ((33, 100000), (36, 150000), (126, 20), (129, 500),
                          (132, 4), (135, 100), (138, 4), (141, 100), (144, 4), (147, 100)):
        expected[offset:offset + 3] = value.to_bytes(3, "big")
    assert data[1314:1536] == b"\xff" * 222, "unexpected first-account padding"
    assert data[1536:2082] == expected, "larger account differs from declared fixture"
    assert data[2082:2121] == original[1314:1353], "terminal record changed"
    tail = 2121
    if two_terminals:
        terminal = (b"terminal1" + bytes(3) + b"2\0\0" + b"develop" + bytes(5) +
                    bytes.fromhex("03c002") + bytes(9))
        assert data[tail:tail + 39] == terminal, "second terminal record changed"
        tail += 39
    assert data[tail:] == b"\xff" * (len(data) - tail), "unexpected trailing catalog data"


def prepare(simulator, destination, two_terminals=False):
    sources = {"MONITOR": ROOT / "src/monitor/MONITOR", "UNIT0": ROOT / "src/disks/UNIT0"}
    before = {name: digest(path) for name, path in sources.items()}
    destination.mkdir()
    exchange = destination / "exchange"
    exchange.mkdir()
    (exchange / "devinput").write_bytes(HELLO)
    (exchange / "biginput").write_bytes(LARGE_TEXT)
    for name, path in sources.items():
        shutil.copyfile(path, destination / name)
    result = subprocess.run([str(simulator), "--mkdisc", "UNIT1"], cwd=destination,
                            capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result
    (destination / "RC4000.cfg").write_text(configuration(4004, preparation=True), encoding="ascii")
    (exchange / "catinput").write_text(catalog_text(two_terminals), encoding="ascii")
    dialogue = None
    with (destination / "prepare.log").open("x", encoding="ascii") as log:
        try:
            dialogue = Dialogue(simulator, destination, "RC4000.cfg", b"initialize date:", log)
            dialogue.send("86 09 30 08 00 00")
            dialogue.until(b"pause startproc finis fp")
            dialogue.system("all catprep size 150000 run")
            dialogue.until(b"\x07")
            label = dialogue.fp("kitlabel 16 disc1 catdisc1 slow 36 9 2030")
            mounted = dialogue.fp("kiton devno.16")
            verify_second_disc(label, mounted, dialogue.fp("show"))
            dialogue.fp("catspec=fromhost catinput")
            response = dialogue.fp("soscat=upsoscat catspec")
            assert len(re.findall(r"(?m)^end\s+\d+\s*\r?$", response)) == 1, response
            descriptor = dialogue.fp("lookup soscat")
            dialogue.fp("catalogbin=tohost conv.bin soscat")
            verify_catalog((exchange / "catalogbin").read_bytes(), descriptor, two_terminals)
            dialogue.fp("scope user soscat")
            dialogue.fp("oldboot=tohost startarea")
            dialogue.fp("oldsos=tohost startsos")
            (exchange / "mipsboot").write_bytes(autostart_text((exchange / "oldboot").read_bytes()))
            sosboot = private_sos_start((exchange / "oldsos").read_bytes(), 2500, 200, "disc1")
            if two_terminals:
                assert sosboot.count(b"internal 1") == 1, "unexpected SOS child-process claim"
                sosboot = sosboot.replace(b"internal 1", b"internal 2")
            (exchange / "sosboot").write_bytes(sosboot)
            for source, target, export in (("sosboot", "startsos", "pksos"),
                                           ("mipsboot", "startarea", "pkboot")):
                dialogue.fp(f"boottemp=fromhost {source}")
                dialogue.fp(f"{target}=copy boottemp")
                dialogue.fp(f"scope user {target}")
                dialogue.fp("clear temp boottemp")
                dialogue.fp(f"{export}=tohost {target}")
                assert (exchange / export).read_bytes() == (exchange / source).read_bytes()
            dialogue.send("finis")
            dialogue.until(b"pause catprep finis fp")
            project = "0 100" if two_terminals else "0 0"
            dialogue.system(f"all seedprep base 0 0 login 0 0 user 0 0 project {project} size 150000 run")
            dialogue.until(b"\x07")
            for source, target in (("devinput", "devseed"), ("biginput", "bigseed")):
                dialogue.fp(f"seedtmp=fromhost {source}")
                dialogue.fp(f"{target}=copy seedtmp")
                dialogue.fp(f"scope {'project' if two_terminals else 'user'} {target}")
                dialogue.fp("clear temp seedtmp")
            dialogue.send("finis")
            dialogue.until(b"pause seedprep finis fp")
            dialogue.stop("quit\n")
        finally:
            if dialogue:
                dialogue.close()
            assert {name: digest(path) for name, path in sources.items()} == before
    assert not list(destination.glob("*.overlay")), "preparation must not use runtime overlays"
    assert digest(destination / "MONITOR") == before["MONITOR"]
    (destination / "preparation.json").write_text(json.dumps({
        "experimental": True, "protection": "off", "demo_record_unchanged": True,
        "optional_account": "develop", "memory_halfwords": [100000, 150000],
        "second_terminal": two_terminals,
        "catalog_sha256": digest(exchange / "catalogbin"), "source_sha256": before,
        "runtime_verified": False,
    }, indent=2) + "\n", encoding="ascii")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new exclusive private workspace")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    parser.add_argument("--verify", action="store_true", help="test staged inputs/compiler and retention over two boots")
    parser.add_argument("--include-host", action="store_true", help="explicitly include device 63 for enrolled jobs during verification")
    args = parser.parse_args()
    if args.include_host and not args.verify:
        parser.error("--include-host requires --verify")
    prepare(args.simulator.resolve(), args.destination.resolve())
    if args.verify:
        verify_runtime(args.simulator.resolve(), args.destination.resolve(), args.include_host)
    print("Optional account catalog prepared; " + ("two-boot workload passed." if args.verify else "runtime not yet accepted."))
