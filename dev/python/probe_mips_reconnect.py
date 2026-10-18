#!/usr/bin/env python3
"""Check waiting-FP TCP reconnection and known partial input on private media."""

import argparse
import json
from pathlib import Path
import re
import shutil

from probe_mips_account import ROOT, HELLO, digest, verify_fp_reply
from probe_mips_sessions import Terminal
from mips_terminal import verify_interactive_child
from probe_mips_ts import verify_component_listing
from test_mips_manual import Dialogue
from test_mips_printer import cli
from test_operator_acceptance import ports


def verify_endpoint(reply, state, accepted):
    # CLI display may wrap at 80 columns, including inside a field name.
    flat = reply.replace("\r", "").replace("\n", "")
    assert len(re.findall(r"device=4 type=CONSOLE listen=127\.0\.0\.1:\d+ ", flat)) == 1, reply
    assert f" state={state} " in flat, reply
    assert re.search(rf" accepted={accepted} rejected=0 ", flat), reply
    assert "dropped-input=0 dropped-output=0" in flat, reply


def healthy(dialogue):
    listing = dialogue.system("list")
    verify_component_listing(listing, ("tem", "primo", "sos"))
    verify_interactive_child(listing)
    return listing


def verify_completed_partial(reply, marker):
    verify_fp_reply(reply)
    assert len(re.findall(rf"(?m)^ {re.escape(marker)}\r?$", reply)) == 1, reply
    assert "enrolled" not in reply and "from tem" not in reply, reply


def administrative_export(dialogue, destination, name, source="reckeep"):
    dialogue.system("all recover base 0 0 login 0 0 user 0 0 project 0 0 run")
    dialogue.until(b"\x07")
    dialogue.fp(f"{name}=tohost {source}")
    assert (destination / "exchange" / name).read_bytes() == HELLO
    dialogue.send("finis")
    dialogue.until(b"pause recover finis fp")


