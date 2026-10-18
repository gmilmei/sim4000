#!/usr/bin/env python3
"""Opt-in: verify bounded FP command-file workflows in either monitor profile."""

import argparse
import os
from pathlib import Path
import re

from test_operator_cli import Session
from test_protected_catalog import digest, claim_report, free_space

ROOT = Path(__file__).resolve().parents[2]
INPUTS = {
    "outerin": ("nested_outer.fp", "outercmd"),
    "innerin": ("nested_inner.fp", "innercmd"),
    "errinput": ("command_error.fp", "errcmd"),
    "errouter": ("command_error_outer.fp", "errtop"),
    "condinput": ("conditional_commands.fp", "condcmd"),
    "statusin": ("assembly_status.fp", "checkcmd"),
    "badinput": ("slang_error.sl", "checksrc"),
    "goodinput": ("slang_integer.sl", "checkvalid"),
    "listinput": ("slang_commands.fp", "slcmd"),
}
CODE = b"".join(w.to_bytes(3, "big") for w in
                (0, 0, 0x2c007b, 0x2d0141, 0x1c0002, 0x348000))


def events(output):
    text = output.replace("\r", "")
    return [name or error for name, error in re.findall(
        r"(?m)^(\w+)\s+=set [^\n]*|^(\*\*\*[^\n]*)$", text)]


def verify_binary(exchange):
    assert not (exchange / "badcheck").exists(), "failed assembly was exported"
    data = (exchange / "checkbin").read_bytes()
    assert len(data) == 768 and data[:len(CODE)] == CODE, (len(data), data[:18].hex())


def verify_listing(exchange):
    data = (exchange / "sllisting").read_bytes()
    assert not any(c in data for c in (b"\r", b"\0", b"\x19")), data
    text = data.decode("ascii")
    assert "***" not in text and text.count("slang ok 1/12/1") == 1, text
    rows = re.findall(r"(?m)^.*\b(?:al w[01] \d+|wa w0 2|jl\. 0).*$", text)
    instructions = [re.search(r"\b(?:al w[01] \d+|wa w0 2|jl\. 0)", row)[0] for row in rows]
    assert instructions == ["al w0 123", "al w1 321", "wa w0 2", "jl. 0"], instructions
    binary = (exchange / "slbinary").read_bytes()
    assert len(binary) == 768 and binary[:len(CODE)] == CODE, (len(binary), binary[:18].hex())
    assert (exchange / "sloriginal").read_bytes() == (ROOT / "tests/fixtures/slang_integer.sl").read_bytes()


def loop_state(output, first, compatibility=False):
    match = re.search(r"IC=([0-9a-f]{6}) W0=0001bc W1=000141 ", output)
    assert match and "stopped=yes" in output and "exceptions=000" in output, output
    assert f"protection={'off' if compatibility else 'rc4000'} " in output and "ix=on" in output, output
    if not compatibility:
        assert "PR=bf" in output and "monitor=no" in output, output
    address = int(match[1], 16)
    assert first + 6 <= address < first + 100000 and address % 2 == 0, output
    return address


def verify_loop_code(output, address):
    words = re.findall(r"(?m)^[ >]*([0-9a-f]{6}): ([0-9a-f]{6})  ", output)
    assert words == [(f"{address - 6 + i * 2:06x}", word) for i, word in
                     enumerate(("2c007b", "2d0141", "1c0002", "348000"))], words


