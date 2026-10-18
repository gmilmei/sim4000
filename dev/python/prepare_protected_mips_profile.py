#!/usr/bin/env python3
"""Prepare an experimental protected MIPS/TS candidate, never promote it."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from prepare_mips_test import autostart_text
from probe_mips_protection import (ROOT, Dialogue, digest, create_sos_catalog,
                                   catalog_fixture, prepare_sos, retained_local,
                                   retained_session, verify_persistent_media,
                                   protected_printer)
from probe_mips_ts import verify_component_listing, terminal_catalog_bytes
from test_mips_printer import terminal
from test_operator_acceptance import ports

PRIMARY = "UNIT0.mipsts-protected"
SECONDARY = "UNIT1.mipsts-protected"
CONFIG = "RC4000.mipsts-protected.cfg"
ARTIFACTS = ("MONITOR.protected", PRIMARY, SECONDARY, CONFIG)


def configuration(port):
    assert 1024 <= port <= 65535
    return (
        "; Experimental protected MIPS/TS candidate, not a shipped release profile.\n"
        "; Dummy login test/test; unencrypted Telnet is bound to loopback only.\n"
        "; Use only the matching protected disk pair and independent overlays.\n"
        "PROTECTION MODE=RC4000\nCPU IX=YES\nCORESIZE=0,1048576\n"
        "MONITOR FILE=MONITOR.protected\n"
        "; Control-E enters the CLI; Alt-D inserts the translated current date.\n"
        "OPERATOR ESCAPE=CTRL-E\nOPERATOR DATE=ALT-D\nINPUT DEL=CTRL-H\n"
        "DATE YEAROFFSET=-40\nTHROTTLE VALUE=50%\n"
        "; Autostart sets a fixed 1986-09-30 date, not the current host date.\n"
        "DEFINE CONSOLE DEVNO=2 INTNO=6,3 PROTOCOL=CLASSIC\n"
        "DEFINE CLOCK DEVNO=3 INTNO=11\n"
        "DEFINE RC610 DEVNO=7 INTNO=10 FILE=printer.protected.txt APPEND=YES\n"
        "DEFINE CONSOLE DEVNO=4 INTNO=20,19 PROTOCOL=CLASSIC\n"
        f"TELNET DEVNO=4 ADDRESS=127.0.0.1 PORT={port} ATTENTION=CTRL-A\n"
        "; Keep both sidecars with their exact bases; never mix other profiles.\n"
        f"DEFINE RC433X DEVNO=15 FILE={PRIMARY} OVERLAY={PRIMARY}.overlay\n"
        f"DEFINE RC433X DEVNO=16 FILE={SECONDARY} OVERLAY={SECONDARY}.overlay\n"
        "DEFINE READER DEVNO=0 INTNO=9\nDEFINE PUNCH DEVNO=1 INTNO=12\n"
        "; Protected guest FP uses rcin/rcout, not fromhost/tohost.\n"
        "DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT=exchange\n")


def verify(simulator, destination):
    """Verify copies, preserving pristine candidate bases and config."""
    frozen = {name: digest(destination / name) for name in ARTIFACTS}
    runtime = destination / "verification"
    runtime.mkdir()
    (runtime / "exchange").mkdir()
    for name in ARTIFACTS:
        shutil.copyfile(destination / name, runtime / name)
    port = ports()[0]
    (runtime / CONFIG).write_text(configuration(port), encoding="ascii")
    bases = {name: digest(runtime / name) for name in ARTIFACTS}
    expected_output = b""
    try:
        for boot in (1, 2):
            evidence = runtime / f"boot{boot}"
            evidence.mkdir()
            dialogue = None
            with (evidence / "session.log").open("x", encoding="ascii") as log:
                try:
                    dialogue = Dialogue(simulator, runtime, CONFIG, b"from s", log)
                    startup = dialogue.until(b"message sos started\r\n")
                    assert not any(marker in startup for marker in ("initialize date:", "not allowed", "***")), startup
                    verify_component_listing(dialogue.system("list"), ("tem", "primo", "sos"))
                    os.write(dialogue.session.master, b"\x05")
                    dialogue.until(b"\nsim4000> ")
                    cpu = dialogue.record(dialogue.session.command(b"show cpu\n"))
                    assert "protection=rc4000 " in cpu and "ix=on" in cpu, cpu
                    verify_persistent_media(dialogue.record(dialogue.session.command(b"show media\n")),
                                            PRIMARY, SECONDARY)
                    os.write(dialogue.session.master, b"continue\n")
                    if boot == 1:
                        retained_local(dialogue, runtime, boot, seed=True)
                    terminal(dialogue, port, evidence, session_check=retained_session(boot == 1))
                    retained_local(dialogue, runtime, boot)
                    expected_output = protected_printer(dialogue, runtime, evidence, expected_output)
                    verify_component_listing(dialogue.system("list"), ("tem", "primo", "sos"))
                    dialogue.stop("quit\n")
                finally:
                    if dialogue:
                        dialogue.close()
            assert (runtime / "printer.protected.txt").read_bytes() == expected_output
            assert {name: digest(runtime / name) for name in ARTIFACTS} == bases
    finally:
        assert {name: digest(destination / name) for name in ARTIFACTS} == frozen
    (runtime / "result.json").write_text(json.dumps({
        "experimental": True, "protection": "rc4000", "automatic_boots": 2,
        "remote_file_survives_restart": True, "telnet_login_commands_logout_each_boot": True,
        "local_primo_jobs": 4, "exact_immediate_append_output": True,
        "candidate_artifacts_unchanged": frozen,
    }, indent=2) + "\n", encoding="ascii")


def build(simulator, destination, verify_candidate=False):
    sources = (ROOT / "src/monitor/MONITOR.protected", ROOT / "src/disks/UNIT0.protected",
               ROOT / "src/config/RC4000.protected.cfg", ROOT / "src/monitor/MONITOR",
               ROOT / "src/disks/UNIT0", ROOT / "src/config/RC4000.cfg")
    before = {str(path.relative_to(ROOT)): digest(path) for path in sources}
    destination.mkdir()
    work = destination / "preparation"
    work.mkdir()
    (work / "exchange").mkdir()
    factory = destination / "catalog-factory"
    try:
        create_sos_catalog(simulator, factory, demo=True)
        assert catalog_fixture(factory) == terminal_catalog_bytes(30, True)
        for name, path in (("MONITOR.protected", sources[0]), ("UNIT0.protected", sources[1])):
            shutil.copyfile(path, work / name)
        creation = subprocess.run([str(simulator), "--mkdisc", "UNIT1.protected"],
                                  cwd=work, capture_output=True, text=True, timeout=10)
        assert creation.returncode == 0, creation
        assert (work / "UNIT1.protected").read_bytes() == bytes(180 * 768)
        text = sources[2].read_text(encoding="ascii")
        line = "DEFINE RC433X DEVNO=15 FILE=UNIT0.protected\n"
        assert text.count(line) == 1
        text = text.replace(line, line.rstrip() + " WRITABLE=YES\n")
        text += "\n; Exclusive preparation copies only.\nDEFINE RC433X DEVNO=16 FILE=UNIT1.protected WRITABLE=YES\n"
        (work / "RC4000.protected.cfg").write_text(text, encoding="ascii")
        dialogue = None
        with (work / "session.log").open("x", encoding="ascii") as log:
            try:
                dialogue = Dialogue(simulator, work, "RC4000.protected.cfg", b"initialize date:", log)
                dialogue.send("86 09 30 08 00 00")
                dialogue.until(b"pause startproc finis fp")
                prepare_sos(dialogue, work, factory)
                dialogue.system("all bootprep size 150000 temp disc 500 20 perm disc 200 10 run")
                dialogue.until(b"\x07")
                dialogue.fp("oldboot=rcout startarea")
                boot = autostart_text((work / "exchange/oldboot").read_bytes())
                (work / "exchange/bootinput").write_bytes(boot)
                for source, target, export in (("sosinput", "startsos", "pfsos"),
                                                ("bootinput", "startarea", "pfboot")):
                    dialogue.fp(f"boottemp=rcin {source}")
                    dialogue.fp(f"{target}=copy boottemp")
                    dialogue.fp(f"scope user {target}")
                    dialogue.fp("clear temp boottemp")
                    dialogue.fp(f"{export}=rcout {target}")
                    expected = (work / f"exchange/{source}").read_bytes()
                    assert (work / f"exchange/{export}").read_bytes() == expected
                dialogue.fp("clear user psosboot")
                dialogue.send("finis")
                dialogue.until(b"pause bootprep finis fp")
                dialogue.stop("quit\n")
            finally:
                if dialogue:
                    dialogue.close()
        assert not list(work.glob("*.overlay")), "preparation must not use runtime overlays"
        assert digest(work / "MONITOR.protected") == before["src/monitor/MONITOR.protected"]
        # Freeze only prepared bases before any verification or demo user files.
        for source, target in (("MONITOR.protected", "MONITOR.protected"),
                               ("UNIT0.protected", PRIMARY), ("UNIT1.protected", SECONDARY)):
            shutil.copyfile(work / source, destination / target)
        (destination / CONFIG).write_text(configuration(4004), encoding="ascii")
        (destination / "manifest.json").write_text(json.dumps({
            "experimental": True, "protection": "rc4000", "fixed_boot_date": "1986-09-30",
            "catalog_factory_protection": "off", "catalog_sha256": digest(factory / "exchange/catalogbin"),
            "protected_preparation": True, "source_sha256": before,
            "artifacts": {name: digest(destination / name) for name in ARTIFACTS},
        }, indent=2) + "\n", encoding="ascii")
        if verify_candidate:
            verify(simulator, destination)
    finally:
        assert {str(path.relative_to(ROOT)): digest(path) for path in sources} == before


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new exclusive candidate workspace")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    parser.add_argument("--verify", action="store_true", help="two automatic boots on copies with independent overlays")
    args = parser.parse_args()
    build(args.simulator.resolve(), args.destination.resolve(), args.verify)
    print("Experimental protected MIPS/TS candidate prepared; no source promotion or packaging change.")


if __name__ == "__main__":
    main()
