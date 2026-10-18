#!/usr/bin/env python3
"""Inspect MIPS/TS startup material on exclusive disposable runtime copies."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import sys
import time

from make_unit0_copy import CopyError
from mips_primo import DEVICE_COMMANDS, PRIMO_TEXT, verify_primo_devices, verify_primo_transfer

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/python"))
from test_operator_cli import Session
from test_operator_acceptance import ports
TEXTS = ("startmips", "starttem", "startprimo", "startsos",
         "temtrim", "primotrim", "sostrim", "startsysadm")
BINARIES = {"btem": 14, "bprimo": 52, "bsos": 29, "upsoscat": 114}
EMPTY_CATALOG = b"\xff" * 759 + bytes.fromhex("000000 000032 ffffff")
ACCOUNT_INPUT = 'maxp 4\nproc test\nmins 20000\nmaxs 30000\npass "test"'
TERMINAL_INPUT = ACCOUNT_INPUT + '\nterm console1 "1" "test" 60 2'
ACCOUNT_RESOURCE_CLAUSE = '\nbs disc key0 6 30 key1 0 0 key2 0 0 key3 0 0'
PARSER_OFFSET = 40638
PARSER_SIGNATURE = bytes.fromhex("512f29502f01a16effa4100037ce281d2efd5d2e594d8e39")
SECOND_DISC_TEXT = b"".join(f"Second disk probe {i:05d}: data.\n".encode("ascii")
                            for i in range(5120))
SECOND_DISC_SEGMENTS = (len(SECOND_DISC_TEXT) + 767) // 768
BATCH_JOB = b"finis\n"
BATCH_JOBS = {"finis": BATCH_JOB, "message": b"message batchprobe\nfinis\n",
              "lookup": b"lookup batchjob\nfinis\n", "read": b"copy batchjob\nfinis\n",
              "allocate": b"batchproof=set 1\nfinis\n",
              "copy": b"batchproof=copy batchjob\nfinis\n",
              "roundtrip": b"batchproof=copy batchjob\ncopy batchproof\nfinis\n",
              "retain": b"batchproof=copy batchjob\nscope user batchproof\nfinis\n"}


def account_catalog_bytes(segments=0, user_storage=False):
    """Recorded updater layout for this disposable fixture, not a general encoder."""
    directory = b"test" + bytes(8) + bytes.fromhex("000001")
    directory += b"\xff" * (759 - len(directory))
    directory += bytes.fromhex("000001 000032 ffffff")
    record = bytearray(546)
    record[:3] = bytes.fromhex("004007")
    record[21:27] = b"test\0\0"
    record[33:39] = bytes.fromhex("004e20 007530")
    record[114:120] = b"disc\0\0"
    record[126:129] = bytes.fromhex("000006")
    if segments not in (0, 30):
        raise CopyError("only the fixed 0/30-segment account fixtures are supported")
    record[129:132] = segments.to_bytes(3, "big")
    if user_storage:
        if segments != 30:
            raise CopyError("user storage requires the fixed 30-segment temporary claim")
        record[132:150] = bytes.fromhex("000002 000006") * 3
    return directory + record + b"\xff" * 222


def terminal_catalog_bytes(segments=0, user_storage=False):
    """Fixed observed terminal record; not a general catalog writer."""
    data = bytearray(account_catalog_bytes(segments, user_storage))
    record = bytearray(39)
    record[:12] = b"console1" + bytes(4)
    record[12:15] = b"1\0\0"
    record[15:27] = b"test" + bytes(8)
    record[27:30] = bytes.fromhex("03c002")
    data[1314:1353] = record
    return bytes(data)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_inventory(work):
    exchange = work / "exchange"
    transcript = (work / "inventory.log").read_text(encoding="ascii")
    if transcript.count("MIPS inventory exported without executing subsystem startup") != 1 or \
            "result=0" not in transcript or "***" in transcript or "syntax error:" in transcript:
        raise CopyError("MIPS inventory session did not complete cleanly")
    for name in TEXTS:
        data = (exchange / name).read_bytes()
        if not data.strip() or any(c in data for c in (b"\r", b"\0", b"\x19")):
            raise CopyError(f"missing or unnormalized MIPS text: {name}")
    for name, segments in BINARIES.items():
        if len((exchange / name).read_bytes()) != segments * 768:
            raise CopyError(f"unexpected MIPS binary allocation: {name}")
    if not (exchange / "metadata").read_text(encoding="ascii").startswith("startmips"):
        raise CopyError("missing MIPS startup descriptors")


def verify_component_listing(output, names):
    for name in names:
        size = {"tem": 11000, "primo": 16938, "sos": 300000}[name]
        rows = re.findall(rf"(?m)^{name}\s+(\d+)\s+{size}\s+(\d+)\s+0\s+(run|wait)\s+s\s*\r?$", output)
        if len(rows) != 1 or int(rows[0][0]) < 8 or int(rows[0][1]) == 0:
            raise CopyError(f"missing/invalid active component row: {name}")


def verify_primo_reply(output, error=None):
    completions = re.findall(r"(?m)^\d{2}\.\d{2} primo:ready\r?$", output)
    diagnostics = re.findall(r"(?m)^.*\*\*\*[^\r\n]*", output)
    if error is None:
        if diagnostics or len(completions) != 1:
            raise CopyError("missing/duplicate PRIMO completion or unexpected diagnostic")
    elif completions or len(diagnostics) != 1 or not re.fullmatch(rf"\d{{2}}\.\d{{2}} primo:\*\*\*{re.escape(error)}", diagnostics[0]):
        raise CopyError("PRIMO negative-control diagnostic differs")


def verify_empty_catalog(response, descriptor, data):
    if not re.search(r"\nend\s+42\s*\r?\n", response) or any(
            marker in response for marker in ("***", "abnormal end", "illegal char", "errors")):
        raise CopyError("empty-catalog updater did not complete normally")
    if not re.search(r"(?m)^soscat\s+=set 1 disc .*; temp\r?$", descriptor) or data != EMPTY_CATALOG:
        raise CopyError("empty-catalog descriptor or packed bytes differ")


def verify_account_catalog(response, descriptor, data, segments=0, user_storage=False):
    completion = r"(?m)^end\s+58\s*\r?$" if segments else r"(?m)^end\s+56\s*\r?$"
    if len(re.findall(r"(?m)^end\s+\d+\s*\r?$", response)) != 1 or len(re.findall(completion, response)) != 1 or any(
            marker in response for marker in ("***", "abnormal end", "illegal char", "errors")):
        raise CopyError("test-account updater did not complete normally")
    if not re.search(r"(?m)^soscat\s+=set 2 disc .*; temp\r?$", descriptor) or data != account_catalog_bytes(segments, user_storage):
        raise CopyError("test-account descriptor or packed bytes differ")


def verify_terminal_catalog(response, descriptor, data, segments=0, user_storage=False):
    completion = r"(?m)^end\s+60\s*\r?$" if segments else r"(?m)^end\s+58\s*\r?$"
    if len(re.findall(r"(?m)^end\s+\d+\s*\r?$", response)) != 1 or len(re.findall(completion, response)) != 1 or any(
            marker in response for marker in ("***", "abnormal end", "illegal char", "errors")):
        raise CopyError("test-terminal updater did not complete normally")
    if not re.search(r"(?m)^soscat\s+=set 2 disc .*; temp\r?$", descriptor) or data != terminal_catalog_bytes(segments, user_storage):
        raise CopyError("test-terminal descriptor or packed bytes differ")


def catalog_outcome(response, descriptor, data):
    """Classify updater completion without certifying accounts or terminals."""
    labels = re.findall(r"(?m)^([a-z][a-z ]*?)\s{2,}process [^\r\n]+\r?$", response)
    rejected = any(marker in response for marker in ("***", "abnormal end", "illegal char", "errors"))
    completions = re.findall(r"(?m)^end\s+(\d+)\s*\r?$", response)
    areas = re.findall(r"(?m)^soscat\s+=set (\d+) disc .*; temp\r?$", descriptor)
    segments = int(areas[0]) if len(areas) == 1 else None
    export_matches = segments is not None and segments > 0 and data is not None and len(data) == segments * 768
    if rejected:
        status = "rejected"
    elif len(completions) == 1 and export_matches:
        status = "normal"
    elif completions:
        status = "inconsistent"
    else:
        status = "incomplete"
    return {"status": status, "diagnostics": labels, "allocated_segments": segments,
            "exported_bytes": None if data is None else len(data),
            "export_matches_allocation": export_matches,
            "account_login_verified": False, "terminal_connection_verified": False}


def verify_sos_startup(output, listing):
    if "message sos release: 5.0" not in output or output.count("message sos started") != 1 or \
            "***" in output or "bs claims exceeded" in output or "area unknown" in output:
        raise CopyError("SOS did not complete the known initialization")
    verify_component_listing(listing, ("tem", "primo", "sos"))


def verify_batch_enrollment(reply, listing):
    """Check the fixed dummy account, not general login or job completion."""
    if len(re.findall(r"(?m)^\d{2}\.\d{2} sos: test enrolled\r?$", reply)) != 1 or "***" in reply:
        raise CopyError("fixed batch account was not enrolled exactly once")
    verify_component_listing(listing, ("tem", "primo"))
    sos = re.findall(r"(?m)^sos\s+(\d+)\s+300000\s+5\s+1\s+wait\s+s\s*\r?$", listing)
    child = re.findall(r"(?m)^test\s+(\d+)\s+30208\s+3\s+0\s+(?:run|wait)\s+sos\s*\r?$", listing)
    if len(re.findall(r"(?m)^(?:sos|test)\s+\d+", listing)) != 2 or \
            len(sos) != 1 or len(child) != 1 or int(sos[0]) < 8 or \
            not int(sos[0]) < int(child[0]) < int(sos[0]) + 300000:
        raise CopyError("fixed batch child or expected SOS parent stop count is missing")


def verify_batch_job(output, listing, workload="finis"):
    if workload not in BATCH_JOBS:
        raise CopyError("unknown fixed batch workload")
    enrolled = re.findall(r"(?m)^\d{2}\.\d{2} sos: test enrolled\r?$", output)
    removed = re.findall(r"(?m)^\d{2}\.\d{2} sos: test removed after finis\r?$", output)
    if len(enrolled) != 1 or len(removed) != 1 or output.index(enrolled[0]) >= output.index(removed[0]) or \
            "***" in output or "removed after break" in output or re.search(r"(?m)^test\s+\d+", listing):
        raise CopyError("minimal batch job did not enroll, finish normally and disappear exactly once")
    verify_component_listing(listing, ("tem", "primo", "sos"))
    for name, key in (("tem", 1), ("primo", 2), ("sos", 5)):
        if len(re.findall(rf"(?m)^{name}\s+\d+", listing)) != 1 or not re.search(
                rf"(?m)^{name}\s+\d+\s+\d+\s+{key}\s+0\s+(?:run|wait)\s+s\s*\r?$", listing):
            raise CopyError("minimal batch completion has duplicate components or unexpected keys")
    if workload == "message" and len(re.findall(r"(?m)^ batchprobe\r?$", output)) != 1:
        raise CopyError("batch message output is missing or duplicated")
    if workload == "lookup" and len(re.findall(r"(?m)^batchjob\s+=set 1 disc .*; system\r?$", output)) != 1:
        raise CopyError("batch lookup output is missing or duplicated")
    if workload in ("read", "copy", "roundtrip", "retain"):
        names = ("batchjob", "batchproof") if workload == "roundtrip" else ("batchjob",)
        for name in names:
            rows = re.findall(rf"(?m)^{name}\s+segm\.\s+(\d+)\s+(\d+)/(\d+)\s*\r?$", output)
            data = BATCH_JOBS[workload]
            if rows != [("1", str(len(data)), str(sum(data)))]:
                raise CopyError("batch copy character-count/checksum report differs")


def batch_core_state(core):
    """Read the fixed child's descriptor; offsets come from montxt1 a11..a34."""
    if len(core) != 524288 * 3:
        raise CopyError("batch snapshot is not a complete packed core")
    def word(address):
        if address < 0 or address % 2 or address + 2 > 1048576:
            raise CopyError("invalid descriptor address in batch snapshot")
        offset = address // 2 * 3
        return int.from_bytes(core[offset:offset + 3], "big")
    candidates = []
    for offset in range(3, len(core) - 90, 3):
        if core[offset:offset + 12] != b"test" + bytes(8):
            continue
        address = offset // 3 * 2 - 2
        first, top, parent = word(address + 22), word(address + 24), word(address + 50)
        if word(address) != 0 or top - first != 30208 or not 8 <= parent < 1048480 or parent % 2:
            continue
        p = (parent + 2) // 2 * 3
        if word(parent) != 0 or core[p:p + 12] != b"sos" + bytes(9) or word(parent + 24) - word(parent + 22) != 300000:
            continue
        if not word(parent + 22) <= first < top <= word(parent + 24):
            continue
        state = word(address + 10)
        candidates.append({"descriptor": address, "parent_descriptor": parent,
                           "parent_state": word(parent + 10) & 4095,
                           "parent_stop_count": word(parent + 10) >> 12,
                           "first": first, "top": top, "stop_count": state >> 12,
                           "state": state & 4095, "exceptions": word(address + 46),
                           "ic": word(address + 48), "interrupt_address": word(address + 36),
                           "key": word(address + 32) & 4095})
    if len(candidates) != 1:
        raise CopyError("fixed batch child descriptor is absent or ambiguous in snapshot")
    return candidates[0]


