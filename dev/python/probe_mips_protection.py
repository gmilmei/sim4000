#!/usr/bin/env python3
"""Assess MIPS/TS startup and bounded terminal service on private protected media.

This is not full protected MIPS/TS acceptance or reusable image preparation.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/python"))
from test_mips_manual import Dialogue
from test_protected_catalog import digest
from probe_mips_ts import (verify_primo_reply, verify_second_disc,
                           verify_empty_catalog, private_sos_start, EMPTY_CATALOG,
                           verify_sos_disc1_storage, TERMINAL_INPUT,
                           ACCOUNT_RESOURCE_CLAUSE, terminal_catalog_bytes,
                           verify_terminal_catalog, verify_component_listing)

SIZES = {"tem": 11000, "primo": 16938, "sos": 300000}
KEEP_TEXT = b"Protected SOS retained file.\n"
ISOLATION_TARGET = 8192
ISOLATION_CODE = bytes.fromhex("000000 000000 51800a 501000 2e0001 5e1000 348000 002000")


def verify_isolation_fault(response, core, bounds):
    """Match the reported next IC to the exact assembled forbidden RS program."""
    matches = re.findall(r"(?m)^\*\*\*break 0 (\d+)\r?$", response)
    assert len(matches) == 1 and response.count("***") == 1, response
    assert len(re.findall(r"(?m)^\d{2}\.\d{2} sos: test removed after break\r?$", response)) == 1, response
    pc = int(matches[0])
    first, top = bounds
    # The monitor reports IC after the RS at program-relative address 10.
    base = pc - 12
    assert pc % 2 == 0 and first <= base and base + 16 <= top, (pc, bounds)
    assert len(core) == 524288 * 3
    offset = base // 2 * 3
    # FP owns the first two reserved words; require the actual instruction body.
    assert core[offset + 6:offset + len(ISOLATION_CODE)] == ISOLATION_CODE[6:], "fault PC/code differs"
    return pc


def prepare_isolation(dialogue, destination):
    """Assemble a deliberate forbidden store with installed guest SLANG."""
    source = ("; Deliberately attempt to store into Key-0 monitor memory.\n"
              "s. a1 w.\n0,0\nrl. w1 a0.\nrl w0 x1\nal w2 1\n"
              f"rs w2 x1\njl. 0\na0: {ISOLATION_TARGET}\ne.\n")
    (destination / "exchange/faultsrc").write_text(source, encoding="ascii")
    dialogue.system("all faultprep base 0 0 login 0 0 user 0 0 project 0 0 run")
    dialogue.until(b"\x07")
    dialogue.fp("fsrc=rcin faultsrc")
    assembled = dialogue.fp("badstore=slang fsrc")
    assert "slang ok" in assembled and "break" not in assembled, assembled
    dialogue.fp("scope user badstore")
    dialogue.fp("faultbin=rcout conv.bin badstore")
    assert (destination / "exchange/faultbin").read_bytes().startswith(ISOLATION_CODE), "unexpected SLANG program"
    dialogue.send("finis")
    dialogue.until(b"pause faultprep finis fp")


def isolation_session(dialogue, destination):
    from test_mips_printer import cli
    def check(send, until):
        active = json.loads((destination / "active-processes.json").read_text(encoding="ascii"))
        child = active["processes"]["test"]
        # Fixed installed FP load origin for this tiny fixture. A breakpoint
        # hit, exact code and live registers must validate the prediction.
        store_pc = child["first"] + 1536 + 10
        keys = cli(dialogue, f"show keys {ISOLATION_TARGET} 1")
        assert re.search(r"(?m)^[0-9a-f]{6}: key=0 ", keys), keys
        for phase in ("before", "after"):
            if phase == "after":
                assert "execution breakpoint set" in cli(dialogue, f"break {store_pc}")
                send(b"badstore\r\n")
                try:
                    for attempt in range(64):
                        dialogue.until(f"execution breakpoint hit at {store_pc:06x}".encode("ascii"))
                        dialogue.until(b"\nsim4000> ")
                        cpu = dialogue.record(dialogue.session.command(b"show cpu\n"))
                        assert f"IC={store_pc:06x}" in cpu, cpu
                        if "W1=002000 W2=000001" in cpu:
                            break
                        # FP can visit the same address before loading the
                        # fixture. Resume past those unrelated instructions.
                        if attempt < 63:
                            os.write(dialogue.session.master, b"continue\n")
                    else:
                        raise AssertionError("forbidden store not reached within 64 breakpoint visits")
                    assert "protection=rc4000 PR=e7" in cpu and "monitor=no" in cpu, cpu
                    code_key = dialogue.record(dialogue.session.command(f"show keys {store_pc} 1\n".encode("ascii")))
                    assert re.search(r"(?m)^[0-9a-f]{6}: key=3 ", code_key), code_key
                    saved = dialogue.record(dialogue.session.command(b"save-core fault-instruction.bin 0 524288\n"))
                    assert "saved 524288 words" in saved, saved
                finally:
                    dialogue.record(dialogue.session.command(f"nobreak {store_pc}\n".encode("ascii")))
                    os.write(dialogue.session.master, b"continue\n")
                response = until(b"test removed after break\r")
                (destination / "fault-response.txt").write_text(response, encoding="ascii")
                saved = cli(dialogue, "save-core fault-core.bin 0 524288")
                assert "saved 524288 words" in saved, saved
                pc = verify_isolation_fault(response, (destination / "fault-instruction.bin").read_bytes(),
                                            (child["first"], child["top"]))
                assert pc == store_pc + 2, (pc, store_pc)
                (destination / "fault-context.json").write_text(json.dumps({
                    "reported_next_ic": pc, "store_ic": pc - 2, "target": ISOLATION_TARGET,
                    "assembled_code_verified": True, "fault_inside_child": True,
                    "live_pr": 0xe7, "live_code_key": 3, "target_key": 0,
                    "live_w1": ISOLATION_TARGET, "live_w2": 1,
                    "breakpoint_visits": attempt + 1,
                    "pre_fault_child": child,
                }, indent=2) + "\n", encoding="ascii")
            saved = cli(dialogue, f"save-core sentinel-{phase}.bin {ISOLATION_TARGET} 1")
            assert "saved 1 words" in saved, saved
        before = (destination / "sentinel-before.bin").read_bytes()
        after = (destination / "sentinel-after.bin").read_bytes()
        assert len(before) == 3 and before == after and before != b"\0\0\1", (before, after)
    return check


def verify_persistent_media(output, primary="UNIT0.protected", secondary="UNIT1.protected"):
    """Allow the CLI's intentional 80-column wrapping without losing identities."""
    compact = re.sub(r"\s+", "", output)
    for unit, name in enumerate((primary, secondary)):
        escaped = re.escape(name)
        pattern = (rf"device={15 + unit}type=RC433Xstate=attached"
                   rf"base=\./{escaped}mode=persistent-overlay"
                   rf"overlay-sectors=\d+overlay=\./{escaped}\.overlay")
        assert len(re.findall(pattern, compact)) == 1, output