def run(simulator, directory, compatibility=False, execute_slang=False):
    profile = "compatibility" if compatibility else "protected"
    monitor = ROOT / ("src/monitor/MONITOR" if compatibility else "src/monitor/MONITOR.protected")
    disk = ROOT / ("src/disks/UNIT0" if compatibility else "src/disks/UNIT0.protected")
    sources = {monitor, disk, ROOT / "src/monitor/MONITOR", ROOT / "src/disks/UNIT0",
               ROOT / "src/disks/UNIT0.original"}
    sources.update(ROOT / "tests/fixtures" / f for f, _ in INPUTS.values())
    before = {path: digest(path) for path in sources}
    directory.mkdir()  # Never overwrite previous evidence or a working session.
    exchange = directory / "exchange"
    exchange.mkdir()
    transfer_in, transfer_out = ("fromhost", "tohost") if compatibility else ("rcin", "rcout")
    inputs = {}
    for host, (filename, _) in INPUTS.items():
        data = (ROOT / "tests/fixtures" / filename).read_bytes()
        if not compatibility:
            # Keep the reviewed fixture logic; only select the matching transfer.
            data = data.replace(b"=tohost ", b"=rcout ")
        inputs[host] = data
        (exchange / host).write_bytes(data)
    (directory / "RC4000.cfg").write_text(
        f"PROTECTION MODE={'OFF' if compatibility else 'RC4000'}\n"
        f"MONITOR FILE={monitor}\nCORESIZE=0,1048576\nTHROTTLE VALUE=OFF\n"
        "DEFINE CONSOLE DEVNO=2 INTNO=6,3\nDEFINE CLOCK DEVNO=3 INTNO=11\n"
        f"DEFINE RC433X DEVNO=15 FILE={disk}\n"
        f"DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT={exchange}\n", encoding="ascii")
    session = None
    with (directory / "session.log").open("x", encoding="ascii") as log:
        def record(output):
            log.write(output)
            log.flush()
            return output

        def guest(line, expected_events=None):
            os.write(session.master, line.encode("ascii") + b"\r")
            output = record(session.until(b"\x07", timeout=30))
            assert " break fp" not in output and "***fp reinitialized" not in output, output
            assert "syntax error:" not in output, output
            if expected_events is None:
                assert "***" not in output, output
            else:
                assert events(output) == expected_events, output
                assert output.count("***") == sum(e.startswith("***") for e in expected_events), output
            return output

        try:
            session = Session(simulator, directory, ("-a",), startup=b"initialize date:")
            record("initialize date:\n")
            os.write(session.master, b"86 09 30 08 00 00\r")
            record(session.until(b"pause startproc finis fp"))
            os.write(session.master, b"\x1b")
            record(session.until(b"att") + session.until(b"\x07"))
            guest("s")
            guest("new cmdcheck size 100000 temp disc 500 20 run")
            initial_claim = claim_report(guest("claim"))
            initial_space = free_space(guest("show"))
            for host, (_, name) in INPUTS.items():
                guest(f"{name}={transfer_in} {host}")

            guest("slsrc=copy checkvalid")
            listing_console = guest("i slcmd")
            assert events(listing_console) == [], listing_console
            assert not any(marker in listing_console for marker in
                           ("al w0 123", "al w1 321", "slang ok")), listing_console
            guest(f"sloriginal={transfer_out} slsrc")
            verify_listing(exchange)
            descriptor = guest("lookup slprog", ["slprog"])
            assert re.search(r"(?m)^slprog\s+=set 1 disc .* 2\.4 12\s+; temp\r?$", descriptor), descriptor
            if execute_slang:
                os.write(session.master, b"slprog\r")
                record(session.until(b"slprog\r\n"))
                os.write(session.master, b"\x05")
                record(session.until(b"\nsim4000> "))
                record(session.command(b"step 1000000\n"))
                cpu = record(session.command(b"show cpu\n"))
                address = loop_state(cpu, initial_claim[0][3], compatibility)
                decoded = record(session.command(f"examine /instruction 0x{address - 6:x} 4\n".encode("ascii")))
                verify_loop_code(decoded, address)
                if not compatibility:
                    keys = record(session.command(f"show keys 0x{address:x}\n".encode("ascii")))
                    assert f"{address:06x}: key=1 " in keys, keys
                record(session.command(b"step 257\n"))
                assert loop_state(record(session.command(b"show cpu\n")), initial_claim[0][3], compatibility) == address
                os.write(session.master, b"continue\n\x1b")
                record(session.until(b"att") + session.until(b"\x07"))
                guest("s")
                # Break is a guest system command, not a debugger breakpoint.
                os.write(session.master, b"proc cmdcheck break\r")
                broken = record(session.until(b"pause cmdcheck break fp", timeout=30))
                assert re.findall(r"(?m)^\*\*\*[^\r\n]*", broken) == [f"***break 8 {address}"], broken
                assert "syntax error:" not in broken, broken
                os.write(session.master, b"\x1b")
                record(session.until(b"att") + session.until(b"\x07"))
                guest("s")
                os.write(session.master, b"proc cmdcheck start\r")
                recovered = record(session.until(b"\x07", timeout=30))
                assert re.findall(r"(?m)^\*\*\*[^\r\n]*", recovered) == ["***fp reinitialized"], recovered
                assert "syntax error:" not in recovered and " break fp" not in recovered, recovered
                guest("lookup slprog", ["slprog"])

            nested = guest("i outercmd")
            assert events(nested) == [], nested  # Descriptors go to nestlog, not console.
            # The unchanged outer file also exports the redirected report.
            report = (exchange / "nestedout").read_text(encoding="ascii")
            assert events(report) == ["fromhost", "tohost", "algol"] and "***" not in report, report
            guest("lookup slang", ["slang"])  # Keyboard input and console output restored.

            for command, expected in (
                    ("i errcmd", ["fromhost", "***fp name nosuchprog", "tohost"]),
                    ("i errtop", ["slang", "fromhost", "***fp name nosuchprog", "tohost", "algol"]),
                    ("i condcmd", ["slang", "algol", "slang", "***lookup absentone unknown",
                                   "fromhost", "***lookup absentone unknown", "tohost",
                                   "slang", "slang", "algol", "fromhost", "tohost"])):
                guest(command, expected)
                guest("lookup slang", ["slang"])

            status = guest("i checkcmd", ["***slang sorry 1 1/4/1", "fromhost",
                                          "checkbad", "checkgood", "slang"])
            assert "a1 undeclared" in status and "slang ok 1/12/1" in status, status
            for name, entry in (("checkbad", 4), ("checkgood", 12)):
                assert re.search(rf"(?m)^{name}\s+=set 1 disc .* 2\.4 {entry}\s+; temp\r?$", status), status
            verify_binary(exchange)
            for name in ("checkbad", "checkgood"):
                guest(f"lookup {name}", [f"***lookup {name} unknown"])
            names = [name for _, name in INPUTS.values()] + ["nestlog", "slsrc", "slprog", "slist"]
            guest("clear temp " + " ".join(names))
            for name in names:
                guest(f"lookup {name}", [f"***lookup {name} unknown"])
            guest("lookup slang", ["slang"])
            assert claim_report(guest("claim")) == initial_claim
            assert free_space(guest("show")) == initial_space
            os.write(session.master, b"\x05")
            record(session.until(b"\nsim4000> "))
            cpu = record(session.command(b"show cpu\n"))
            assert f"protection={'off' if compatibility else 'rc4000'} " in cpu and "ix=on" in cpu, cpu
            media = record(session.command(b"show media\n")).replace("\r", "").replace("\n", "")
            assert "mode=volatile-overlay" in media, media
            if not compatibility:
                first = initial_claim[0][3]
                for address in (first, first + 50000, first + 99998):
                    keys = record(session.command(f"show keys {address}\n".encode("ascii")))
                    assert f"{address:06x}: key=1 " in keys, keys
            session.finish()
            verify_listing(exchange)
            assert (exchange / "slbinary").read_bytes()[:len(CODE)] == (exchange / "checkbin").read_bytes()[:len(CODE)]
            assert {p: digest(p) for p in sources} == before
            assert all((exchange / name).read_bytes() == data for name, data in inputs.items())
            (directory / "result.txt").write_text(
                f"FP command-file profile={profile}: nested EOF/output, missing-program continuation,\n"
                "conditional lookup and guarded SLANG export passed; claims/disk restored.\n"
                "SLANG list.yes command-file listing/binary exports and console restoration passed.\n"
                + ("SLANG execution: W0=444 W1=321, stable self-loop, guest break/FP recovery passed.\n" if execute_slang else "")
                + f"monitor-sha256={digest(monitor)}\ndisk-sha256={digest(disk)}\n", encoding="ascii")
        finally:
            if session is not None:
                if session.pending:
                    record(session.pending.decode("ascii", errors="backslashreplace"))
                session.close()
            assert {p: digest(p) for p in sources} == before, "source media or fixtures changed"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("simulator", type=Path)
    parser.add_argument("destination", type=Path, help="new exclusive workspace")
    parser.add_argument("--compatibility", action="store_true")
    parser.add_argument("--execute-slang", action="store_true",
                        help="also execute the known integer loop and check guest break/FP recovery")
    args = parser.parse_args()
    run(args.simulator.resolve(), args.destination.resolve(), args.compatibility, args.execute_slang)
    print(f"{'Compatibility' if args.compatibility else 'Protected'} FP command-file checkpoint passed")


if __name__ == "__main__":
    main()
