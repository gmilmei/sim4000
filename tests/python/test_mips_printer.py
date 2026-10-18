#!/usr/bin/env python3
"""Opt-in PRIMO-to-RC610 acceptance on private MIPS/TS media and overlays."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from probe_mips_ts import digest, verify_component_listing
from test_mips_manual import Dialogue
from test_operator_acceptance import Remote, ports
from test_shared_terminal import select_process, reply_frame, settle_output
from mips_terminal import (verify_interactive_enrollment, verify_interactive_child,
                           verify_interactive_message, verify_interactive_lookup,
                           verify_interactive_logout)


def terminal(dialogue, port, evidence, active_check=None, session_check=None, fault_check=None):
    """Decode NVT CR-NUL while retaining the original wire transcript."""
    remote = Remote(port)
    with (evidence / "terminal-wire.bin").open("xb") as log:
        def until(marker):
            raw = remote.until(marker)
            log.write(raw)
            log.flush()
            return raw.replace(b"\r\0", b"\r").decode("ascii")

        def login(password):
            remote.send(b"\x01")
            until(b"\x07")
            remote.send(b"sos\r\n")
            assert until(b"\x07").endswith(">\x07")
            remote.send(f"run test pass {password}\r\n".encode("ascii"))

        try:
            negotiation = remote.until(b"\xff\xfd\x03")
            log.write(negotiation)
            assert b"\xff\xfb\x01" in negotiation
            remote.send(b"\xff\xfd\x01\xff\xfd\x03\xff\xfb\x03")
            login("wrong")
            assert "***sos: bad password" in until(b"bad password\r\n")
            login("test")
            verify_interactive_enrollment(until(b"\x07"), "run")
            listing = dialogue.system("list")
            verify_component_listing(listing, ("tem", "primo", "sos"))
            verify_interactive_child(listing)
            (evidence / "terminal-active.txt").write_text(listing, encoding="ascii")
            remote.send(b"message termprobe\r\n")
            verify_interactive_message(until(b"\x07"))
            remote.send(b"lookup fp\r\n")
            verify_interactive_lookup(until(b"\x07"))
            if session_check:
                session_check(remote.send, until)
            if active_check:
                active_check(dialogue.system("list"))
            if fault_check:
                fault_check(remote.send, until)
                listing = dialogue.system("list")
                verify_component_listing(listing, ("tem", "primo", "sos"))
                assert not re.search(r"(?m)^test\s+\d+", listing), listing
                (evidence / "terminal-after-fault.txt").write_text(listing, encoding="ascii")
                login("test")
                verify_interactive_enrollment(until(b"\x07"), "run")
                verify_interactive_child(dialogue.system("list"))
                remote.send(b"message termprobe\r\n")
                verify_interactive_message(until(b"\x07"))
                remote.send(b"lookup fp\r\n")
                verify_interactive_lookup(until(b"\x07"))
            remote.send(b"finis\r\n")
            verify_interactive_logout(until(b"test removed after finis\r"))
            listing = dialogue.system("list")
            verify_component_listing(listing, ("tem", "primo", "sos"))
            assert not re.search(r"(?m)^test\s+\d+", listing), listing
            (evidence / "terminal-finished.txt").write_text(listing, encoding="ascii")
        finally:
            log.write(remote.pending)
            remote.close()


def operator(dialogue, command):
    os.write(dialogue.session.master, b"\x1b")
    dialogue.until(b"att")
    dialogue.until(b"\x07")
    dialogue.send("primo")
    dialogue.until(b"\x07")
    dialogue.send(command)
    reply = dialogue.until(b"primo:ready\r\n")
    assert "***" not in reply, reply
    return reply


def cli(dialogue, command):
    os.write(dialogue.session.master, b"\x05")
    dialogue.until(b"\nsim4000> ")
    result = dialogue.record(dialogue.session.command(command.encode("ascii") + b"\n"))
    os.write(dialogue.session.master, b"continue\n")
    return result


def verify_completion(reply, payload):
    definitions = re.findall(r"(?m)^primo define 0 reply 0 transport (\d+)\r?$", reply)
    assert len(definitions) == 1 and int(definitions[0]) > 0, reply
    assert len(re.findall(r"(?m)^primo wait 0 reply 0 state 5 cause -1\r?$", reply)) == 1, reply
    assert len(re.findall(r"(?m)^end\s+\d+\s*\r?$", reply)) == 1, reply
    assert not any(marker in reply for marker in ("***", "hard error", "abnormal end")), reply
    assert not any(line in reply for line in payload.decode("ascii").splitlines()), "printer payload leaked to console"
    return int(definitions[0])


def verify_offline(failure, display):
    assert len(re.findall(r"(?m)^\d{2}\.\d{2} primo:\*\*\*printer end document\r?$", failure)) == 1, failure
    assert "primo wait" not in failure and not re.search(r"(?m)^end\s+\d+", failure), failure
    assert re.search(r"(?m)^printer\s+[^\r\n]*\bpin\s+[^\r\n]*,waiting\s*\r?$", display), display


def offline_recovery(dialogue, exchange, output, evidence, previous):
    """Keep the transport blocked until a healthy append connection returns."""
    payload = b"PRIMO offline recovery: exactly once.\n"
    (exchange / "prbinput").write_bytes(payload)
    dialogue.fp("ptmp=fromhost prbinput")
    dialogue.fp("pin=copy ptmp")
    assert "output detached" in cli(dialogue, "detach 7")
    assert output.read_bytes() == previous
    dialogue.send("prbprog")
    failure = dialogue.until(b"primo:") + dialogue.until(b"\r\n")
    (evidence / "offline-notice.txt").write_text(failure, encoding="ascii")
    assert output.read_bytes() == previous
    held = operator(dialogue, "display")
    (evidence / "offline-display.txt").write_text(held, encoding="ascii")
    verify_offline(failure, held)
    verify_component_listing(dialogue.system("list"), ("tem", "primo", "sos"))
    assert "output attached" in cli(dialogue, "attach 7 printer.mipsts.txt append")
    assert output.read_bytes() == previous, "attach replayed or truncated output"
    resumed = operator(dialogue, "start printer")
    completed = dialogue.until(b"\x07")
    (evidence / "offline-resumed.txt").write_text(resumed + completed, encoding="ascii")
    verify_completion(resumed + completed, payload)
    assert output.read_bytes() == previous + payload
    return payload


def queued_jobs(dialogue, exchange, output, evidence, previous):
    source = (ROOT / "tests/fixtures/primo_queued.alg").read_bytes()
    assert source.count(b"<:pin:>") == source.count(b"<:plp:>") == 1
    (exchange / "qsource").write_bytes(source)
    (exchange / "qnext").write_bytes(source.replace(b"<:pin:>", b"<:pjn:>").replace(b"<:plp:>", b"<:qlp:>"))
    payloads = (b"Queued PRIMO client one.\n", b"Queued PRIMO client two.\n")
    dialogue.fp("clear temp prbprog")
    dialogue.fp("psrc=fromhost qsource")
    dialogue.fp("qone=algol psrc connect.no")
    (exchange / "prbinput").write_bytes(payloads[0])
    dialogue.fp("ptmp=fromhost prbinput")
    dialogue.fp("pin=copy ptmp")
    assert "output detached" in cli(dialogue, "detach 7")
    dialogue.send("qone")
    held_reply = dialogue.until(b"primo define 0 reply 0 transport ") + dialogue.until(b"\r\n")
    if "end document\r\n" not in held_reply:
        held_reply += dialogue.until(b"end document\r\n")
    (evidence / "queued-held.txt").write_text(held_reply, encoding="ascii")
    first, failure = held_definition(held_reply)
    dialogue.system("new prbnext size 150000 temp disc 200 10 perm disc 0 0 run")
    dialogue.until(b"\x07")
    (exchange / "prbinput").write_bytes(payloads[1])
    for command in ("psrc=fromhost qnext", "qlp=set lp printer",
                    "qtwo=algol psrc connect.no", "ptmp=fromhost prbinput", "pjn=copy ptmp"):
        dialogue.fp(command)
    dialogue.send("qtwo")
    second = dialogue.until(b"primo define 0 reply 0 transport ") + dialogue.until(b"\r\n")
    waiting = operator(dialogue, "display")
    (evidence / "queued-waiting.txt").write_text(first + failure + second + waiting, encoding="ascii")
    ids = verify_queued_definitions(first, second)
    verify_offline(failure, waiting)
    assert output.read_bytes() == previous
    assert "output attached" in cli(dialogue, "attach 7 printer.mipsts.txt append")
    resumed = operator(dialogue, "start printer")
    completions = ""
    deadline = time.monotonic() + 30
    for _ in range(16):
        frame = dialogue.until(b"\x07", timeout=max(0.1, deadline - time.monotonic()))
        completions += frame
        if len(re.findall(r"(?m)^end\s+\d+\s*\r?$", completions)) == 2:
            break
        # Only an explicitly completed ordinary FP may receive an empty
        # command to let the other client's queued console output proceed.
        assert re.search(r"(?m)^end\s+\d+\s*\r?$", completions), frame
        assert "***" not in frame and "break" not in frame, frame
        settle_output(dialogue.session)
        dialogue.send("")
    else:
        raise AssertionError("queued clients did not both return normally")
    (evidence / "queued.txt").write_text(first + failure + second + waiting + resumed + completions, encoding="ascii")
    verify_queued_completion(completions, payloads)
    assert output.read_bytes() == previous + b"".join(payloads)
    idle_command(dialogue, "s", "proc prbnext remove", "ready")
    idle_command(dialogue, "prbprep", "message queue ready", " queue ready")
    followup = b"PRIMO after queued pair.\n"
    (exchange / "prbinput").write_bytes(followup)
    dialogue.fp("ptmp=fromhost prbinput")
    dialogue.fp("pin=copy ptmp")
    followup_reply = dialogue.fp("qone")
    followup_id = verify_completion(followup_reply, followup)
    assert followup_id not in ids
    expected = b"".join(payloads) + followup
    assert output.read_bytes() == previous + expected
    (evidence / "queued-result.json").write_text(json.dumps({
        "outstanding_transport_ids": ids, "followup_transport_id": followup_id,
        "blocking_waiters": 1, "polling_clients": 1,
        "payload_bytes": len(expected), "exact_fifo_output": True,
    }, indent=2) + "\n", encoding="ascii")
    return expected


def idle_command(dialogue, name, command, expected):
    """Route only the two clients already known to have returned to idle FP."""
    select_process(dialogue.session, name)
    dialogue.send(command)
    output = ""
    for _ in range(16):
        frame = dialogue.record(reply_frame(dialogue.session, name == "s"))
        assert "***" not in frame and "break" not in frame, frame
        output += frame
        routed = re.search(r"\bto (prbprep|prbnext)\s", frame)
        if expected in output and (name == "s" or routed is None or routed[1] == name):
            return output
        if routed:
            settle_output(dialogue.session)
            dialogue.send("")
    raise AssertionError((name, command, output))


def verify_queued_completion(reply, payloads):
    assert "***" not in reply and "hard error" not in reply, reply
    for name, operation in (("prbprep", "wait"), ("prbnext", "poll")):
        assert len(re.findall(rf"(?m)^from {name}\r?$", reply)) == 1, reply
        assert len(re.findall(rf"(?m)^primo {operation} 0 reply 0 state 5 cause -1\r?$", reply)) == 1, reply
    assert len(re.findall(r"(?m)^end\s+\d+\s*\r?$", reply)) == 2, reply
    assert not any(line in reply for payload in payloads for line in payload.decode("ascii").splitlines()), reply


def verify_queued_definitions(first, second):
    ids = []
    for reply in (first, second):
        matches = re.findall(r"(?m)^primo define 0 reply 0 transport (\d+)\r?$", reply)
        assert len(matches) == 1 and int(matches[0]) > 0, reply
        assert "primo wait" not in reply and "***" not in reply, reply
        assert not re.search(r"(?m)^end\s+\d+", reply), reply
        ids.append(int(matches[0]))
    assert ids[0] != ids[1], "outstanding transports have the same identity"
    return ids


def held_definition(reply):
    """The client acknowledgement and asynchronous notice may arrive either way."""
    definitions = re.findall(r"(?m)^primo define 0 reply 0 transport [1-9]\d*\r?$", reply)
    notices = re.findall(r"(?m)^\d{2}\.\d{2} primo:\*\*\*printer end document\r?$", reply)
    assert len(definitions) == len(notices) == 1, reply
    assert "***" not in reply.replace(notices[0], ""), reply
    assert "primo wait" not in reply and not re.search(r"(?m)^end\s+\d+", reply), reply
    return definitions[0].rstrip("\r") + "\r\n", notices[0].rstrip("\r") + "\r\n"


def verify_primo_start(text):
    expected = ("* new primo size 16938 function 1 2 3 4 5 buf 10 area 7 "
                "* perm disc 18 3 base -8388607 8388605 prog bprimo "
                "* run * unstack")
    assert text.split() == expected.split(), "unrecognized installed PRIMO startup"


def verify_primo_trimming(text):
    for name, value in (("waitops", 1), ("waittrans", 300), ("prcount", 3),
                        ("oprcount", 4), ("options", 941109)):
        assignments = re.findall(rf"\b{name}\s*:=\s*(\d+)\s*,", text)
        assert assignments == [str(value)], (name, assignments)


def restart_primo(dialogue, exchange, evidence, buffers):
    """Adjust only a private live process claim, never its program or disk script."""
    dialogue.system("all cfgprep size 100000 temp disc 100 10 run")
    dialogue.until(b"\x07")
    dialogue.fp("oldprimo=tohost startprimo")
    dialogue.fp("oldtrim=tohost primotrim")
    dialogue.fp("oldbprimo=tohost conv.bin bprimo")
    startup = (exchange / "oldprimo").read_text(encoding="ascii")
    verify_primo_start(startup)
    trimming = (exchange / "oldtrim").read_text(encoding="ascii")
    verify_primo_trimming(trimming)
    dialogue.send("finis")
    dialogue.until(b"pause cfgprep finis fp")
    dialogue.system("proc primo remove")
    dialogue.system(f"new primo size 16938 function 1 2 3 4 5 buf {buffers} area 7")
    dialogue.system("perm disc 18 3 base -8388607 8388605 prog bprimo")
    dialogue.system("run")
    dialogue.until(b"message primo started\r\n")
    verify_component_listing(dialogue.system("list"), ("tem", "primo", "sos"))
    (evidence / "primo-resources.json").write_text(json.dumps({
        "runtime_buffers": buffers, "area_claim": 7, "startprimo_unchanged": True,
        "startup_sha256": digest(exchange / "oldprimo"),
        "trimming_sha256": digest(exchange / "oldtrim"),
        "bprimo_sha256": digest(exchange / "oldbprimo"),
        "trimming_waitops": 1, "binary_regenerated": False,
    }, indent=2) + "\n", encoding="ascii")


def run(simulator, destination, offline=False, queued=False, primo_buffers=None):
    sources = {"MONITOR": ROOT / "src/monitor/MONITOR",
               "UNIT0.mipsts": ROOT / "src/disks/UNIT0.mipsts",
               "UNIT1.mipsts": ROOT / "src/disks/UNIT1.mipsts",
               "RC4000.mipsts.cfg": ROOT / "src/config/RC4000.mipsts.cfg"}
    before = {name: digest(path) for name, path in sources.items()}
    destination.mkdir()  # Never overwrite earlier evidence or a working session.
    exchange = destination / "exchange"
    exchange.mkdir()
    for name, path in sources.items():
        shutil.copyfile(path, destination / name)
    port = ports()[0]
    config = destination / "RC4000.mipsts.cfg"
    config.write_text(config.read_text(encoding="ascii").replace("PORT=4004", f"PORT={port}"),
                      encoding="ascii")
    bases = {name: digest(destination / name) for name in sources}
    (exchange / "prbsource").write_bytes((ROOT / "doc/usermanual/examples/primo_print.alg").read_bytes())
    output = destination / "printer.mipsts.txt"
    observations = []
    expected_output = b""
    try:
        for boot in (1, 2):
            evidence = destination / f"boot{boot}"
            evidence.mkdir()
            dialogue = None
            with (evidence / "console.log").open("x", encoding="ascii") as log:
                try:
                    dialogue = Dialogue(simulator, destination, config.name, b"from s", log)
                    dialogue.until(b"message sos started\r\n")
                    verify_component_listing(dialogue.system("list"), ("tem", "primo", "sos"))
                    if primo_buffers is not None and boot == 1:
                        restart_primo(dialogue, exchange, evidence, primo_buffers)
                    operator(dialogue, "signup printer 14")
                    assigned = operator(dialogue, "display")
                    assert re.search(r"(?m)^printer\s+[^\r\n]*,idle\s*\r?$", assigned), assigned
                    creation = "new" if queued else "all"
                    claim = "200 10" if queued else "500 20"
                    permanent = " perm disc 0 0" if queued else ""
                    dialogue.system(f"{creation} prbprep size 150000 temp disc {claim}{permanent} run")
                    dialogue.until(b"\x07")
                    for command in ("psrc=fromhost prbsource", "plp=set lp printer",
                                    "prbprog=algol psrc connect.no"):
                        dialogue.fp(command)
                    previous = output.read_bytes()
                    assert previous == expected_output, "restart changed earlier printer output"
                    for job in (1, 2):
                        payload = f"PRIMO RC610 boot {boot} job {job}.\n".encode("ascii")
                        if job == 2:
                            payload += b"".join(f"Listing line {line:03d}: repeated PRIMO spool output.\n".encode("ascii")
                                                for line in range(1, 161))
                        (exchange / "prbinput").write_bytes(payload)
                        for command in ("ptmp=fromhost prbinput", "pin=copy ptmp"):
                            dialogue.fp(command)
                        reply = dialogue.fp("prbprog")
                        transport = verify_completion(reply, payload)
                        current = output.read_bytes()  # Must be visible before shutdown, without sleeps.
                        assert current.startswith(previous), "printer output was truncated"
                        printed = current[len(previous):]
                        assert printed == payload, repr(printed)
                        (evidence / f"job{job}.txt").write_bytes(printed)
                        observations.append({"boot": boot, "job": job,
                                             "transport": transport, "bytes": len(printed)})
                        previous = current
                        expected_output += payload
                    if offline and boot == 1:
                        expected_output += offline_recovery(dialogue, exchange, output, evidence, expected_output)
                    if queued and boot == 1:
                        expected_output += queued_jobs(dialogue, exchange, output, evidence, expected_output)
                    dialogue.send("finis")
                    dialogue.until(b"pause prbprep finis fp")
                    operator(dialogue, "display")
                    operator(dialogue, "signoff printer")
                    unassigned = operator(dialogue, "display")
                    assert not re.search(r"(?m)^printer\s", unassigned), unassigned
                    verify_component_listing(dialogue.system("list"), ("tem", "primo", "sos"))
                    terminal(dialogue, port, evidence)
                    dialogue.stop("quit\n")
                finally:
                    if dialogue:
                        dialogue.close()
            assert {name: digest(destination / name) for name in bases} == bases
            assert output.read_bytes() == expected_output, "shutdown changed completed output"
        (destination / "result.json").write_text(json.dumps({
            "jobs": observations, "physical_device": "printer", "device_number": 7,
            "completed_state": 5, "completion_cause": -1,
            "visible_before_shutdown": True, "telnet_login_logout_each_boot": True,
            "boots": 2, "base_images_unchanged": True, "output_sha256": digest(output),
            "offline_recovery": offline,
            "two_client_queue": queued,
            "private_primo_buffer_claim": primo_buffers,
        }, indent=2) + "\n", encoding="ascii")
    finally:
        assert {name: digest(path) for name, path in sources.items()} == before


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, nargs="?", help="new exclusive evidence directory")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    parser.add_argument("--offline", action="store_true", help="also exercise detached output and PRIMO recovery")
    parser.add_argument("--queued", action="store_true", help="also probe two outstanding independent transports")
    parser.add_argument("--primo-buffers", type=int, choices=(10, 11),
                        help="diagnostic only: recreate the private live PRIMO with this buffer claim on boot 1")
    args = parser.parse_args()
    if args.destination:
        run(args.simulator.resolve(), args.destination.resolve(), args.offline, args.queued, args.primo_buffers)
    else:
        with tempfile.TemporaryDirectory(prefix="sim4000-mips-printer-") as temporary:
            run(args.simulator.resolve(), Path(temporary) / "runtime", args.offline, args.queued, args.primo_buffers)
    print("MIPS/TS PRIMO RC610: four completed jobs, two boots and TEM/SOS login/logout passed."
          + (" Detached-output recovery passed." if args.offline else "")
          + (" Two-client queue/polling and followup passed." if args.queued else ""))


if __name__ == "__main__":
    main()