def retained_session(create, text=KEEP_TEXT):
    """Return a check executed by the actual remote SOS FP, not by Guest-s."""
    def check(send, until):
        def fp(command):
            send(command.encode("ascii") + b"\r\n")
            reply = until(b"\x07")
            assert not any(marker in reply for marker in ("***", "break", "giveup")), reply
            return reply
        if create:
            fp("keepsake=copy keepseed")
            fp("scope user keepsake")
        descriptor = fp("lookup keepsake")
        assert re.search(r"(?m)^keepsake\s+=set 1 disc .*; user\r?$", descriptor), descriptor
        reply = fp("c=copy keepsake")
        assert reply.count(text.decode("ascii").replace("\n", "\r\n")) == 1, reply
    return check


def retained_local(dialogue, destination, boot, seed=False, transfer_in="rcin", transfer_out="rcout", text=KEEP_TEXT, check_absent=False):
    """Create only the shared seed locally; verify the remote-created file exactly."""
    dialogue.system("all verify base 0 0 login 0 0 user 0 0 project 0 0 run")
    dialogue.until(b"\x07")
    if seed:
        if check_absent:
            dialogue.send("lookup keepseed keepsake")
            reply = dialogue.until(b"\x07")
            assert re.findall(r"(?m)^\*\*\*lookup (\w+) unknown\r?$", reply) == ["keepseed", "keepsake"], reply
            assert "=set" not in reply and reply.count("***") == 2, reply
        (destination / "exchange/keepinput").write_bytes(text)
        for command in (f"keeptmp={transfer_in} keepinput", "keepseed=copy keeptmp", "scope user keepseed"):
            dialogue.fp(command)
    else:
        descriptor = dialogue.fp("lookup keepsake")
        assert re.search(r"(?m)^keepsake\s+=set 1 disc .*; user\r?$", descriptor), descriptor
        dialogue.fp(f"keepcheck{boot}={transfer_out} keepsake")
        assert (destination / f"exchange/keepcheck{boot}").read_bytes() == text
    dialogue.send("finis")
    dialogue.until(b"pause verify finis fp")


