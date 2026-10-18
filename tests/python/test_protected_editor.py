#!/usr/bin/env python3
"""Opt-in: verify command-driven editor workflows on isolated runtime media."""

import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess

from test_operator_cli import Session
from test_protected_catalog import claim_report, free_space

ROOT = Path(__file__).resolve().parents[2]
SAMPLE = b"Monitor manual sample.\nSecond line.\n"
LINES = b"First line.\nKeep this line.\nRemove this line.\nLast line.\n"
PATCHED = (b"First line.\nKeep this line.\nInserted line.\n"
           b"Literal comma, slash / preserved.\nLast line.\n")
EXPORTS = {
    "edited": SAMPLE.replace(b"Monitor", b"Edited", 1),
    "original": SAMPLE,
    "operations": PATCHED,
    "linesorig": LINES,
    "reopened": PATCHED.replace(b"Inserted", b"Reopened", 1),
    "savedagain": PATCHED,
    "cut": b"First line.\nKeep this line.\n",
    "cmporig": SAMPLE,
    "cmpcopy": SAMPLE,
    "cmpedit": SAMPLE.replace(b"Monitor", b"Edited", 1),
    "joined": SAMPLE * 2,
}


def comparison_report(output):
    text = output.replace("\r", "")
    assert text.count("DISCREPANCY NO.") == 1, text
    assert text.count("discrepancy found") == 1, text
    match = re.search(r"DISCREPANCY NO\. 1[^\n]*\n.*?1 discrepancy found", text, re.S)
    assert match, text
    lines = [line.strip() for line in match[0].splitlines() if line.strip()]
    assert lines == ["DISCREPANCY NO. 1  msrc line 1, medit line 1  TEXT REPLACED",
                     "=========>> msrc:", "Monitor manual sample.",
                     "=========>> medit:", "Edited manual sample.",
                     "1 discrepancy found"], lines
    return lines


def verify_cmp_break(output):
    text = output.replace("\r", "")
    assert re.findall(r"(?m)^break\s+(\d+)\s+(\w+)$", text) == [("0", "tofrom1")], text
    assert re.findall(r"called from\s+line (\d+)-(\d+)", text) == [("219", "220"), ("380", "381")], text
    assert text.count("pause edcheck break fp") == 1, text
    assert not any(marker in text for marker in
                   ("***", "syntax error:", "files are identical", "DISCREPANCY")), text


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_exports(exchange):
    for name, expected in EXPORTS.items():
        assert (exchange / name).read_bytes() == expected, name
    assert (exchange / "input").read_bytes() == SAMPLE
    assert (exchange / "edinput").read_bytes() == LINES


def verify_host_comparison(exchange):
    """Use POSIX host tools; status 1 means different, not an I/O error."""
    results = []
    for tool in ("cmp", "diff"):
        for target, expected in (("cmpcopy", 0), ("cmpedit", 1), ("absent", 2)):
            assert target != "absent" or not (exchange / target).exists()
            args = [tool, "-s" if tool == "cmp" else "-u",
                    str(exchange / "cmporig"), str(exchange / target)]
            result = subprocess.run(args, capture_output=True, text=True, timeout=10,
                                    env={**os.environ, "LC_ALL": "C"})
            if expected == 2:
                assert result.returncode > 1, (args, result)
            else:
                assert result.returncode == expected, (args, result)
                assert not result.stderr, (args, result.stderr)
                if tool == "diff" and expected == 1:
                    assert result.stdout.splitlines()[2:] == [
                        "@@ -1,2 +1,2 @@", "-Monitor manual sample.",
                        "+Edited manual sample.", " Second line."], result.stdout
                else:
                    assert not result.stdout, (args, result.stdout)
            results.append(f"{tool} cmporig {target}: status={result.returncode}\n"
                           + result.stdout + result.stderr)
    return "".join(results)


