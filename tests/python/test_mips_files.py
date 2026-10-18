#!/usr/bin/env python3
"""Opt-in remote-file recipe on the shipped unprotected MIPS/TS profile."""

import argparse
import json
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from probe_mips_protection import retained_local, retained_session, digest
from probe_mips_ts import verify_component_listing
from test_mips_manual import Dialogue
from test_mips_printer import terminal, cli
from test_operator_acceptance import ports

FILE_TEXT = b"MIPS/TS file retained across sessions.\n"


def manual_recipe():
    """Bind the checked dialogue to the manual's literal command blocks."""
    source = (ROOT / "doc/usermanual/19-mips-ts.tex").read_text(encoding="utf-8")
    found = re.findall(r"% mips-files: ([a-z-]+)\n\\begin\{commandexample\}\{[^\n]*\}\n(.*?)\\end\{commandexample\}", source, re.S)
    assert len(found) == len({name for name, _ in found}), "duplicate manual recipe block"
    blocks = {name: body for name, body in found}
    expected = {
        "input": FILE_TEXT.decode("ascii"),
        "administrator": "all verify base 0 0 login 0 0 user 0 0 project 0 0 run\n",
        "absent": "lookup keepseed keepsake\n",
        "seed": "keeptmp=fromhost keepinput\nkeepseed=copy keeptmp\nscope user keepseed\nfinis\n",
        "create": "keepsake=copy keepseed\nscope user keepsake\nlookup keepsake\nc=copy keepsake\nfinis\n",
        "read": "lookup keepsake\nc=copy keepsake\nfinis\n",
        "export": "lookup keepsake\nkeepcheck1=tohost keepsake\nfinis\n",
        "cleanup": "clear user keepsake\nlookup keepsake\nfinis\n",
    }
    assert blocks == expected, "manual sequence differs from checked remote-file dialogue"
    return sorted(blocks)


def remote_cleanup(send, until):
    send(b"clear user keepsake\r\n")
    reply = until(b"\x07")
    assert "***" not in reply and "break" not in reply, reply
    send(b"lookup keepsake\r\n")
    reply = until(b"\x07")
    assert len(re.findall(r"(?m)^\*\*\*lookup keepsake unknown\r?$", reply)) == 1, reply
    assert reply.count("***") == 1 and "=set" not in reply, reply


def run(simulator, destination):
    recipe = manual_recipe()
    names = {"MONITOR": ROOT / "src/monitor/MONITOR",
             "UNIT0.mipsts": ROOT / "src/disks/UNIT0.mipsts",
             "UNIT1.mipsts": ROOT / "src/disks/UNIT1.mipsts",
             "RC4000.mipsts.cfg": ROOT / "src/config/RC4000.mipsts.cfg"}
    sources = {name: digest(path) for name, path in names.items()}
    destination.mkdir()
    (destination / "exchange").mkdir()
    for name, path in names.items():
        shutil.copyfile(path, destination / name)
    port = ports()[0]
    config = destination / "RC4000.mipsts.cfg"
    text = config.read_text(encoding="ascii")
    assert text.count("PORT=4004") == 1 and "PROTECTION MODE=OFF" in text
    config.write_text(text.replace("PORT=4004", f"PORT={port}"), encoding="ascii")
    bases = {name: digest(destination / name) for name in names}
    try:
        for boot in (1, 2):
            evidence = destination / f"boot{boot}"
            evidence.mkdir()
            dialogue = None
            with (evidence / "session.log").open("x", encoding="ascii") as log:
                try:
                    dialogue = Dialogue(simulator, destination, config.name, b"from s", log)
                    dialogue.until(b"message sos started\r\n")
                    assert "protection=off " in cli(dialogue, "show cpu")
                    verify_component_listing(dialogue.system("list"), ("tem", "primo", "sos"))
                    if boot == 1:
                        retained_local(dialogue, destination, boot, seed=True,
                                       transfer_in="fromhost", transfer_out="tohost", text=FILE_TEXT, check_absent=True)
                    for session in (1, 2):
                        session_evidence = evidence / f"login{session}"
                        session_evidence.mkdir()
                        check = (remote_cleanup if (boot, session) == (2, 2) else
                                 retained_session((boot, session) == (1, 1), FILE_TEXT))
                        terminal(dialogue, port, session_evidence, session_check=check)
                        if session == 1:
                            retained_local(dialogue, destination, boot, transfer_out="tohost", text=FILE_TEXT)
                    if boot == 2:
                        dialogue.system("all verify base 0 0 login 0 0 user 0 0 project 0 0 run")
                        dialogue.until(b"\x07")
                        dialogue.fp("clear user keepseed")
                        for name in ("keepsake", "keepseed"):
                            dialogue.send(f"lookup {name}")
                            reply = dialogue.until(b"\x07")
                            assert len(re.findall(rf"(?m)^\*\*\*lookup {name} unknown\r?$", reply)) == 1, reply
                        dialogue.send("finis")
                        dialogue.until(b"pause verify finis fp")
                    dialogue.stop("quit\n")
                finally:
                    if dialogue:
                        dialogue.close()
            assert {name: digest(destination / name) for name in names} == bases
        assert all((destination / f"exchange/keepcheck{boot}").read_bytes() == FILE_TEXT for boot in (1, 2))
        (destination / "result.json").write_text(json.dumps({
            "protection": "off", "automatic_boots": 2, "normal_remote_sessions": 4,
            "remote_create_retain_read": True, "logout_login_readback": True,
            "exact_exports_each_boot": True, "remote_cleanup_verified": True,
            "administrative_seed_removed": True, "base_images_unchanged": True,
            "retained_text": FILE_TEXT.decode("ascii"), "source_sha256": sources,
            "manual_blocks_verified": recipe,
        }, indent=2) + "\n", encoding="ascii")
    finally:
        assert {name: digest(path) for name, path in names.items()} == sources


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new exclusive private workspace")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    args = parser.parse_args()
    run(args.simulator.resolve(), args.destination.resolve())
    print("Unprotected MIPS/TS remote files: four sessions, two boots, exact readback and cleanup passed.")