def verify_retained_batch_file(descriptor, text, source_packed, copied_packed):
    rows = re.findall(r"(?m)^batchproof\s+=set (\d+) (\S+) .*; (\w+)\r?$", descriptor)
    metadata = re.findall(r"(?m)^\s*;\s+\d+\s+\d+\s+3\s+0\s+0\s*\r?$", descriptor)
    logical = BATCH_JOBS["retain"] + b"\x19"
    if rows != [("1", "disc", "user")] or len(metadata) != 1 or "***" in descriptor or \
            text != BATCH_JOBS["retain"] or len(source_packed) != 768 or len(copied_packed) != 768 or \
            source_packed[:len(logical)] != logical or copied_packed[:len(logical)] != logical:
        raise CopyError("retained batch descriptor, text or exact packed contents differ")


def private_sos_start(source, segments, entries, document="disc"):
    old = b"perm disc 2500 200"
    if source.count(old) != 1 or not 1 <= segments <= 6090 or not 1 <= entries <= 1080 or document not in ("disc", "disc1"):
        raise CopyError("unexpected SOS startup resource clause or invalid private claim")
    return source.replace(old, f"perm {document} {segments} {entries}".encode("ascii"))


def validate_operator_commands(probe, commands):
    if len(commands) > 8:
        raise CopyError("at most eight SOS diagnostic commands are allowed")
    for command in commands:
        if not command or len(command) > 79 or any(ord(c) < 32 or ord(c) > 126 for c in command):
            raise CopyError("SOS diagnostic command must be one printable ASCII console line")
    if commands and probe not in ("sos", "account", "terminal"):
        raise CopyError("SOS diagnostic commands require an SOS startup probe")