def verify_restart(simulator, destination, port, bases):
    """Boot a new emulator on the same private bases and persistent sidecars."""
    from test_mips_printer import terminal
    evidence = destination / "boot2"
    evidence.mkdir()
    dialogue = None
    at_cli = False
    with (evidence / "session.log").open("x", encoding="ascii") as log:
        try:
            dialogue = Dialogue(simulator, destination, "RC4000.protected.cfg", b"initialize date:", log)
            dialogue.send("86 09 30 08 00 00")
            dialogue.until(b"pause startproc finis fp")
            dialogue.system("all rsmount size 50000 temp disc 100 10 perm disc 0 0 run")
            dialogue.until(b"\x07")
            mounted = dialogue.fp("kiton devno.16")
            assert re.search(r"disc1\s+mounted on\s+16", mounted), mounted
            dialogue.send("finis")
            dialogue.until(b"pause rsmount finis fp")
            # Never label the second disk or manufacture/import a catalog again.
            for command in ("read starttem", "read startprimo", "read psosboot"):
                dialogue.system(command)
            os.write(dialogue.session.master, b"\x05")
            dialogue.until(b"\nsim4000> ")
            at_cli = True
            dialogue.record(dialogue.session.command(b"step 1000000\n"))
            cpu = dialogue.record(dialogue.session.command(b"show cpu\n"))
            assert "protection=rc4000 " in cpu and "ix=on" in cpu, cpu
            media = dialogue.record(dialogue.session.command(b"show media\n"))
            verify_persistent_media(media)
            os.write(dialogue.session.master, b"continue\n")
            at_cli = False
            verify_component_listing(dialogue.system("list"), tuple(SIZES))
            terminal(dialogue, port, evidence, session_check=retained_session(False))
            retained_local(dialogue, destination, 2)
            dialogue.stop("quit\n")
        except Exception as error:
            if dialogue:
                preparation_failure(dialogue, evidence, error, at_cli)
            raise
        finally:
            if dialogue:
                dialogue.close()
            assert {name: digest(destination / name) for name in bases} == bases
    for name in ("UNIT0.protected.overlay", "UNIT1.protected.overlay"):
        assert (destination / name).stat().st_size > 0
    (destination / "restart-result.json").write_text(json.dumps({
        "protection": "rc4000", "boots": 2, "manual_subsystem_start": True,
        "remote_file_created": True, "remote_file_read_after_restart": True,
        "exact_local_exports_each_boot": True, "telnet_login_logout_each_boot": True,
        "base_images_unchanged": True, "retained_text": KEEP_TEXT.decode("ascii"),
        "overlays": {name: digest(destination / name) for name in
                     ("UNIT0.protected.overlay", "UNIT1.protected.overlay")},
    }, indent=2) + "\n", encoding="ascii")


def catalog_fixture(directory):
    data = (directory / "exchange/catalogbin").read_bytes()
    manifest = json.loads((directory / "catalog-result.json").read_text(encoding="ascii"))
    demo = manifest.get("account_provisioned", False)
    assert type(demo) is bool, manifest
    expected = terminal_catalog_bytes(30, True) if demo else EMPTY_CATALOG
    assert data == expected, "catalog is not the declared verified fixture"
    assert manifest["guest_updater_verified"] is True and manifest["protection"] == "off", manifest
    assert manifest["catalog_sha256"] == digest(directory / "exchange/catalogbin"), manifest
    return data


def create_sos_catalog(simulator, destination, demo=False):
    """Explicit compatibility-only manufacturing; never a protected fallback."""
    names = {"MONITOR": ROOT / "src/monitor/MONITOR", "UNIT0": ROOT / "src/disks/UNIT0",
             "RC4000.cfg": ROOT / "src/config/RC4000.cfg"}
    original = {name: digest(path) for name, path in names.items()}
    destination.mkdir()
    exchange = destination / "exchange"
    exchange.mkdir()
    for name, path in names.items():
        shutil.copyfile(path, destination / name)
    dialogue = None
    log = (destination / "session.log").open("x", encoding="ascii")
    try:
        dialogue = Dialogue(simulator, destination, "RC4000.cfg", b"initialize date:", log)
        dialogue.send("86 09 30 08 00 00")
        dialogue.until(b"pause startproc finis fp")
        os.write(dialogue.session.master, b"\x05")
        dialogue.until(b"\nsim4000> ")
        cpu = dialogue.record(dialogue.session.command(b"show cpu\n"))
        assert "protection=off " in cpu, cpu
        os.write(dialogue.session.master, b"continue\n")
        dialogue.system("all catprep size 150000 temp disc 500 20 perm disc 200 10 run")
        dialogue.until(b"\x07")
        source = (TERMINAL_INPUT.replace('\npass "test"', '\npass "test"' +
                  ACCOUNT_RESOURCE_CLAUSE.replace("key1 0 0 key2 0 0 key3 0 0",
                                                "key1 2 6 key2 2 6 key3 2 6"))
                  if demo else "maxp 4")
        (exchange / "catinput").write_text(source + "\n", encoding="ascii")
        dialogue.fp("catspec=fromhost catinput")
        response = dialogue.fp("soscat=upsoscat catspec")
        descriptor = dialogue.fp("lookup soscat")
        dialogue.fp("catalogbin=tohost conv.bin soscat")
        data = (exchange / "catalogbin").read_bytes()
        if demo:
            verify_terminal_catalog(response, descriptor, data, 30, True)
        else:
            verify_empty_catalog(response, descriptor, data)
        dialogue.send("finis")
        dialogue.until(b"pause catprep finis fp")
        dialogue.stop("quit\n")
    finally:
        try:
            if dialogue:
                dialogue.close()
        finally:
            log.close()
            assert {name: digest(path) for name, path in names.items()} == original
            assert {name: digest(destination / name) for name in original} == original
    (destination / "catalog-result.json").write_text(json.dumps({
        "guest_updater_verified": True, "protection": "off", "account_provisioned": demo,
        "catalog_sha256": digest(exchange / "catalogbin"), "source_sha256": original,
    }, indent=2) + "\n", encoding="ascii")
    print("Verified SOS catalog manufactured by guest upsoscat in explicit compatibility mode.")


