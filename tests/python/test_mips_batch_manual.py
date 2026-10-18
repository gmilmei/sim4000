#!/usr/bin/env python3
"""Opt-in batch jobfile recipe on private shipped unprotected MIPS/TS media."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from probe_mips_protection import digest
from probe_mips_ts import BATCH_JOBS, read_sos_event, verify_batch_job, verify_component_listing
from test_mips_manual import Dialogue
from test_mips_printer import cli, terminal
from test_operator_acceptance import ports


def manual_recipe():
    source = (ROOT / "doc/usermanual/19-mips-ts.tex").read_text(encoding="utf-8")
    found = re.findall(r"% mips-batch: ([a-z-]+)\n\\begin\{commandexample\}\{[^\n]*\}\n(.*?)\\end\{commandexample\}", source, re.S)
    assert len(found) == len({name for name, _ in found}), "duplicate batch manual block"
    blocks = dict(found)
    expected = {
        "input": BATCH_JOBS["message"].decode("ascii"),
        "administrator": "all bprep base 0 0 login 0 0 user 0 0 project 0 0 run\n",
        "absent": "lookup batchjob\n",
        "prepare": "batchtmp=fromhost batchinput\nbatchjob=copy batchtmp\nscope user batchjob\nlookup batchjob\nfinis\n",
        "submit": "batch test jobfile batchjob\n", "password": "test\n",
        "inline": "batch test jobfile batchjob pass test\n",
        "cleanup": "batchcheck=tohost batchjob\nclear user batchjob\nlookup batchjob\nfinis\n",
    }
    assert blocks == expected, "batch manual sequence differs from checked dialogue"
    return sorted(blocks)


def verify_password_frame(kind, reply, inline):
    command = "batch test jobfile batchjob" + (" pass test" if inline else "")
    assert reply.startswith(command + "\r\n"), reply
    if inline:
        assert kind == "response" and ">password" not in reply, reply
    else:
        assert kind == "password" and reply.endswith(">password \x07"), reply
        assert reply.count(">password") == 1, reply


def submit(dialogue, inline):
    os.write(dialogue.session.master, b"\x1b")
    dialogue.until(b"att")
    dialogue.until(b"\x07")
    dialogue.send("sos")
    assert dialogue.until(b"\x07").endswith(">\x07")
    command = "batch test jobfile batchjob" + (" pass test" if inline else "")
    dialogue.send(command)
    kind, reply = read_sos_event(dialogue.session)
    dialogue.record(reply)
    verify_password_frame(kind, reply, inline)
    if not inline:
        dialogue.send("test")
    if "test removed after finis\r\n" not in reply:
        reply += dialogue.until(b"test removed after finis\r\n")
    listing = dialogue.system("list")
    verify_batch_job(reply, listing, "message")
    return reply


def run(simulator, destination):
    recipe = manual_recipe()
    names = {"MONITOR": ROOT / "src/monitor/MONITOR",
             "UNIT0.mipsts": ROOT / "src/disks/UNIT0.mipsts",
             "UNIT1.mipsts": ROOT / "src/disks/UNIT1.mipsts",
             "RC4000.mipsts.cfg": ROOT / "src/config/RC4000.mipsts.cfg"}
    original = {name: digest(path) for name, path in names.items()}
    destination.mkdir()
    exchange = destination / "exchange"
    exchange.mkdir()
    for name, path in names.items():
        shutil.copyfile(path, destination / name)
    port = ports()[0]
    config = destination / "RC4000.mipsts.cfg"
    text = config.read_text(encoding="ascii")
    assert "PROTECTION MODE=OFF" in text and text.count("PORT=4004") == 1
    config.write_text(text.replace("PORT=4004", f"PORT={port}"), encoding="ascii")
    bases = {name: digest(destination / name) for name in names}
    (exchange / "batchinput").write_bytes(BATCH_JOBS["message"])
    dialogue = None
    with (destination / "session.log").open("x", encoding="ascii") as log:
        try:
            dialogue = Dialogue(simulator, destination, config.name, b"from s", log)
            dialogue.until(b"message sos started\r\n")
            assert "protection=off " in cli(dialogue, "show cpu")
            verify_component_listing(dialogue.system("list"), ("tem", "primo", "sos"))
            dialogue.system("all bprep base 0 0 login 0 0 user 0 0 project 0 0 run")
            dialogue.until(b"\x07")
            dialogue.send("lookup batchjob")
            absent = dialogue.until(b"\x07")
            assert len(re.findall(r"(?m)^\*\*\*lookup batchjob unknown\r?$", absent)) == 1, absent
            for command in ("batchtmp=fromhost batchinput", "batchjob=copy batchtmp", "scope user batchjob"):
                dialogue.fp(command)
            descriptor = dialogue.fp("lookup batchjob")
            assert re.search(r"(?m)^batchjob\s+=set 1 disc .*; user\r?$", descriptor), descriptor
            dialogue.send("finis")
            dialogue.until(b"pause bprep finis fp")
            for inline in (False, True):
                reply = submit(dialogue, inline)
                (destination / ("batch-inline.txt" if inline else "batch-password.txt")).write_text(reply, encoding="ascii")
            terminal(dialogue, port, destination)
            dialogue.system("all bprep base 0 0 login 0 0 user 0 0 project 0 0 run")
            dialogue.until(b"\x07")
            dialogue.fp("batchcheck=tohost batchjob")
            assert (exchange / "batchcheck").read_bytes() == BATCH_JOBS["message"]
            dialogue.fp("clear user batchjob")
            dialogue.send("lookup batchjob")
            absent = dialogue.until(b"\x07")
            assert len(re.findall(r"(?m)^\*\*\*lookup batchjob unknown\r?$", absent)) == 1, absent
            dialogue.send("finis")
            dialogue.until(b"pause bprep finis fp")
            dialogue.stop("quit\n")
        finally:
            if dialogue:
                dialogue.close()
            assert {name: digest(path) for name, path in names.items()} == original
            assert {name: digest(destination / name) for name in names} == bases
    (destination / "result.json").write_text(json.dumps({
        "protection": "off", "prepared_compact_jobfile": True,
        "password_dialogue_verified": True, "inline_password_verified": True,
        "batch_runs": 2, "message_output_each_run": True, "normal_child_removal_each_run": True,
        "later_terminal_login_logout": True, "jobfile_preserved": True,
        "jobfile_cleanup_verified": True, "base_images_unchanged": True,
        "source_sha256": original,
        "manual_blocks_verified": recipe,
    }, indent=2) + "\n", encoding="ascii")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new exclusive private workspace")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    args = parser.parse_args()
    run(args.simulator.resolve(), args.destination.resolve())
    print("Unprotected MIPS/TS batch: both password forms, output, normal removal and cleanup passed.")