def run(simulator, destination, partial_input=False):
    media = {"MONITOR": ROOT / "src/monitor/MONITOR",
             "UNIT0.mipsts": ROOT / "src/disks/UNIT0.mipsts",
             "UNIT1.mipsts": ROOT / "src/disks/UNIT1.mipsts",
             "RC4000.mipsts.cfg": ROOT / "src/config/RC4000.mipsts.cfg"}
    sources = {name: digest(path) for name, path in media.items()}
    destination.mkdir()
    (destination / "exchange").mkdir()
    for name, path in media.items():
        shutil.copyfile(path, destination / name)
    port = ports()[0]
    config = destination / "RC4000.mipsts.cfg"
    text = config.read_text(encoding="ascii")
    assert text.count("PORT=4004") == 1 and "PROTECTION MODE=OFF" in text
    config.write_text(text.replace("PORT=4004", f"PORT={port}"), encoding="ascii")
    bases = {name: digest(destination / name) for name in media}
    (destination / "exchange/recinput").write_bytes(HELLO)
    dialogue = None
    terminal = None
    with (destination / "session.log").open("x", encoding="ascii") as log:
        try:
            dialogue = Dialogue(simulator, destination, config.name, b"from s", log)
            dialogue.until(b"message sos started\r\n")
            assert "protection=off " in cli(dialogue, "show cpu")
            dialogue.system("all recover base 0 0 login 0 0 user 0 0 project 0 0 run")
            dialogue.until(b"\x07")
            dialogue.send("lookup recseed reckeep reclost")
            absent = dialogue.until(b"\x07")
            for name in ("recseed", "reckeep", "reclost"):
                assert f"***lookup {name} unknown" in absent, absent
            dialogue.fp("rectmp=fromhost recinput")
            dialogue.fp("recseed=copy rectmp")
            dialogue.fp("scope user recseed")
            dialogue.send("finis")
            dialogue.until(b"pause recover finis fp")
            terminal = Terminal(port, destination / "connection1.bin")
            terminal.connect()
            terminal.login("test", "test")
            terminal.fp("reclost=copy recseed")
            terminal.fp("reckeep=copy recseed")
            terminal.fp("scope user reckeep")
            initial = healthy(dialogue)
            identity = re.findall(r"(?m)^test\s+.*\r?$", initial)
            assert len(identity) == 1, initial
            for cycle in (1, 2):
                if partial_input:
                    prefix = b"message par" if cycle == 1 else b"message wrong"
                    terminal.remote.send(prefix)  # Deliberately no Return.
                    echoed = terminal.until(prefix)
                    assert echoed.endswith(prefix.decode("ascii")), echoed
                verify_endpoint(cli(dialogue, "show telnet"), "connected", cycle)
                terminal.close()  # No guest finis, attention or out command.
                terminal = None
                listing = healthy(dialogue)
                assert re.findall(r"(?m)^test\s+.*\r?$", listing) == identity, listing
                (destination / f"disconnected{cycle}.txt").write_text(listing, encoding="ascii")
                verify_endpoint(cli(dialogue, "show telnet"), "listening", cycle)
                administrative_export(dialogue, destination, f"offline{cycle}")
                terminal = Terminal(port, destination / f"connection{cycle + 1}.bin")
                terminal.connect()
                verify_endpoint(cli(dialogue, "show telnet"), "connected", cycle + 1)
                if partial_input:
                    suffix = b"tial\r\n" if cycle == 1 else b"\x08" * 5 + b"right\r\n"
                    terminal.remote.send(suffix)
                    reply = terminal.until(b"\x07")
                    verify_completed_partial(reply, "partial" if cycle == 1 else "right")
                # Resume the already waiting FP directly, with no new SOS login.
                reply = terminal.fp("message resumed")
                assert len(re.findall(r"(?m)^ resumed\r?$", reply)) == 1, reply
                assert "enrolled" not in reply and "from tem" not in reply, reply
                for name, scope in (("reclost", "temp"), ("reckeep", "user")):
                    reply = terminal.fp(f"lookup {name}")
                    assert re.search(rf"(?m)^{name}\s+=set 1 disc .*; {scope}\r?$", reply), reply
                terminal.absent("recproof")
                terminal.fp("recproof=copy reclost")
                terminal.fp("scope user recproof")
                administrative_export(dialogue, destination, f"tempcopy{cycle}", "recproof")
                terminal.fp("clear user recproof")
                terminal.absent("recproof")
                healthy(dialogue)
            terminal.fp("clear temp reclost")
            terminal.absent("reclost")
            terminal.logout("test")
            listing = dialogue.system("list")
            verify_component_listing(listing, ("tem", "primo", "sos"))
            assert not re.search(r"(?m)^test\s+\d+", listing), listing
            terminal.login("test", "test")
            terminal.absent("reclost")
            terminal.fp("lookup reckeep")
            terminal.fp("clear user reckeep")
            terminal.absent("reckeep")
            terminal.logout("test")
            dialogue.system("all recover base 0 0 login 0 0 user 0 0 project 0 0 run")
            dialogue.until(b"\x07")
            dialogue.fp("clear user recseed")
            for name in ("recseed", "reckeep", "reclost"):
                dialogue.send(f"lookup {name}")
                reply = dialogue.until(b"\x07")
                assert f"***lookup {name} unknown" in reply, reply
            dialogue.send("finis")
            dialogue.until(b"pause recover finis fp")
            dialogue.stop("quit\n")
        finally:
            if terminal:
                terminal.close()
            if dialogue:
                dialogue.close()
            assert {name: digest(path) for name, path in media.items()} == sources
            assert {name: digest(destination / name) for name in media} == bases
    (destination / "result.json").write_text(json.dumps({
        "protection": "off", "profile": "shipped-mipsts", "tcp_disconnect_cycles": 2,
        "clean_prompt_only": not partial_input, "same_job_after_reconnect": True,
        "partial_line_continuation_and_backspace": partial_input,
        "temporary_text_survives": True, "retained_exact_exports_while_disconnected": True,
        "no_new_login_on_reconnect": True, "normal_logout_relogin_checked": True,
        "cleanup_verified": True, "source_sha256": sources,
        "limits": "No unknown/unacknowledged input, in-flight output, hard terminal error or crash recovery claim.",
    }, indent=2) + "\n", encoding="ascii")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new exclusive private workspace")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    parser.add_argument("--partial-input", action="store_true", help="test echoed partial-line continuation and five-character backspace correction")
    args = parser.parse_args()
    run(args.simulator.resolve(), args.destination.resolve(), args.partial_input)
    print("SOS FP passes two TCP reconnect cycles" + (" with echoed partial input." if args.partial_input else " at a clean prompt."))
