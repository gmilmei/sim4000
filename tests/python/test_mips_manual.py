#!/usr/bin/env python3
"""Opt-in acceptance of the manual MIPS/TS recipe on exclusive private copies."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from probe_mips_ts import (digest, verify_second_disc, verify_terminal_catalog,
                           read_system_reply, verify_component_listing, verify_sos_startup)
from mips_terminal import (verify_interactive_enrollment, verify_interactive_child,
                           verify_interactive_message, verify_interactive_lookup,
                           verify_interactive_logout)
from test_operator_cli import Session
from test_operator_acceptance import Remote


def blocks():
    text = (ROOT / "doc/MIPS-TS.md").read_text(encoding="ascii")
    found = re.findall(r"<!-- mips-manual: ([a-z-]+) -->\n```text\n(.*?)\n```", text, re.S)
    assert len(found) == len({name for name, _ in found}), "duplicate manual block"
    return {name: contents + "\n" for name, contents in found}


class Dialogue:
    def __init__(self, simulator, directory, config, startup, log):
        self.session = Session(simulator, directory, ("-a", "-c", config), startup=startup)
        self.log = log
        self.record("Startup reached: " + startup.decode("ascii") + "\n")

    def record(self, output):
        self.log.write(output)
        self.log.flush()
        return output

    def until(self, marker, timeout=30):
        return self.record(self.session.until(marker, timeout=timeout))

    def send(self, line):
        os.write(self.session.master, line.encode("ascii") + b"\r")

    def fp(self, line):
        self.send(line)
        result = self.until(b"\x07")
        assert not any(word in result for word in
                       ("***", "giveup", "hard error", "abnormal end", "syntax error")), result
        return result

    def system(self, line):
        os.write(self.session.master, b"\x1b")
        self.until(b"att")
        self.until(b"\x07")
        self.send("s")
        self.until(b"\x07")
        self.send(line)
        return read_system_reply(self.session, self.record)

    def stop(self, command):
        os.write(self.session.master, b"\x05")
        self.until(b"\nsim4000> ")
        self.session.finish(command=command.encode("ascii"))

    def close(self):
        if self.session.pending:
            self.record(self.session.pending.decode("ascii", errors="backslashreplace"))
        self.session.close()


def remote_check(port, commands, dialogue, evidence):
    remote = Remote(port)
    with (evidence / "terminal.log").open("x", encoding="ascii") as log:
        def until(marker):
            output = remote.until(marker).decode("ascii")
            log.write(output)
            log.flush()
            return output

        def select():
            remote.send(b"\x01")
            until(b"att")
            until(b"\x07")
            remote.send(b"sos\r\n")
            assert until(b"\x07").endswith(">\x07")

        try:
            negotiation = remote.until(b"\xff\xfd\x03")
            assert b"\xff\xfb\x01" in negotiation
            remote.send(b"\xff\xfd\x01\xff\xfd\x03\xff\xfb\x03")
            select()
            remote.send(commands["rejected-login"].strip().encode("ascii") + b"\r\n")
            assert "***sos: bad password" in until(b"bad password\r\n")
            select()
            remote.send(commands["accepted-login"].strip().encode("ascii") + b"\r\n")
            verify_interactive_enrollment(until(b"test enrolled\r\n"), "run")
            until(b"\x07")
            listing = dialogue.system(commands["inspect-processes"].strip())
            verify_component_listing(listing, ("tem", "primo", "sos"))
            verify_interactive_child(listing)
            for line, check, marker in zip(commands["remote-session"].splitlines(),
                    (verify_interactive_message, verify_interactive_lookup, verify_interactive_logout),
                    (b"\x07", b"\x07", b"test removed after finis\r\n"), strict=True):
                remote.send(line.encode("ascii") + b"\r\n")
                check(until(marker))
            listing = dialogue.system(commands["inspect-processes"].strip())
            verify_component_listing(listing, ("tem", "primo", "sos"))
            assert not re.search(r"(?m)^test\s+\d+", listing), listing
        finally:
            if remote.pending:
                log.write(remote.pending.decode("ascii", errors="backslashreplace"))
            remote.close()


def run(simulator, destination):
    commands = blocks()
    active = lambda text: [line.strip() for line in text.splitlines()
                           if line.strip() and not line.lstrip().startswith(";")]
    assert active(commands["runtime-config"]) == active(
        (ROOT / "src/config/RC4000.mipsts.cfg").read_text(encoding="ascii")), "runtime config drift"
    protected = [p for directory in (ROOT / "src/disks", ROOT / "src/monitor", ROOT / "src/config")
                 for p in directory.iterdir() if p.is_file()]
    original = {p: digest(p) for p in protected}
    destination.mkdir()  # Never reuse existing preparation, runtime or evidence.
    preparation, prepared, runtime = (destination / name for name in ("prepare", "prepared", "run"))
    for directory in (preparation, prepared, runtime):
        directory.mkdir()
    (preparation / "exchange").mkdir()
    shutil.copyfile(ROOT / "src/disks/UNIT0", preparation / "UNIT0.mipsts")
    shutil.copyfile(ROOT / "src/monitor/MONITOR", preparation / "MONITOR")
    try:
        result = subprocess.run([str(simulator), "--mkdisc", str(preparation / "UNIT1.mipsts")],
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, result
        assert (preparation / "UNIT1.mipsts").stat().st_size == 138240
        (preparation / "RC4000.prepare.cfg").write_text(commands["preparation-config"], encoding="ascii")
        check = subprocess.run([str(simulator), "--check-config", str(preparation / "RC4000.prepare.cfg")],
                               capture_output=True, text=True, timeout=10)
        assert check.returncode == 0 and "protection: off" in check.stdout and "ix=on" in check.stdout
        dialogue = None
        with (destination / "preparation.log").open("x", encoding="ascii") as log:
            try:
                dialogue = Dialogue(simulator, preparation, "RC4000.prepare.cfg", b"initialize date:", log)
                dialogue.send(commands["initial-date"].strip())
                dialogue.until(b"pause startproc finis fp")
                dialogue.system(commands["preparation-process"].strip())
                dialogue.until(b"\x07")
                replies = [dialogue.fp(line) for line in commands["initialize-disc"].splitlines()]
                verify_second_disc(*replies)
                for line in commands["export-originals"].splitlines():
                    dialogue.fp(line)
                exchange = preparation / "exchange"
                assert b"startdate c" in (exchange / "oldboot").read_bytes()
                expected = (exchange / "oldsos").read_text(encoding="ascii").replace("perm disc ", "perm disc1 ")
                normalize = lambda text: re.sub(r"(key|internal)(?=\d)", r"\1 ", text).split()
                assert normalize(expected) == normalize(commands["sos-startup"]), expected
                for name, size in (("oldtem", 11000), ("oldprimo", 16938)):
                    assert re.search(rf"size\s+{size}\b", (exchange / name).read_text(encoding="ascii"))
                for name, block in (("catinput", "catalog-input"), ("sosboot", "sos-startup"),
                                    ("mipsboot", "automatic-startup")):
                    (exchange / name).write_text(commands[block], encoding="ascii")
                replies = {line: dialogue.fp(line) for line in commands["create-catalog"].splitlines()}
                verify_terminal_catalog(replies["soscat=upsoscat catspec"], replies["lookup soscat"],
                                        (exchange / "catalogbin").read_bytes(), 30, True)
                for line in commands["install-startup"].splitlines():
                    if line == "finis":
                        dialogue.send(line)
                        dialogue.until(b"pause mipsprep finis fp")
                    else:
                        dialogue.fp(line)
                for source, exported in (("mipsboot", "newboot"), ("sosboot", "newsos")):
                    assert (exchange / source).read_bytes() == (exchange / exported).read_bytes()
                dialogue.stop(commands["exit-emulator"])
            finally:
                if dialogue:
                    dialogue.close()
        assert not list(preparation.glob("*.overlay")), "preparation unexpectedly used overlays"
        for name in ("MONITOR", "UNIT0.mipsts", "UNIT1.mipsts"):
            shutil.copyfile(preparation / name, prepared / name)
        (prepared / "RC4000.mipsts.cfg").write_text(commands["runtime-config"], encoding="ascii")
        (prepared / "exchange").mkdir()
        frozen = {p.name: digest(p) for p in prepared.iterdir() if p.is_file()}
        for name in frozen:
            shutil.copyfile(prepared / name, runtime / name)
        (runtime / "exchange").mkdir()
        for boot in (1, 2):
            evidence = destination / f"boot{boot}"
            evidence.mkdir()
            dialogue = None
            with (evidence / "boot.log").open("x", encoding="ascii") as log:
                try:
                    dialogue = Dialogue(simulator, runtime, "RC4000.mipsts.cfg", b"from s", log)
                    startup = dialogue.until(b"message sos started\r\n")
                    assert "initialize date:" not in startup and "not allowed" not in startup
                    verify_sos_startup(startup, dialogue.system(commands["inspect-processes"].strip()))
                    remote_check(4004, commands, dialogue, evidence)
                    if boot == 1:
                        (runtime / "exchange/keepinput").write_text(commands["retained-input"], encoding="ascii")
                    dialogue.system(commands["verification-process"].strip())
                    dialogue.until(b"\x07")
                    if boot == 1:
                        for line in commands["create-retained-file"].splitlines():
                            dialogue.fp(line)
                    block = "read-first-boot" if boot == 1 else "read-second-boot"
                    for line in commands[block].splitlines():
                        if line == "finis":
                            dialogue.send(line)
                            dialogue.until(b"pause verify finis fp")
                        else:
                            reply = dialogue.fp(line)
                            if line == "lookup keepsake":
                                assert re.search(r"(?m)^keepsake\s+=set 1 disc .*; user\r?$", reply), reply
                    dialogue.stop(commands["exit-emulator"])
                finally:
                    if dialogue:
                        dialogue.close()
            assert (runtime / f"exchange/keepcheck{boot}").read_bytes() == commands["retained-input"].encode("ascii")
            assert {name: digest(runtime / name) for name in frozen} == frozen
            assert {name: digest(prepared / name) for name in frozen} == frozen
        (destination / "result.json").write_text(json.dumps({
            "manual": "doc/MIPS-TS.md", "manual_sha256": digest(ROOT / "doc/MIPS-TS.md"),
            "blocks_verified": sorted(commands), "generator_called": False,
            "packed_catalog_verified": True, "startup_exports_match": True,
            "boots_verified": 2, "telnet_login_logout_verified": True,
            "wrong_password_rejected": True, "retained_text_verified": True,
            "clean_bases_unchanged": True, "prepared_sha256": frozen,
            "input_sha256": {str(p.relative_to(ROOT)): value for p, value in original.items()},
        }, indent=2) + "\n", encoding="ascii")
    finally:
        assert {p: digest(p) for p in protected} == original, "shipped inputs changed"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new exclusive workspace")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    args = parser.parse_args()
    run(args.simulator.resolve(), args.destination.resolve())
    print("Manual MIPS/TS preparation, two boots, Telnet and persistence passed.")


if __name__ == "__main__":
    main()