def verify_operator_frame(selection, reply, command):
    """Verify a complete console exchange, not the requested operation's success."""
    if not selection.endswith(">\x07") or not reply.startswith(command + "\r\n"):
        raise CopyError("unexpected SOS operator selection/echo")
    rows = re.findall(r"(?m)^\d{2}\.\d{2} (?:\*\*\*)?sos: ([^\r\n]+)\r?$", reply)
    if len(rows) != 1 or not reply.endswith("\n"):
        raise CopyError("missing/ambiguous complete SOS operator response")
    return rows[0]


def sos_reply_event(data):
    """Find the first complete timestamped response or password question."""
    candidates = []
    response = re.search(rb"(?m)^\d{2}\.\d{2} (?:\*\*\*)?sos: [^\r\n]+\r?\n", data)
    if response:
        candidates.append((response.end(), "response"))
    prompt = data.find(b">password \x07")
    if prompt >= 0:
        candidates.append((prompt + len(b">password \x07"), "password"))
    return min(candidates) if candidates else None


def read_sos_event(session, timeout=15):
    deadline = time.monotonic() + timeout
    while True:
        if len(session.pending) > 65536:
            raise CopyError("SOS dialogue exceeded its output bound")
        event = sos_reply_event(session.pending)
        if event:
            end, kind = event
            result, session.pending = session.pending[:end], session.pending[end:]
            return kind, result.decode("ascii")
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([session.process.stdout], [], [], remaining)[0]:
            raise CopyError("SOS dialogue exceeded its deadline")
        data = os.read(session.process.stdout.fileno(), 65536)
        if not data:
            raise CopyError("SOS dialogue ended before a complete response")
        session.pending += data


def verify_second_disc(label, mounted, space):
    for reply in (label, mounted, space):
        if any(marker in reply for marker in ("***", "giveup", "hard error", "abnormal end",
                                             "syntax error", "not allowed", "process does not exist")):
            raise CopyError("second-disk setup contains a guest failure")
    for reply in (label, mounted):
        if len(re.findall(r"(?m)^end\s+\d+\s*\r?$", reply)) != 1:
            raise CopyError("second-disk utility did not complete normally")
    if len(re.findall(r"(?m)^disc1\s+mounted on\s+0*16\s*\r?$", mounted)) != 1 or \
            not re.search(r"(?m)^disc1\s*:", space):
        raise CopyError("second-disk mount/free-space evidence is missing")


def verify_sos_disc1_storage(descriptor):
    rows = re.findall(r"(?m)^swpsos\s+=set (\d+) (\S+) .*; (\w+)\r?$", descriptor)
    if rows != [("1722", "disc1", "temp")] or "***" in descriptor:
        raise CopyError("SOS swap descriptor is missing, ambiguous or not 1722 segments on disc1")


def verify_second_disc_file(descriptor, data):
    rows = re.findall(r"(?m)^dkeep\s+=set (\d+) disc1 .*; user\r?$", descriptor)
    if len(rows) != 1 or int(rows[0]) != SECOND_DISC_SEGMENTS or \
            "***" in descriptor or data != SECOND_DISC_TEXT:
        raise CopyError("second-disk retained descriptor or file contents differ")


def locate_parser_code(core, program):
    """Locate an unchanged known updater instruction sequence in packed core."""
    signature = program[PARSER_OFFSET:PARSER_OFFSET + len(PARSER_SIGNATURE)]
    if signature != PARSER_SIGNATURE:
        raise CopyError("updater does not match the known parser instruction sequence")
    positions = [i for i in range(0, len(core) - 23, 3) if core[i:i + 24] == signature]
    if len(positions) != 1:
        raise CopyError("parser instruction sequence is absent or ambiguous in saved core")
    return positions[0] // 3 * 2


def read_system_reply(session, record):
    deadline = time.monotonic() + 30
    output = ""
    for _ in range(200):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise CopyError("system command exceeded its deadline")
        line = record(session.until(b"\n", timeout=remaining))
        output += line
        if re.fullmatch(r"\s*to sos\s*", line):
            raise CopyError("system reply interrupted by console transfer to SOS; lifecycle not verified")
        if any(error in line for error in ("bs claims exceeded", "area unknown", "no core",
                                          "not allowed", "not implemented", "syntax error:",
                                          "program unknown", "process unknown", "load error")):
            raise CopyError(f"guest system command rejected: {line.strip()}")
        if line.strip() == "ready":
            return output
    raise CopyError("system command exceeded its output bound")