def prepare_sos(dialogue, destination, catalog_from=None):
    """Prepare a verified catalog and a disc1 startup copy under protection."""
    exchange = destination / "exchange"
    dialogue.system("all catprep size 150000 temp disc 500 20 perm disc 200 10 run")
    dialogue.until(b"\x07")
    label = dialogue.fp("kitlabel 16 disc1 catdisc1 slow 36 9 2030")
    mounted = dialogue.fp("kiton devno.16")
    verify_second_disc(label, mounted, dialogue.fp("show"))
    if catalog_from is None:
        (exchange / "catinput").write_bytes(b"maxp 4\n")
        dialogue.fp("catspec=rcin catinput")
        response = dialogue.fp("soscat=upsoscat catspec")
    else:
        packed = catalog_fixture(catalog_from)
        segments = len(packed) // 768
        (exchange / "catpacked").write_bytes(packed)
        dialogue.fp("soscat=rcin conv.bin catpacked")
        # rcin reserves a minimum 36 segments. Resize only this new private
        # area, then require an exact export before retaining it.
        dialogue.fp(f"soscat=set {segments} disc")
    descriptor = dialogue.fp("lookup soscat")
    dialogue.fp("catalogbin=rcout conv.bin soscat")
    if catalog_from is None:
        verify_empty_catalog(response, descriptor, (exchange / "catalogbin").read_bytes())
    else:
        assert re.search(rf"(?m)^soscat\s+=set {segments} disc .*; temp\r?$", descriptor), descriptor
        assert (exchange / "catalogbin").read_bytes() == catalog_fixture(catalog_from)
    dialogue.fp("scope user soscat")
    dialogue.fp("oldsos=rcout startsos")
    source = (exchange / "oldsos").read_bytes()
    (exchange / "sosinput").write_bytes(private_sos_start(source, 2500, 200, "disc1"))
    for command in ("sostmp=rcin sosinput", "psosboot=copy sostmp", "scope user psosboot",
                    "newsos=rcout psosboot", "sosafter=rcout startsos"):
        dialogue.fp(command)
    assert (exchange / "newsos").read_bytes() == (exchange / "sosinput").read_bytes()
    assert (exchange / "sosafter").read_bytes() == source
    dialogue.send("finis")
    dialogue.until(b"pause catprep finis fp")
    (destination / "sos-preparation.json").write_text(json.dumps({
        "protection": "rc4000", "empty_catalog_verified": (exchange / "catalogbin").read_bytes() == EMPTY_CATALOG,
        "catalog_sha256": digest(exchange / "catalogbin"),
        "account_provisioned": (exchange / "catalogbin").read_bytes() == terminal_catalog_bytes(30, True),
        "second_disk_device": 16, "sos_permanent_claim": [2500, 200],
        "sos_document": "disc1", "original_startsos_unchanged": True,
        "private_startup": "psosboot", "startup_export_matches": True,
        "catalog_manufactured_under_protection": catalog_from is None,
        "catalog_imported_under_protection": catalog_from is not None,
    }, indent=2) + "\n", encoding="ascii")
    return "psosboot"


def preparation_failure(dialogue, destination, error, at_cli=False):
    if not at_cli:
        os.write(dialogue.session.master, b"\x05")
        dialogue.until(b"\nsim4000> ")
    dialogue.record(dialogue.session.command(b"show cpu\n"))
    saved = dialogue.record(dialogue.session.command(b"save-core preparation-failed.bin 0 524288\n"))
    assert "saved 524288 words" in saved, saved
    (destination / "preparation-failure.json").write_text(json.dumps({
        "assessment": "preparation-failed", "protection": "rc4000",
        "functional_service_verified": False, "error": str(error),
        "core": "preparation-failed.bin",
    }, indent=2) + "\n", encoding="ascii")