def run(simulator, directory, compatibility=False, check_cmp_break=False):
    if check_cmp_break and compatibility:
        raise ValueError("the known cmp break check requires the protected profile")
    profile = "compatibility" if compatibility else "protected"
    monitor = ROOT / ("src/monitor/MONITOR" if compatibility else "src/monitor/MONITOR.protected")
    disk = ROOT / ("src/disks/UNIT0" if compatibility else "src/disks/UNIT0.protected")
    media = {ROOT / "src/monitor/MONITOR", ROOT / "src/disks/UNIT0",
             ROOT / "src/disks/UNIT0.original", monitor, disk}
    before = {path: digest(path) for path in media}
    assert (ROOT / "tests/fixtures/editor_lines.txt").read_bytes() == LINES
    directory.mkdir()  # Refuse earlier results or personal working directories.
    exchange = directory / "exchange"
    exchange.mkdir()
    (exchange / "input").write_bytes(SAMPLE)
    (exchange / "edinput").write_bytes(LINES)
    (directory / "RC4000.cfg").write_text(
        f"PROTECTION MODE={'OFF' if compatibility else 'RC4000'}\n"
        f"MONITOR FILE={monitor}\nCORESIZE=0,1048576\nTHROTTLE VALUE=OFF\n"
        "DEFINE CONSOLE DEVNO=2 INTNO=6,3\nDEFINE CLOCK DEVNO=3 INTNO=11\n"
        f"DEFINE RC433X DEVNO=15 FILE={disk}\n"
        f"DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT={exchange}\n", encoding="ascii")
    transfer_in, transfer_out = ("fromhost", "tohost") if compatibility else ("rcin", "rcout")
    session = None
    with (directory / "session.log").open("x", encoding="ascii") as log:
        def record(text):
            log.write(text)
            log.flush()
            return text

        def guest(line, expected=None, diagnostic=None):
            os.write(session.master, line.encode("ascii") + b"\r")
            output = record(session.until(b"\x07"))
            assert " break fp" not in output and "syntax error:" not in output, output
            if diagnostic is None:
                assert "***" not in output, output
            else:
                errors = re.findall(r"(?m)^\*\*\*[^\r\n]*", output)
                assert errors == [diagnostic] and output.count("***") == 1, output
            if expected is not None:
                assert expected in output, output
            return output

        def inspect(first):
            os.write(session.master, b"\x05")
            record(session.until(b"\nsim4000> "))
            cpu = record(session.command(b"show cpu\n"))
            assert f"protection={'off' if compatibility else 'rc4000'} " in cpu and "ix=on" in cpu, cpu
            assert "mode=volatile-overlay" in record(session.command(b"show media\n"))
            if not compatibility:
                for address in (first, first + 50000, first + 99998):
                    keys = record(session.command(f"show keys {address}\n".encode("ascii")))
                    assert f"{address:06x}: key=1 " in keys, keys

        try:
            session = Session(simulator, directory, ("-a",), startup=b"initialize date:")
            record("initialize date:\n")
            os.write(session.master, b"86 09 30 08 00 00\r")
            record(session.until(b"pause startproc finis fp"))
            os.write(session.master, b"\x1b")
            record(session.until(b"att") + session.until(b"\x07"))
            guest("s")
            guest("new edcheck size 100000 run", "to edcheck")
            claim = guest("claim", "first core")
            match = re.search(r"size\s+100000\s+first core\s+(\d+)", claim)
            assert match, claim
            first = int(match[1])
            assert first >= 8 and first % 2 == 0 and first + 100000 <= 1048576, claim
            initial_claim = claim_report(claim)
            initial_space = free_space(guest("show"))
            inspect(first)
            os.write(session.master, b"continue\n")

            guest(f"msrc={transfer_in} input", "bytes")
            source = guest("lookup msrc", "; temp")
            assert re.search(r"(?m)^msrc\s+=set 36 disc ", source), source
            guest("medit=edit msrc", "edit begin.")
            guest("l./Monitor/,r/Monitor/Edited/,f", "edit end.")
            guest(f"edited={transfer_out} medit", "bytes")
            guest(f"original={transfer_out} msrc", "bytes")

            display = guest("c=copy msrc")
            assert display.replace("\r", "").count(SAMPLE.decode("ascii")) == 1, display
            guest("mcopy=copy msrc")
            guest("rename mcopy.moved")
            moved = guest("lookup moved", "; temp")
            assert re.search(r"(?m)^moved\s+=set 1 disc ", moved), moved
            guest("lookup mcopy", "***lookup mcopy unknown", diagnostic="***lookup mcopy unknown")
            if compatibility:
                identical = guest("cmp msrc moved", "files are identical")
                assert "DISCREPANCY" not in identical, identical
                difference = comparison_report(guest("cmp msrc medit"))
                guest("o cmpreport")
                redirected = guest("cmp msrc medit")
                assert "DISCREPANCY" not in redirected, redirected
                guest("o c")
                guest(f"cmpout={transfer_out} cmpreport", "bytes")
                report = (exchange / "cmpout").read_bytes()
                assert b"\r" not in report and b"\0" not in report and b"\x19" not in report
                assert comparison_report(report.decode("ascii")) == difference
                guest("clear temp cmpreport")
                guest("lookup cmpreport", diagnostic="***lookup cmpreport unknown")
            elif check_cmp_break:
                # Explicit negative coverage, never a successful comparison.
                os.write(session.master, b"cmp msrc moved\r")
                failure = record(session.until(b"pause edcheck break fp", timeout=30))
                verify_cmp_break(failure)
                os.write(session.master, b"\x1b")
                record(session.until(b"att") + session.until(b"\x07"))
                guest("s")
                guest("proc edcheck start", "***fp reinitialized", diagnostic="***fp reinitialized")
                guest("lookup msrc moved medit", "; temp")
            for export, name in (("cmporig", "msrc"), ("cmpcopy", "moved"), ("cmpedit", "medit")):
                guest(f"{export}={transfer_out} {name}", "bytes")
            preserved = (exchange / "cmporig").read_bytes()
            stat_before = (exchange / "cmporig").stat()
            guest(f"cmporig={transfer_out} medit",
                  diagnostic="*** cannot open or create host file, result -8388608")
            assert (exchange / "cmporig").read_bytes() == preserved == SAMPLE
            stat_after = (exchange / "cmporig").stat()
            for field in ("st_ino", "st_size", "st_mode", "st_mtime_ns"):
                assert getattr(stat_after, field) == getattr(stat_before, field), field
            guest("lookup msrc moved medit", "; temp")
            concatenated = guest("c=copy msrc moved").replace("\r", "")
            assert concatenated.count(SAMPLE.decode("ascii") * 2) == 1, concatenated
            guest("mjoin=copy msrc moved")
            guest(f"joined={transfer_out} mjoin", "bytes")
            guest("clear temp moved mjoin")
            for name in ("moved", "mjoin"):
                guest(f"lookup {name}", diagnostic=f"***lookup {name} unknown")

            guest(f"mlines={transfer_in} edinput", "bytes")
            guest("mpatch=edit mlines", "edit begin.")
            for line in ("l1,l1,d0,i!", "Inserted line.",
                         "Literal comma, slash / preserved.", "!", "f"):
                guest(line, "edit end." if line == "f" else None)
            guest(f"operations={transfer_out} mpatch", "bytes")
            guest(f"linesorig={transfer_out} mlines", "bytes")

            guest("mreopen=edit mpatch", "edit begin.")
            guest("l./Inserted/,r/Inserted/Reopened/,f", "edit end.")
            guest(f"reopened={transfer_out} mreopen", "bytes")
            guest(f"savedagain={transfer_out} mpatch", "bytes")
            guest("mcut=edit mlines", "edit begin.")
            guest("l2,d1,f", "edit end.")
            guest(f"cut={transfer_out} mcut", "bytes")
            guest("clear temp msrc medit mlines mpatch mreopen mcut")
            assert claim_report(guest("claim")) == initial_claim
            assert free_space(guest("show")) == initial_space
            inspect(first)
            session.finish()
            verify_exports(exchange)
            host_report = verify_host_comparison(exchange)
            verify_exports(exchange)
            (directory / "host-comparison.log").write_text(host_report, encoding="ascii")
            assert {path: digest(path) for path in media} == before, "base media changed"
            (directory / "result.txt").write_text(
                f"Editor profile={profile}: replacement, insertion/deletion, reopening and truncation passed.\n"
                "Eleven content exports match; display/copy/rename/concatenation passed; claims/disk restored.\n"
                "Host cmp/diff distinguish equal/different/error; overwrite refused without changing the original.\n"
                + ("Compatibility cmp identical/replacement and exported report passed.\n" if compatibility else
                   "Protected cmp remains unsupported: known tofrom1 break, not a passing comparison.\n")
                + ("Explicit cmp-break/FP-reinitialization recovery checked; inputs survived.\n" if check_cmp_break else "")
                + f"monitor-sha256={digest(monitor)}\ndisk-sha256={digest(disk)}\n",
                encoding="ascii")
        finally:
            if session is not None:
                if session.pending:
                    record(session.pending.decode("ascii", errors="backslashreplace"))
                session.close()
            assert {path: digest(path) for path in media} == before, "base media changed"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("simulator", type=Path)
    parser.add_argument("destination", type=Path, help="new private directory; must not exist")
    parser.add_argument("--compatibility", action="store_true",
                        help="repeat the same workflow with the unchanged compatibility profile")
    parser.add_argument("--check-cmp-break", action="store_true",
                        help="explicitly check the known protected cmp break and bounded FP recovery")
    args = parser.parse_args()
    run(args.simulator.resolve(), args.destination.resolve(), args.compatibility, args.check_cmp_break)
    print(f"{'Compatibility' if args.compatibility else 'Protected'} editor checkpoint passed")


if __name__ == "__main__":
    main()