def start_components(simulator, destination, probe=None, catalog_text="maxp 4", sos_claim=(2500, 200), operator_commands=(), parser_snapshot=False, test_password=None, sos_disc1=False, verify_batch=False, batch_job=False, account_segments=0, account_user_storage=False, interactive_terminal=None):
    session = None
    with (destination / "startup.log").open("x", encoding="ascii") as log:
        def record(output):
            log.write(output)
            log.flush()
            return output

        def system(line):
            os.write(session.master, b"\x1b")
            record(session.until(b"att") + session.until(b"\x07"))
            os.write(session.master, b"s\r")
            record(session.until(b"\x07"))
            os.write(session.master, line.encode("ascii") + b"\r")
            try:
                output = read_system_reply(session, record)
            except CopyError as error:
                if not verify_batch or line != "list" or "console transfer to SOS" not in str(error):
                    raise
                # One bounded, empty response on disposable batch fixtures only.
                record(session.until(b"\x07", timeout=15))
                os.write(session.master, b"\r")
                output = read_system_reply(session, record)
            assert "***" not in output and "syntax error:" not in output and "pause tem" not in output and "pause primo" not in output, output
            return output

        def fp(line):
            os.write(session.master, line.encode("ascii") + b"\r")
            reply = record(session.until(b"\x07", timeout=30))
            if any(marker in reply for marker in ("***", "giveup", "hard error", "abnormal end",
                                                  "syntax error", "not allowed", "process does not exist")):
                raise CopyError(f"guest FP command failed: {line}")
            return reply

        try:
            session = Session(simulator, destination, ("-a",), startup=b"initialize date:")
            record("initialize date:\n")
            os.write(session.master, b"86 09 30 08 00 00\r")
            record(session.until(b"pause startproc finis fp"))
            if sos_disc1:
                system("all diskprep run")
                record(session.until(b"\x07"))
                label = fp("kitlabel 16 disc1 catdisc1 slow 36 9 2030")
                mounted = fp("kiton devno.16")
                space = fp("show")
                verify_second_disc(label, mounted, space)
                (destination / "sos-disc1-setup.txt").write_text(label + mounted + space, encoding="ascii")
                os.write(session.master, b"finis\r")
                record(session.until(b"pause diskprep finis fp", timeout=15))
            if probe == "disc":
                system("all big run")
                record(session.until(b"\x07"))
                os.write(session.master, b"kitlabel 16 disc1 catdisc1 slow 36 9 2030\r")
                label = record(session.until(b"\x07", timeout=30))
                (destination / "kitlabel.txt").write_text(label, encoding="ascii")
                if any(marker in label for marker in ("***", "abnormal end", "syntax error", "not allowed",
                                                     "giveup", "hard error", "process does not exist")):
                    raise CopyError("second-disk kitlabel failed")
                os.write(session.master, b"kiton devno.16\r")
                mounted = record(session.until(b"\x07", timeout=30))
                (destination / "kiton.txt").write_text(mounted, encoding="ascii")
                if any(marker in mounted for marker in ("***", "giveup", "hard error", "abnormal end")):
                    raise CopyError("second-disk kiton failed")
                os.write(session.master, b"show\r")
                space = record(session.until(b"\x07", timeout=15))
                (destination / "second-disc-space.txt").write_text(space, encoding="ascii")
                verify_second_disc(label, mounted, space)
                (destination / "exchange/discinput").write_bytes(SECOND_DISC_TEXT)
                for line in ("dsource=fromhost discinput", "dkeep=set 216 disc1",
                             "dkeep=copy dsource", "scope user dkeep"):
                    fp(line)
                descriptor = fp("lookup dkeep")
                fp("before=tohost dkeep")
                verify_second_disc_file(descriptor, (destination / "exchange/before").read_bytes())
                (destination / "second-disc-before.txt").write_text(descriptor, encoding="ascii")
                os.write(session.master, b"finis\r")
                record(session.until(b"pause big finis fp", timeout=15))
                maximum = system("max")
                (destination / "second-disc-mount.txt").write_text(mounted + maximum, encoding="ascii")
                os.write(session.master, b"\x05")
                record(session.until(b"\nsim4000> "))
                record(session.command(b"show media\n"))
                session.finish()
                session.close()
                session = None
                record("\nRestarting emulator with the same private UNIT1 overlay.\n")
                session = Session(simulator, destination, ("-a",), startup=b"initialize date:")
                os.write(session.master, b"86 09 30 08 00 00\r")
                record(session.until(b"pause startproc finis fp"))
                system("all verify run")
                record(session.until(b"\x07"))
                remounted = fp("kiton devno.16")
                descriptor = fp("lookup dkeep")
                fp("after=tohost dkeep")
                verify_second_disc_file(descriptor, (destination / "exchange/after").read_bytes())
                if descriptor != (destination / "second-disc-before.txt").read_bytes().decode("ascii"):
                    raise CopyError("second-disk file metadata changed after restart")
                (destination / "second-disc-after.txt").write_text(remounted + descriptor, encoding="ascii")
                os.write(session.master, b"finis\r")
                record(session.until(b"pause verify finis fp", timeout=15))
                system("max")
                os.write(session.master, b"\x05")
                record(session.until(b"\nsim4000> "))
                session.finish()
                (destination / "second-disc.json").write_text(json.dumps({
                    "device": 16, "document": "disc1", "file": "dkeep",
                    "file_segments": SECOND_DISC_SEGMENTS, "file_bytes": len(SECOND_DISC_TEXT),
                    "before_sha256": digest(destination / "exchange/before"),
                    "after_sha256": digest(destination / "exchange/after"),
                    "base_sha256": digest(destination / "UNIT1"),
                    "overlay_sha256": digest(destination / "UNIT1.overlay"),
                    "emulator_restart_verified": True, "metadata_unchanged": True,
                    "mips_resource_use_verified": False,
                }, indent=2) + "\n", encoding="ascii")
                return
            system("max")
            system("read starttem")
            tem = system("list")
            verify_component_listing(tem, ("tem",))
            system("read startprimo")
            both = system("list")
            verify_component_listing(both, ("tem", "primo"))
            system("max")
            os.write(session.master, b"\x05")
            record(session.until(b"\nsim4000> "))
            cpu = record(session.command(b"show cpu\n"))
            assert "protection=off" in cpu and "ix=on" in cpu, cpu
            record(session.command(b"step 1000000\n"))
            os.write(session.master, b"continue\n")
            verify_component_listing(system("list"), ("tem", "primo"))
            if probe in ("primo", "primo-devices", "primo-transfer"):
                commands = (("display", None), ("zzzzz", "command unknown"), ("display", None))
                if probe == "primo-devices":
                    commands = tuple((line, "diagnostic") for line in DEVICE_COMMANDS)
                observations = []
                for command, error in commands:
                    os.write(session.master, b"\x1b")
                    record(session.until(b"att") + session.until(b"\x07"))
                    os.write(session.master, b"primo\r")
                    record(session.until(b"\x07", timeout=15))
                    os.write(session.master, command.encode("ascii") + b"\r")
                    if error == "diagnostic":
                        reply = record(session.until(b"primo:", timeout=15) + session.until(b"\r\n", timeout=15))
                        observations.append({"command": command, "reply": reply})
                    else:
                        marker = b"primo:ready\r\n" if error is None else b"primo:***command unknown\r\n"
                        reply = record(session.until(marker, timeout=15))
                        verify_primo_reply(reply, error)
                if probe == "primo-devices":
                    (destination / "primo-devices.json").write_text(json.dumps(observations, indent=2) + "\n", encoding="ascii")
                    verify_primo_devices(observations)
                if probe == "primo-transfer":
                    system("all prbprep size 150000 temp disc 500 20 run")
                    record(session.until(b"\x07"))
                    (destination / "exchange/prbsource").write_bytes((ROOT / "tests/fixtures/primo_transport.alg").read_bytes())
                    (destination / "exchange/prbinput").write_bytes(PRIMO_TEXT)
                    fp("psrc=fromhost prbsource")
                    fp("ptmp=fromhost prbinput")
                    fp("pin=copy ptmp")
                    fp("plp=set lp console1")
                    fp("prbprog=algol psrc connect.no")
                    reply = fp("prbprog")
                    (destination / "primo-transfer.txt").write_text(reply, encoding="ascii")
                    transportno = verify_primo_transfer(reply)
                    (destination / "primo-transfer.json").write_text(json.dumps({
                        "transport_number": transportno, "transfer_result": 0,
                        "reply_code": 0, "completed_state": 5, "output_verified": True,
                        "receiver": "plp", "external_process": "console1", "kind": 14,
                        "real_printer_hardware_verified": False,
                        "source_sha256": digest(destination / "exchange/prbsource"),
                        "text_sha256": digest(destination / "exchange/prbinput"),
                    }, indent=2) + "\n", encoding="ascii")
                    os.write(session.master, b"finis\r")
                    record(session.until(b"pause prbprep finis fp", timeout=15))
                verify_component_listing(system("list"), ("tem", "primo"))
            elif probe in ("catalog", "sos", "account", "terminal"):
                if probe in ("sos", "account", "terminal"):
                    system("new catprep size 150000 base -8388607 8388605")
                    system("login -8388606 8388605 temp disc 500 20 perm disc 200 10 run")
                else:
                    system("new catprep size 150000 temp disc 500 20 perm disc 200 10 run")
                record(session.until(b"\x07"))
                (destination / "exchange/catinput").write_text(catalog_text + "\n", encoding="ascii")
                os.write(session.master, b"catspec=fromhost catinput\r")
                record(session.until(b"\x07", timeout=15))
                os.write(session.master, b"soscat=upsoscat catspec\r")
                response = record(session.until(b"\x07", timeout=15))
                (destination / "catalog-response.txt").write_text(response, encoding="ascii")
                if parser_snapshot:
                    os.write(session.master, b"\x05")
                    record(session.until(b"\nsim4000> "))
                    record(session.command(b"save-core parser-core.bin 0 524288\n"))
                    packed_core = (destination / "parser-core.bin").read_bytes()
                    program = (destination / "exchange/upsoscat").read_bytes()
                    address = locate_parser_code(packed_core, program)
                    record(session.command(f"break {address}\n".encode("ascii")))
                    os.write(session.master, b"continue\n")
                    os.write(session.master, b"sosagain=upsoscat catspec\r")
                    stopped = record(session.until(b"\nsim4000> ", timeout=15))
                    if f"execution breakpoint hit at {address:06x}" not in stopped:
                        raise CopyError("parser replay did not hit its expected instruction breakpoint")
                    record(session.command(b"save-core parser-stopped.bin 0 524288\n"))
                    current = (destination / "parser-stopped.bin").read_bytes()
                    if current[address // 2 * 3:address // 2 * 3 + 24] != PARSER_SIGNATURE:
                        raise CopyError("parser replay hit reused storage, not the expected native code")
                    trace = record(session.command(b"trace 20000\n"))
                    (destination / "parser-trace.txt").write_text(stopped + trace, encoding="ascii")
                    (destination / "parser.json").write_text(json.dumps({
                        "program_sha256": digest(destination / "exchange/upsoscat"),
                        "packed_program_offset": PARSER_OFFSET,
                        "breakpoint_address": address, "trace_calls": 20000,
                        "pre_replay_core_sha256": digest(destination / "parser-core.bin"),
                        "stopped_core_sha256": digest(destination / "parser-stopped.bin"),
                        "instruction_signature_verified_at_stop": True,
                        "replay_completion_verified": False,
                    }, indent=2) + "\n", encoding="ascii")
                    session.finish()
                    return
                os.write(session.master, b"lookup soscat\r")
                descriptor = record(session.until(b"\x07", timeout=15))
                if re.search(r"(?m)^soscat\s+=set \d+ disc ", descriptor):
                    os.write(session.master, b"catalogbin=tohost conv.bin soscat\r")
                    record(session.until(b"\x07", timeout=15))
                packed = destination / "exchange/catalogbin"
                outcome = catalog_outcome(response, descriptor, packed.read_bytes() if packed.exists() else None)
                (destination / "catalog-outcome.json").write_text(
                    json.dumps(outcome, indent=2) + "\n", encoding="ascii")
                if probe in ("sos", "account", "terminal"):
                    expected = {"account": ACCOUNT_INPUT, "terminal": TERMINAL_INPUT, "sos": "maxp 4"}[probe]
                    if account_segments:
                        clause = ACCOUNT_RESOURCE_CLAUSE.replace("key1 0 0 key2 0 0 key3 0 0", "key1 2 6 key2 2 6 key3 2 6") if account_user_storage else ACCOUNT_RESOURCE_CLAUSE
                        expected = expected.replace('\npass "test"', '\npass "test"' + clause)
                    if catalog_text != expected:
                        raise CopyError("SOS startup requires the selected known catalog fixture")
                    verifier = {"account": verify_account_catalog, "terminal": verify_terminal_catalog,
                                "sos": verify_empty_catalog}[probe]
                    args = (account_segments, account_user_storage) if probe in ("account", "terminal") else ()
                    verifier(response, descriptor, (destination / "exchange/catalogbin").read_bytes(), *args)
                    os.write(session.master, b"scope user soscat\r")
                    retained = record(session.until(b"\x07", timeout=15))
                    assert "***" not in retained, retained
                    start_name = "startsos"
                    if batch_job:
                        if batch_job == "retain":
                            os.write(session.master, b"lookup batchproof\r")
                            absent = record(session.until(b"\x07", timeout=15))
                            (destination / "batch-before-lookup.txt").write_text(absent, encoding="ascii")
                            if len(re.findall(r"(?m)^\*\*\*lookup batchproof unknown\r?$", absent)) != 1:
                                raise CopyError("batchproof must be absent before the retained-copy job")
                        (destination / "exchange/batchinput").write_bytes(BATCH_JOBS[batch_job])
                        for line in ("batchtmp=fromhost batchinput", "batchjob=copy batchtmp", "scope user batchjob"):
                            fp(line)
                        fp("fpprogram=tohost conv.bin fp")
                        if batch_job == "retain":
                            fp("batchorig=tohost conv.bin batchjob")
                    if sos_claim != (2500, 200) or sos_disc1:
                        source = (destination / "exchange/startsos").read_bytes()
                        (destination / "exchange/sosboot").write_bytes(private_sos_start(
                            source, *sos_claim, document="disc1" if sos_disc1 else "disc"))
                        for line in ("sostmp=fromhost sosboot", "sosboot=copy sostmp", "scope user sosboot"):
                            os.write(session.master, line.encode("ascii") + b"\r")
                            reply = record(session.until(b"\x07", timeout=15))
                            assert "***" not in reply, reply
                        start_name = "sosboot"
                    os.write(session.master, b"claim\r")
                    record(session.until(b"\x07", timeout=15))
                    os.write(session.master, b"show\r")
                    record(session.until(b"\x07", timeout=15))
                    os.write(session.master, b"finis\r")
                    record(session.until(b"pause catprep finis fp", timeout=15))
                    system("list")
                    startup_offset = log.tell()
                    system(f"read {start_name}")
                    os.write(session.master, b"\x05")
                    record(session.until(b"\nsim4000> "))
                    record(session.command(b"step 1000000\n"))
                    os.write(session.master, b"continue\n")
                    listing = system("list")
                    verify_sos_startup((destination / "startup.log").read_bytes()[startup_offset:].decode("ascii"), listing)
                    if sos_disc1:
                        system("all storage run")
                        record(session.until(b"\x07"))
                        storage = fp("lookup swpsos")
                        (destination / "sos-disc1-storage.txt").write_text(storage, encoding="ascii")
                        verify_sos_disc1_storage(storage)
                        os.write(session.master, b"finis\r")
                        record(session.until(b"pause storage finis fp", timeout=15))
                        verify_component_listing(system("list"), ("tem", "primo", "sos"))
                    if interactive_terminal:
                        from mips_terminal import run_terminal
                        port, mode = interactive_terminal
                        run_terminal(port, mode, destination, system,
                                     lambda listing: verify_component_listing(listing, ("tem", "primo", "sos")))
                    for index, command in enumerate(operator_commands):
                        os.write(session.master, b"\x1b")
                        record(session.until(b"att") + session.until(b"\x07"))
                        os.write(session.master, b"sos\r")
                        selection = record(session.until(b"\x07", timeout=15))
                        os.write(session.master, command.encode("ascii") + b"\r")
                        kind, output = read_sos_event(session)
                        reply = record(output)
                        if kind == "password":
                            (destination / f"password-{index}.txt").write_text(selection + reply, encoding="ascii")
                            if test_password is None:
                                raise CopyError("SOS requested a password; only explicit --test-password test/wrong is supported")
                            os.write(session.master, test_password.encode("ascii") + b"\r")
                            kind, output = read_sos_event(session)
                            reply += record(output)
                            if kind != "response":
                                raise CopyError("SOS repeated its password question instead of completing the test exchange")
                        verify_operator_frame(selection, reply, command)
                        (destination / f"operator-{index}.txt").write_text(selection + reply, encoding="ascii")
                    if operator_commands:
                        if batch_job:
                            os.write(session.master, b"\x05")
                            record(session.until(b"\nsim4000> "))
                            record(session.command(b"step 1000000\n"))
                            record(session.command(b"save-core batch-job-core.bin 0 524288\n"))
                            os.write(session.master, b"continue\n")
                        if verify_batch:
                            record(session.until(b"\x07", timeout=15))
                            os.write(session.master, b"\x05")
                            record(session.until(b"\nsim4000> "))
                            record(session.command(b"step 1000000\n"))
                            os.write(session.master, b"continue\n")
                        listing = system("list")
                        if batch_job:
                            (destination / "batch-finished.txt").write_text(listing, encoding="ascii")
                            output = (destination / "startup.log").read_bytes()[startup_offset:].decode("ascii")
                            verify_batch_job(output, listing, batch_job)
                            if batch_job == "retain":
                                system("all jobcheck base 0 0 login 0 0 user 0 0 project 0 0 run")
                                record(session.until(b"\x07"))
                                descriptor = fp("lookup batchproof")
                                (destination / "batch-retained.txt").write_text(descriptor, encoding="ascii")
                                fp("batchresult=tohost batchproof")
                                fp("batchpacked=tohost conv.bin batchproof")
                                verify_retained_batch_file(descriptor,
                                    (destination / "exchange/batchresult").read_bytes(),
                                    (destination / "exchange/batchorig").read_bytes(),
                                    (destination / "exchange/batchpacked").read_bytes())
                                os.write(session.master, b"finis\r")
                                record(session.until(b"pause jobcheck finis fp", timeout=15))
                                verify_component_listing(system("list"), ("tem", "primo", "sos"))
                            (destination / "batch-job.json").write_text(json.dumps({
                                "command": "batch test jobfile batchjob", "job_text": BATCH_JOBS[batch_job].decode("ascii"),
                                "advance_calls": 1000000, "normal_finish_verified": True,
                                "workload": batch_job,
                                "account_key0_disc_segments": account_segments,
                                "account_key3_user_storage": account_user_storage,
                                "retained_text_sha256": digest(destination / "exchange/batchresult") if batch_job == "retain" else None,
                                "retained_packed_sha256": digest(destination / "exchange/batchpacked") if batch_job == "retain" else None,
                                "source_packed_sha256": digest(destination / "exchange/batchorig") if batch_job == "retain" else None,
                                "packed_logical_bytes_verified": len(BATCH_JOBS["retain"]) + 1 if batch_job == "retain" else None,
                                "sector_padding_compared": False,
                                "output_checked": batch_job in ("message", "lookup"),
                                "copy_summary_verified": batch_job in ("read", "copy", "roundtrip", "retain"),
                                "child_removed_verified": True, "components_healthy": True,
                                "core_sha256": digest(destination / "batch-job-core.bin"),
                                "terminal_login_verified": False, "file_copy_verified": batch_job == "retain",
                            }, indent=2) + "\n", encoding="ascii")
                        if verify_batch:
                            initial_listing = listing
                            (destination / "batch-enrolled.txt").write_text(listing, encoding="ascii")
                            os.write(session.master, b"\x05")
                            record(session.until(b"\nsim4000> "))
                            advance = record(session.command(b"step 1000000\n"))
                            record(session.command(b"save-core batch-core.bin 0 524288\n"))
                            state = batch_core_state((destination / "batch-core.bin").read_bytes())
                            state["core_sha256"] = digest(destination / "batch-core.bin")
                            (destination / "batch-state.json").write_text(json.dumps(state, indent=2) + "\n", encoding="ascii")
                            os.write(session.master, b"continue\n")
                            listing = system("list")
                            (destination / "batch-advanced.txt").write_text(advance + listing, encoding="ascii")
                            verify_batch_enrollment(reply, initial_listing)
                            verify_batch_enrollment(reply, listing)
                        else:
                            verify_component_listing(listing, ("tem", "primo", "sos"))
            os.write(session.master, b"\x05")
            record(session.until(b"\nsim4000> "))
            session.finish()
        finally:
            if session is not None:
                if session.pending:
                    record(session.pending.decode("ascii", errors="backslashreplace"))
                session.close()


def inspect(builder, destination, start=False, simulator=None, probe=None, catalog_text="maxp 4", sos_claim=(2500, 200), operator_commands=(), parser_snapshot=False, test_password=None, sos_disc1=False, verify_batch=False, batch_job=False, account_segments=0, account_user_storage=False, interactive_terminal=None):
    if interactive_terminal and (interactive_terminal not in ("go", "run") or probe != "terminal" or batch_job or verify_batch or operator_commands or test_password):
        raise CopyError("interactive terminal requires the fixed terminal fixture and no other dialogue probe")
    if account_segments not in (0, 30) or account_segments and probe not in ("account", "terminal"):
        raise CopyError("account segments require the fixed account/terminal 0/30-segment fixture")
    if account_user_storage and (account_segments != 30 or probe not in ("account", "terminal")):
        raise CopyError("user storage requires the fixed 30-segment account/terminal fixture")
    if account_segments:
        expected = {"account": ACCOUNT_INPUT, "terminal": TERMINAL_INPUT}[probe]
        if catalog_text != expected:
            raise CopyError("account resource fixture requires the unchanged fixed catalog input")
        clause = ACCOUNT_RESOURCE_CLAUSE.replace("key1 0 0 key2 0 0 key3 0 0", "key1 2 6 key2 2 6 key3 2 6") if account_user_storage else ACCOUNT_RESOURCE_CLAUSE
        catalog_text = expected.replace('\npass "test"', '\npass "test"' + clause)
    validate_operator_commands(probe, operator_commands)
    if batch_job and (batch_job not in BATCH_JOBS or probe not in ("account", "terminal") or list(operator_commands) != ["batch test jobfile batchjob"] or test_password != "test" or verify_batch):
        raise CopyError("batch job requires the fixed account, jobfile command and dummy password; not --verify-batch")
    if verify_batch and (probe not in ("account", "terminal") or list(operator_commands) != ["batch test"] or test_password != "test"):
        raise CopyError("batch verification requires the fixed account/terminal fixture, batch test and dummy password test")
    if test_password is not None and (test_password not in ("test", "wrong") or not operator_commands):
        raise CopyError("password diagnostics require operator commands and a fixed dummy password")
    if parser_snapshot and probe != "catalog":
        raise CopyError("parser snapshots require a disposable catalog probe")
    if sos_disc1 and probe not in ("sos", "account", "terminal"):
        raise CopyError("SOS disc1 resources require an SOS/account/terminal startup probe")
    media = {ROOT / "src/monitor/MONITOR", ROOT / "src/disks/UNIT0",
             ROOT / "src/disks/UNIT0.original", ROOT / "src/monitor/MONITOR.protected",
             ROOT / "src/disks/UNIT0.protected"}
    before = {path: digest(path) for path in media}
    destination.mkdir()  # Never overwrite earlier evidence or personal work.
    exchange = destination / "exchange"
    exchange.mkdir()
    shutil.copyfile(ROOT / "src/monitor/MONITOR", destination / "MONITOR")
    shutil.copyfile(ROOT / "src/disks/UNIT0", destination / "UNIT0")
    copies = {path: digest(path) for path in (destination / "MONITOR", destination / "UNIT0")}
    extra_disk = ""
    endpoint = (ports()[0], interactive_terminal) if interactive_terminal else None
    extra_terminal = ("DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
                      f"TELNET DEVNO=4 ADDRESS=127.0.0.1 PORT={endpoint[0]} ATTENTION=CTRL-A\n") if endpoint else ""
    if probe == "disc" or sos_disc1:
        creation = subprocess.run([str(simulator or ROOT / "build/sim4000"), "--mkdisc", "UNIT1"],
                                  cwd=destination, capture_output=True, text=True, timeout=10)
        (destination / "mkdisc.txt").write_text(creation.stdout + creation.stderr, encoding="ascii")
        if creation.returncode != 0 or (destination / "UNIT1").read_bytes() != bytes(180 * 768):
            raise CopyError("integrated mkdisc did not create the expected blank second disk")
        copies[destination / "UNIT1"] = digest(destination / "UNIT1")
        extra_disk = "DEFINE RC433X DEVNO=16 FILE=UNIT1 OVERLAY=UNIT1.overlay\n"
    (destination / "RC4000.cfg").write_text(
        "PROTECTION MODE=OFF\nMONITOR FILE=MONITOR\nCORESIZE=0,1048576\n"
        "THROTTLE VALUE=OFF\nDEFINE CONSOLE DEVNO=2 INTNO=6,3\n"
        "DEFINE CLOCK DEVNO=3 INTNO=11\nDEFINE RC433X DEVNO=15 FILE=UNIT0\n"
        "DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT=exchange\n" + extra_disk + extra_terminal, encoding="ascii")
    try:
        with (destination / "inventory.log").open("xb") as log:
            result = subprocess.run(
                [str(builder), "--inspect-mips",
                 str(destination / "MONITOR"),
                 str(destination / "UNIT0"), str(exchange)],
                stdout=log, stderr=subprocess.STDOUT, timeout=120)
        if result.returncode != 0:
            raise CopyError(f"MIPS inventory failed; inspect {destination / 'inventory.log'}")
        verify_inventory(destination)
        if {path: digest(path) for path in copies} != copies:
            raise CopyError("MIPS inventory modified its runtime copies")
        if {path: digest(path) for path in media} != before:
            raise CopyError("MIPS inventory modified source media")
        identities = "".join(f"{name}-sha256={digest(exchange / name)}\n" for name in (*TEXTS, *BINARIES))
        (destination / "inventory.txt").write_text(
            "MIPS/TS startup material exported, not executed by the inventory helper.\n"
            "Compatibility profile, IX enabled, volatile writes on private media copies.\n"
            + identities, encoding="ascii")
        if start:
            start_components(simulator or ROOT / "build/sim4000", destination, probe, catalog_text, sos_claim, operator_commands, parser_snapshot, test_password, sos_disc1, verify_batch, batch_job, account_segments, account_user_storage, endpoint)
            if {path: digest(path) for path in copies} != copies:
                raise CopyError("component startup modified its base copies")
            if {path: digest(path) for path in media} != before:
                raise CopyError("component startup modified source media")
            (destination / "components.txt").write_text(
                "Second disk initialized and mounted; MIPS components not started.\n" if probe == "disc" else
                "TEM and PRIMO created from unchanged startup scripts on a volatile private session.\n"
                "Process listings show requested sizes and zero stop counts.\n"
                "These listings do not establish terminal/spool service or a usable SOS login.\n", encoding="ascii")
            if probe:
                descriptions = {
                    "disc": "Integrated mkdisc, guest kitlabel/kiton, retained disc1 file I/O and remount/read after emulator restart verified. MIPS-on-disc1 not verified.\n",
                    "primo": "PRIMO display/unknown-command/display replies verified; spool I/O unverified.\n",
                    "primo-devices": "PRIMO fixed signup/display/signoff replies retained as diagnostics; no transport verified.\n",
                    "primo-transfer": "Guest ALGOL transfer defines and waits for a completed PRIMO transport; exact text received through a console-backed printer descriptor. No physical printer claim.\n",
                    "catalog": "Experimental updater input/response, classified outcome and available packed output retained; no login verified.\n",
                    "sos": f"SOS 5.0 initialization/started and active TEM/PRIMO/SOS verified; private claim={sos_claim}. No account/login verified.\n",
                    "account": f"Named test-account catalog bytes and SOS startup verified; private claim={sos_claim}. Terminal connection/login not verified.\n",
                    "terminal": f"Fixed account/terminal catalog bytes and SOS startup verified; private claim={sos_claim}. Terminal connection/login not verified.\n",
                }
                description = ("Bounded parser replay stopped and traced; no replay completion or login verified.\n"
                               if parser_snapshot else descriptions[probe])
                (destination / "probe.txt").write_text(description, encoding="ascii")
            if interactive_terminal:
                (destination / "probe.txt").write_text(
                    f"Fixed device-4 Telnet {interactive_terminal} login, wrong-password rejection, FP message/lookup and normal logout verified.\n"
                    "Compatibility profile only; no multiterminal application or restart persistence verified.\n", encoding="ascii")
            if sos_disc1:
                (destination / "sos-disc1.txt").write_text(
                    f"SOS startup verified with private permanent claim: disc1 {sos_claim[0]} {sos_claim[1]}.\n"
                    "The original startup and base media remain unchanged. No login/job verified.\n", encoding="ascii")
            if operator_commands:
                (destination / "operator.txt").write_text(
                    "Timestamped SOS console replies retained as diagnostic evidence only.\n"
                    "A rejected command is not a successful operation or login.\n"
                    "TEM/PRIMO/SOS remain active after the exchange.\n", encoding="ascii")
            if verify_batch:
                (destination / "batch.txt").write_text(
                    "Fixed dummy batch account enrolled; child and SOS parent states verified before and after 1000000 calls.\n"
                    "SOS stop count 1 is expected while its child exists. No job completion or terminal login verified.\n", encoding="ascii")
    finally:
        if {path: digest(path) for path in copies} != copies:
            raise CopyError("private base media changed during MIPS inspection/startup")
        if {path: digest(path) for path in media} != before:
            raise CopyError("source media changed during MIPS inspection")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new exclusive evidence directory")
    parser.add_argument("--builder", type=Path,
                        default=ROOT / "build-protection/sim4000_build_host_image")
    parser.add_argument("--start-components", action="store_true",
                        help="also probe TEM/PRIMO startup on the disposable session; not a login test")
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    probes = parser.add_mutually_exclusive_group()
    probes.add_argument("--probe-primo", action="store_true", help="inspect PRIMO's console display (experimental)")
    probes.add_argument("--probe-primo-devices", action="store_true", help="record fixed private PRIMO signup/display/signoff device diagnostics")
    probes.add_argument("--probe-primo-transfer", action="store_true", help="compile a fixed private ALGOL transfer-to-console experiment")
    probes.add_argument("--probe-catalog", action="store_true", help="retain experimental upsoscat input/response and packed result")
    probes.add_argument("--probe-sos", action="store_true", help="probe SOS startup with a private empty test catalog, no login")
    probes.add_argument("--probe-account", action="store_true", help="probe SOS startup with the fixed disposable test-account catalog; not a login test")
    probes.add_argument("--probe-terminal", action="store_true", help="probe SOS startup with a fixed account/terminal catalog; not a connection or login test")
    probes.add_argument("--probe-second-disc", action="store_true", help="create and initialize a private second RC433X disk; no MIPS startup")
    parser.add_argument("--catalog-text", default="maxp 4", help="experimental disposable upsoscat input text")
    parser.add_argument("--sos-segments", type=int, default=2500, help="private SOS permanent segment claim")
    parser.add_argument("--sos-entries", type=int, default=200, help="private SOS permanent entry claim")
    parser.add_argument("--operator-command", action="append", default=[],
                        help="retain one timestamped SOS operator reply after startup (repeatable, diagnostic only)")
    parser.add_argument("--parser-snapshot", action="store_true",
                        help="save core and trace a bounded diagnostic catalog replay at the known term parser")
    parser.add_argument("--test-password", choices=("test", "wrong"),
                        help="answer one SOS password question per command with fixed dummy test data")
    parser.add_argument("--sos-disc1", action="store_true",
                        help="initialize a private second disk and move only the SOS permanent claim to disc1")
    parser.add_argument("--verify-batch", action="store_true",
                        help="check fixed batch test enrollment and child state across a bounded advance")
    parser.add_argument("--batch-job", action="store_true",
                        help="prepare and run the fixed disposable batchjob via the documented jobfile option")
    parser.add_argument("--batch-workload", choices=tuple(BATCH_JOBS), default="finis",
                        help="fixed batch diagnostic workload (requires --batch-job)")
    parser.add_argument("--account-segments", type=int, choices=(0, 30), default=0,
                        help="fixed account key0 disc segment claim (default 0; experimental comparison 30)")
    parser.add_argument("--account-user-storage", action="store_true",
                        help="grant cumulative key1..3 disc quotas: two entries/six segments (requires --account-segments 30)")
    parser.add_argument("--interactive-terminal", choices=("go", "run"),
                        help="probe fixed password/FP/logout dialogue on a private loopback device-4 endpoint")
    args = parser.parse_args()
    if not args.batch_job and args.batch_workload != "finis":
        parser.error("--batch-workload requires --batch-job")
    if args.batch_job and not args.operator_command:
        args.operator_command = ["batch test jobfile batchjob"]
    if (args.probe_account or args.probe_terminal) and args.catalog_text != "maxp 4":
        parser.error("account/terminal startup probes use fixed fixtures, not --catalog-text")
    probe = "primo-transfer" if args.probe_primo_transfer else "primo-devices" if args.probe_primo_devices else "primo" if args.probe_primo else "catalog" if args.probe_catalog else "sos" if args.probe_sos else "account" if args.probe_account else "terminal" if args.probe_terminal else "disc" if args.probe_second_disc else None
    catalog_text = TERMINAL_INPUT if args.probe_terminal else ACCOUNT_INPUT if args.probe_account else args.catalog_text
    inspect(args.builder.resolve(), args.destination.resolve(), args.start_components or probe is not None,
            args.simulator.resolve(), probe, catalog_text, (args.sos_segments, args.sos_entries), args.operator_command, args.parser_snapshot, args.test_password, args.sos_disc1, args.verify_batch, args.batch_workload if args.batch_job else False, args.account_segments, args.account_user_storage, args.interactive_terminal)
    outcome_path = args.destination / "catalog-outcome.json"
    if outcome_path.exists():
        print("Catalog updater outcome: " + json.loads(outcome_path.read_text(encoding="ascii"))["status"])
    if args.parser_snapshot:
        print("Parser replay stopped after bounded trace; replay completion not verified")
    print("MIPS/TS inventory/probe completed; service operation not yet verified")


if __name__ == "__main__":
    main()