def primo_operator(dialogue):
    for command, error in (("display", None), ("zzzzz", "command unknown"), ("display", None)):
        os.write(dialogue.session.master, b"\x1b")
        dialogue.until(b"att")
        dialogue.until(b"\x07")
        dialogue.send("primo")
        dialogue.until(b"\x07")
        dialogue.send(command)
        marker = b"primo:ready\r\n" if error is None else b"primo:***command unknown\r\n"
        verify_primo_reply(dialogue.until(marker), error)


def saved_code_accessible(state, key):
    """Instruction access uses PR, not equality with the store-key register PK."""
    return 0 <= key <= 7 and ((state["pr"] | 0x80) & (0x80 >> key)) == 0


def process_state(data, name, first, size, parent_name="s"):
    """Match a live descriptor by bounds, kind, name and explicit parent name."""
    def word(address):
        assert address % 2 == 0 and 0 <= address < len(data) // 3 * 2
        offset = address // 2 * 3
        return int.from_bytes(data[offset:offset + 3], "big")

    def name_at(address):
        offset = address // 2 * 3
        return data[offset:offset + 12].rstrip(b"\0 ")

    candidates = []
    for offset in range(3, len(data) - 120, 3):
        if data[offset:offset + 12].rstrip(b"\0 ") != name.encode("ascii"):
            continue
        address = (offset // 3 - 1) * 2
        if word(address) != 0 or word(address + 22) != first or word(address + 24) != first + size:
            continue
        parent = word(address + 50)
        if parent % 2 or parent + 10 >= len(data) // 3 * 2:
            continue
        if word(parent) == 0 and name_at(parent + 2) == parent_name.encode("ascii"):
            # Internal process links point to the queue fields (+18), not
            # descriptor starts. A copied descriptor in a subsystem buffer
            # is not a member of the monitor's live reciprocal chain.
            next_link, previous_link = word(address + 18), word(address + 20)
            limit = len(data) // 3 * 2
            if (next_link % 2 == previous_link % 2 == 0 and
                    0 <= next_link < limit - 2 and 0 <= previous_link < limit and
                    word(next_link + 2) == address + 18 and word(previous_link) == address + 18):
                candidates.append(address)
    assert len(candidates) == 1, (name, candidates)
    address = candidates[0]
    return {"descriptor": address, "first": first, "top": first + size,
            "stop_count": word(address + 10) >> 12, "state": word(address + 10) & 4095,
            "pr": word(address + 32) >> 12, "pk": word(address + 32) & 4095,
            "interrupt_mask": word(address + 34), "interrupt_address": word(address + 36),
            "w": [word(address + delta) for delta in (38, 40, 42, 44)],
            "exceptions": word(address + 46), "saved_ic": word(address + 48)}


def protected_printer(dialogue, destination, evidence=None, expected_previous=b""):
    """Submit bounded real RC610 jobs without newer host-transfer libraries."""
    from test_mips_printer import operator, verify_completion
    exchange = destination / "exchange"
    evidence = evidence or destination
    source = ROOT / "doc/usermanual/examples/primo_print.alg"
    before = digest(source)
    (exchange / "prbsource").write_bytes(source.read_bytes())
    operator(dialogue, "signup printer 14")
    assigned = operator(dialogue, "display")
    assert re.search(r"(?m)^printer\s+[^\r\n]*,idle\s*\r?$", assigned), assigned
    dialogue.system("all prbprep size 150000 temp disc 500 20 run")
    dialogue.until(b"\x07")
    for command in ("psrc=rcin prbsource", "plp=set lp printer",
                    "prbprog=algol psrc connect.no"):
        reply = dialogue.fp(command)
        assert not re.search(r"(?m)^break\s", reply), reply
        if command.startswith("prbprog=algol"):
            assert len(re.findall(r"(?m)^algol end \d+\s*\r?$", reply)) == 1, reply
    output = destination / "printer.protected.txt"
    previous = output.read_bytes()
    assert previous == expected_previous, "printer output differs before jobs"
    jobs = []
    for job in (1, 2):
        payload = f"Protected PRIMO RC610 job {job}.\n".encode("ascii")
        if job == 2:
            payload += b"".join(f"Protected listing line {line:03d}.\n".encode("ascii")
                                for line in range(1, 161))
        (exchange / "prbinput").write_bytes(payload)
        for command in ("ptmp=rcin prbinput", "pin=copy ptmp"):
            dialogue.fp(command)
        reply = dialogue.fp("prbprog")
        (evidence / f"printer-job{job}.log").write_text(reply, encoding="ascii")
        transport = verify_completion(reply, payload)
        assert output.read_bytes() == previous + payload, "output differs or is delayed"
        previous += payload
        jobs.append({"job": job, "transport": transport, "bytes": len(payload)})
    dialogue.send("finis")
    dialogue.until(b"pause prbprep finis fp")
    operator(dialogue, "signoff printer")
    assert not re.search(r"(?m)^printer\s", operator(dialogue, "display"))
    assert digest(source) == before
    (evidence / "printer-result.json").write_text(json.dumps({
        "protection": "rc4000", "physical_device": "printer", "device_number": 7,
        "jobs": jobs, "visible_before_shutdown": True, "exact_output": True,
        "source_sha256": before, "output_sha256": digest(output),
        "scope": "local FP client, two sequential jobs; not remote submission or restart",
    }, indent=2) + "\n", encoding="ascii")
    return previous


def probe(simulator, destination, component, catalog_from=None, printer=False, restart=False, isolation=False):
    combined = component == "combined"
    components = tuple(SIZES) if combined else (component,)
    port = None
    if catalog_from is not None:
        assert component in ("sos", "combined"), "catalog input is only for SOS"
        catalog_fixture(catalog_from)  # Reject invalid inputs before creating runtime files.
    if combined:
        assert catalog_from and catalog_fixture(catalog_from) == terminal_catalog_bytes(30, True), "combined assessment requires a verified demo catalog"
        from test_operator_acceptance import ports
        port = ports()[0]
    names = {"MONITOR.protected": ROOT / "src/monitor/MONITOR.protected",
             "UNIT0.protected": ROOT / "src/disks/UNIT0.protected",
             "RC4000.protected.cfg": ROOT / "src/config/RC4000.protected.cfg"}
    original = {name: digest(path) for name, path in names.items()}
    destination.mkdir()  # Never reuse runtime files or earlier evidence.
    (destination / "exchange").mkdir()
    for name, path in names.items():
        shutil.copyfile(path, destination / name)
    if "sos" in components:
        creation = subprocess.run([str(simulator), "--mkdisc", "UNIT1.protected"],
                                  cwd=destination, capture_output=True, text=True, timeout=10)
        assert creation.returncode == 0, creation
        assert (destination / "UNIT1.protected").read_bytes() == bytes(180 * 768)
        config = destination / "RC4000.protected.cfg"
        config.write_text(config.read_text(encoding="ascii") +
                          "\n; Private protected SOS prerequisite disk, volatile writes only.\n"
                          "DEFINE RC433X DEVNO=16 FILE=UNIT1.protected\n", encoding="ascii")
        if combined:
            with config.open("a", encoding="ascii") as stream:
                stream.write(f"TELNET DEVNO=4 PORT={port} ADDRESS=127.0.0.1 ATTENTION=CTRL-A\n")
        if restart:
            text = config.read_text(encoding="ascii")
            for unit in (0, 1):
                line = f"DEFINE RC433X DEVNO={15 + unit} FILE=UNIT{unit}.protected"
                assert text.splitlines().count(line) == 1, "unrecognized private disk configuration"
                text = text.replace(line + "\n", line + f" OVERLAY=UNIT{unit}.protected.overlay\n")
            config.write_text(text, encoding="ascii")
    bases = {name: digest(destination / name) for name in names}
    if "sos" in components:
        bases["UNIT1.protected"] = digest(destination / "UNIT1.protected")
    dialogue = None
    with (destination / "session.log").open("x", encoding="ascii") as log:
        try:
            dialogue = Dialogue(simulator, destination, "RC4000.protected.cfg", b"initialize date:", log)
            dialogue.send("86 09 30 08 00 00")
            dialogue.until(b"pause startproc finis fp")
            initial = dialogue.system("list")
            assert not re.search(r"(?m)^(tem|primo|sos)\s+\d+", initial), initial
            start_name = f"start{component}"
            if "sos" in components:
                try:
                    start_name = prepare_sos(dialogue, destination, catalog_from)
                except Exception as error:
                    preparation_failure(dialogue, destination, error)
                    raise
            startup = ""
            for name in components:
                startup += dialogue.system(f"read {start_name if name == 'sos' else 'start' + name}")
            os.write(dialogue.session.master, b"\x05")
            startup += dialogue.until(b"\nsim4000> ")
            startup += dialogue.record(dialogue.session.command(b"step 1000000\n"))
            cpu = dialogue.record(dialogue.session.command(b"show cpu\n"))
            assert "protection=rc4000 " in cpu and "ix=on" in cpu, cpu
            os.write(dialogue.session.master, b"continue\n")
            operator_verified = False
            if "primo" in components and "message primo started" in startup:
                primo_operator(dialogue)
                operator_verified = True
            swap_verified = False
            if "sos" in components and "message sos started" in startup:
                dialogue.system("all storage size 50000 temp disc 100 10 perm disc 0 0 run")
                dialogue.until(b"\x07")
                swap = dialogue.fp("lookup swpsos")
                verify_sos_disc1_storage(swap)
                (destination / "sos-swap.txt").write_text(swap, encoding="ascii")
                dialogue.send("finis")
                dialogue.until(b"pause storage finis fp")
                swap_verified = True
            service_verified = False
            printer_verified = False
            if combined:
                verify_component_listing(dialogue.system("list"), components)
                if isolation:
                    prepare_isolation(dialogue, destination)
                if restart:
                    try:
                        retained_local(dialogue, destination, 1, seed=True)
                    except Exception as error:
                        preparation_failure(dialogue, destination, error)
                        raise
                if printer:
                    try:
                        protected_printer(dialogue, destination)
                        printer_verified = True
                    except Exception as error:
                        preparation_failure(dialogue, destination, error)
                        raise
                from test_mips_printer import terminal
                at_cli = False
                def active_check(listing):
                    nonlocal at_cli
                    os.write(dialogue.session.master, b"\x05")
                    dialogue.until(b"\nsim4000> ")
                    at_cli = True
                    cpu = dialogue.record(dialogue.session.command(b"show cpu\n"))
                    assert "protection=rc4000 " in cpu and "ix=on" in cpu, cpu
                    saved = dialogue.record(dialogue.session.command(b"save-core active.bin 0 524288\n"))
                    assert "saved 524288 words" in saved, saved
                    data = (destination / "active.bin").read_bytes()
                    assert len(data) == 524288 * 3
                    states = {}
                    for name in (*components, "test"):
                        parent = "sos" if name == "test" else "s"
                        size = 30208 if name == "test" else SIZES[name]
                        rows = re.findall(rf"(?m)^{name}\s+(\d+)\s+{size}\s+\d+\s+0\s+(?:run|wait|stop)\s+{parent}\s*\r?$", listing)
                        assert len(rows) == 1, listing
                        state = process_state(data, name, int(rows[0]), size, parent)
                        assert state["stop_count"] == 0 and 1 <= state["pk"] <= 7, state
                        pc = state["saved_ic"]
                        assert state["first"] <= pc < state["top"], state
                        keys = dialogue.record(dialogue.session.command(f"show keys {pc} 1\n".encode("ascii")))
                        match = re.findall(r"(?m)^[0-9a-f]{6}: key=(\d+)", keys)
                        assert len(match) == 1, (state, keys)
                        accessible = saved_code_accessible(state, int(match[0]))
                        state["saved_code_currently_accessible"] = accessible
                        if name == "test" and state["state"] & 32:
                            # SOS stops its child while waiting for terminal input.
                            # Its interval may already have been reclaimed with
                            # the parent's key. A saved PC is not a live fetch.
                            assert int(match[0]) in (state["pk"], states["sos"]["pk"]), (state, keys)
                            state["stopped_by_parent_at_input"] = bool(state["state"] & 16)
                            assert state["stopped_by_parent_at_input"], state
                        else:
                            assert accessible, (state, keys)
                            assert match == [str(state["pk"])], (state, keys)
                        state["saved_ic_memory_key"] = int(match[0])
                        states[name] = state
                    (destination / "active-processes.json").write_text(json.dumps({
                        "protection": "rc4000", "ix": True, "processes": states,
                    }, indent=2) + "\n", encoding="ascii")
                    os.write(dialogue.session.master, b"continue\n")
                    at_cli = False
                try:
                    terminal(dialogue, port, destination, active_check,
                             retained_session(True) if restart else None,
                             isolation_session(dialogue, destination) if isolation else None)
                    service_verified = True
                    if isolation:
                        primo_operator(dialogue)
                        (destination / "isolation-result.json").write_text(json.dumps({
                            "protection": "rc4000", "forbidden_store_target": ISOLATION_TARGET,
                            "monitor_word_unchanged": True, "faulted_child_removed": True,
                            "fresh_login_fp_commands_logout": True, "primo_operator_healthy": True,
                            "scope": "one forbidden monitor store; not general sibling or crash isolation",
                        }, indent=2) + "\n", encoding="ascii")
                    if restart:
                        retained_local(dialogue, destination, 1)
                except Exception as error:
                    preparation_failure(dialogue, destination, error, at_cli)
                    raise
            listing = dialogue.system("list")
            banner_seen = all(f"message {name} started" in startup + listing for name in components)
            (destination / "listing.txt").write_text(listing, encoding="ascii")
            os.write(dialogue.session.master, b"\x05")
            dialogue.until(b"\nsim4000> ")
            saved = dialogue.record(dialogue.session.command(b"save-core state.bin 0 524288\n"))
            assert "saved 524288 words" in saved, saved
            data = (destination / "state.bin").read_bytes()
            assert len(data) == 524288 * 3
            selected = "sos" if combined else component
            rows = re.findall(rf"(?m)^{selected}\s+(\d+)\s+{SIZES[selected]}\s+\d+\s+\d+\s+(?:run|wait|stop)\s+s\s*\r?$", listing)
            assert len(rows) <= 1, listing
            state = process_state(data, selected, int(rows[0]), SIZES[selected]) if len(rows) == 1 else None
            if state:
                pc = state["saved_ic"]
                if 0 <= pc < 1048576:
                    dialogue.record(dialogue.session.command(f"examine /instruction {max(0, pc - 8)} 8\n".encode("ascii")))
                    keys = dialogue.record(dialogue.session.command(f"show keys {pc} 1\n".encode("ascii")))
                    match = re.findall(r"(?m)^[0-9a-f]{6}: key=(\d+)", keys)
                    assert len(match) == 1, keys
                    state["saved_ic_memory_key"] = int(match[0])
                    descriptor_key = dialogue.record(dialogue.session.command(f"show keys {state['descriptor']} 1\n".encode("ascii")))
                    assert re.search(r"(?m)^[0-9a-f]{6}: key=0 ", descriptor_key), descriptor_key
                    if state["stop_count"] == 0:
                        assert state["first"] <= pc < state["top"], state
                        assert 1 <= state["pk"] <= 7 and state["saved_ic_memory_key"] == state["pk"], state
            result = {"component": component, "protection": "rc4000", "ix": True,
                      "assessment_scope": "terminal-service" if service_verified else "startup-and-operator" if operator_verified else "startup",
                      "startup_banner_seen": banner_seen,
                      "functional_service_verified": service_verified,
                      "primo_operator_probe_verified": operator_verified,
                      "primo_rc610_jobs_verified": printer_verified,
                      "sos_disc1_swap_verified": swap_verified,
                      "process": state, "source_sha256": original,
                      "assessment": "not-created" if state is None else
                          "stopped" if state["stop_count"] else
                          "initialization-not-complete" if not banner_seen and not combined else
                          "terminal-service-verified" if service_verified else "created-not-functionally-verified"}
            (destination / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="ascii")
            dialogue.session.finish()
            if printer_verified:
                manifest = json.loads((destination / "printer-result.json").read_text(encoding="ascii"))
                assert digest(destination / "printer.protected.txt") == manifest["output_sha256"], "shutdown changed completed output"
        finally:
            if dialogue:
                dialogue.close()
            assert {name: digest(path) for name, path in names.items()} == original
            assert {name: digest(destination / name) for name in bases} == bases
    if restart:
        try:
            verify_restart(simulator, destination, port, bases)
        finally:
            assert {name: digest(path) for name, path in names.items()} == original
    print(f"Protected {component} assessment: {result['assessment']}; no reusable profile produced.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new private evidence workspace")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--component", choices=(*SIZES, "combined"), default="tem")
    mode.add_argument("--prepare-sos-catalog", action="store_true",
                      help="explicitly manufacture a guest catalog in compatibility mode")
    parser.add_argument("--catalog-from", type=Path,
                        help="SOS/combined: import a verified catalog from a separate manufacturing workspace")
    parser.add_argument("--demo-account", action="store_true",
                        help="manufacture the known test/test terminal catalog instead of an empty catalog")
    parser.add_argument("--printer", action="store_true",
                        help="combined only: also compile and execute two real PRIMO-to-RC610 jobs")
    parser.add_argument("--restart", action="store_true",
                        help="combined only: private persistent overlays, remote file creation and a second boot")
    parser.add_argument("--isolation", action="store_true",
                        help="combined only: deliberate forbidden store, SOS child removal and fresh login")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    args = parser.parse_args()
    if args.catalog_from and (args.prepare_sos_catalog or args.component not in ("sos", "combined")):
        parser.error("--catalog-from requires --component sos or combined")
    if args.demo_account and not args.prepare_sos_catalog:
        parser.error("--demo-account requires --prepare-sos-catalog")
    if args.printer and (args.prepare_sos_catalog or args.component != "combined"):
        parser.error("--printer requires --component combined")
    if args.restart and (args.prepare_sos_catalog or args.component != "combined"):
        parser.error("--restart requires --component combined")
    if args.isolation and (args.prepare_sos_catalog or args.component != "combined" or args.restart):
        parser.error("--isolation requires --component combined and a separate run from --restart")
    if args.prepare_sos_catalog:
        create_sos_catalog(args.simulator.resolve(), args.destination.resolve(), args.demo_account)
    else:
        probe(args.simulator.resolve(), args.destination.resolve(), args.component,
              args.catalog_from.resolve() if args.catalog_from else None, args.printer, args.restart, args.isolation)


if __name__ == "__main__":
    main()
