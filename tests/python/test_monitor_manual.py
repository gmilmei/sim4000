#!/usr/bin/env python3
"""Verify the user manual's monitor examples without changing base images."""

import hashlib
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main() -> None:
    builder = Path(sys.argv[1]).resolve()
    standard = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
    root = Path(__file__).resolve().parents[2]
    image = root / "src/disks/UNIT0"
    before = hashlib.sha256(image.read_bytes()).digest()
    catalog = subprocess.check_output(
        [str(builder.with_name("sim4000")), "--list-catalog", str(image)], text=True)
    installed = {line.split()[2] for line in catalog.splitlines()[2:] if line.split()}
    assert len(installed) == 415, catalog
    variants = [("--verify-manual", root / "src/monitor/MONITOR")]
    if standard is not None:
        variants.append(("--verify-manual-dfc", standard))
    for mode, monitor in variants:
        with tempfile.TemporaryDirectory(prefix="sim4000-manual-") as temporary:
            directory = Path(temporary)
            text = b"Monitor manual sample.\nSecond line.\n"
            (directory / "input").write_bytes(text)
            editor_input = (root / "tests/fixtures/editor_lines.txt").read_bytes()
            (directory / "edinput").write_bytes(editor_input)
            (directory / "alginput").write_bytes(
                (root / "tests/fixtures/hello.alg").read_bytes())
            (directory / "slinput").write_bytes(
                (root / "tests/fixtures/slang_integer.sl").read_bytes())
            (directory / "cmdinput").write_bytes(
                (root / "tests/fixtures/slang_commands.fp").read_bytes())
            (directory / "badinput").write_bytes(
                (root / "tests/fixtures/slang_error.sl").read_bytes())
            (directory / "outerinput").write_bytes(
                (root / "tests/fixtures/nested_outer.fp").read_bytes())
            (directory / "innerinput").write_bytes(
                (root / "tests/fixtures/nested_inner.fp").read_bytes())
            (directory / "errinput").write_bytes(
                (root / "tests/fixtures/command_error.fp").read_bytes())
            (directory / "errtopin").write_bytes(
                (root / "tests/fixtures/command_error_outer.fp").read_bytes())
            (directory / "asmerrin").write_bytes(
                (root / "tests/fixtures/assembly_error.fp").read_bytes())
            (directory / "condinput").write_bytes(
                (root / "tests/fixtures/conditional_commands.fp").read_bytes())
            (directory / "statusin").write_bytes(
                (root / "tests/fixtures/assembly_status.fp").read_bytes())
            # Instrumented guest compilation takes several minutes; keep a
            # finite wall limit and the unchanged per-command instruction bound.
            result = subprocess.run([str(builder), mode, str(monitor), str(image),
                            str(directory)], timeout=300,
                           stdout=subprocess.PIPE, text=True)
            print(result.stdout, end="")
            result.check_returncode()
            assert "Hello from Algol" in result.stdout
            assert "algol end 24" in result.stdout
            temporary_catalog, system_catalog = result.stdout.split(
                "\x07catsort\n", 1)[1].split("\x07catsort scope.system\n", 1)
            system_catalog = system_catalog.split("\x07claim\n", 1)[0]

            def sorted_names(report: str) -> list[str]:
                return re.findall(r"(?m)^(\w+)\s+(?:[a-z]+|[+-]?\d+(?:\.\d+)?)\s+\w+\s", report)

            assert sorted_names(temporary_catalog) == ["c", "v"], temporary_catalog
            assert "base: 8388605 8388605 temp" in temporary_catalog, temporary_catalog
            assert re.search(r"total:\s+0 segm\.\s+2 entr\.", temporary_catalog), temporary_catalog
            names = sorted_names(system_catalog)
            assert len(names) == len(set(names)) == 414, system_catalog
            assert set(names) == installed - {"catalog"}, (set(names) ^ installed)
            assert names == sorted(names), names
            pages = 8 if mode == "--verify-manual" else 7
            assert re.findall(r"catsort  page (\d+), name of catalog: catalog", system_catalog) == [
                str(page) for page in range(1, pages + 1)], system_catalog
            assert system_catalog.count("\f") == pages, system_catalog
            assert "base:-8388607 8388605 perm" in system_catalog, system_catalog
            assert re.search(r"total:\s+2818 segm\.\s+414 entr\.", system_catalog), system_catalog
            assert re.search(r"disc\s+: 967 slices \*\s+3 =\s+2901 segments\s+414 entries",
                             system_catalog), system_catalog
            assert "***" not in temporary_catalog + system_catalog
            exported_bytes = (directory / "sortout").read_bytes()
            assert not any(character in exported_bytes for character in (b"\r", b"\x00", b"\x19"))
            exported_catalog = exported_bytes.decode("ascii")
            assert sorted_names(exported_catalog) == names, exported_catalog
            rows = r"(?m)^\w+[ \t]+(?:[a-z]+|[+-]?\d+(?:\.\d+)?)[ \t]+\w+[ \t]+[^\n]*"
            exported_rows = re.findall(rows, exported_catalog)
            console_rows = re.findall(rows, system_catalog)
            assert exported_rows == console_rows, (
                len(exported_rows), len(console_rows),
                [(exported, console) for exported, console in zip(exported_rows, console_rows)
                 if exported != console][:5])
            assert re.search(r"total:\s+2818 segm\.\s+414 entr\.", exported_catalog), exported_catalog
            assert re.search(r"disc\s+: 967 slices \*\s+3 =\s+2901 segments\s+414 entries",
                             exported_catalog), exported_catalog
            assert "***" not in exported_catalog, exported_catalog
            assert (directory / "sortagain").read_bytes() == exported_bytes
            recovery = result.stdout.split("\x07catstat sortreport\n", 1)[1].split(
                "\x07sortagain=tohost sortreport\n", 1)[0]
            assert len(re.findall(r"break\s+0\s+line\s+18-19", recovery)) == 1, recovery
            assert recovery.count("pause muser break fp") == 1, recovery
            assert re.findall(r"(?m)^\*\*\*[^\n]*", recovery) == [
                "***fp reinitialized"], recovery
            assert recovery.index("pause muser break fp") < recovery.index(
                "proc muser start") < recovery.index("***fp reinitialized"), recovery
            assert "att \x07s" in recovery, recovery
            redirected, resumed = result.stdout.split("\x07o sortreport\n", 1)[1].split(
                "\x07o c\n", 1)
            assert "catsort scope.system" in redirected, redirected
            assert not sorted_names(redirected), redirected
            assert "catsort  page" not in redirected, redirected
            assert re.search(r"(?m)^sortreport\s+=set \d+ disc .*; temp$", resumed), resumed
            sort_cleanup = resumed.split("clear temp sortreport\n", 1)[1].split(
                "msrc=fromhost input\n", 1)[0]
            assert "***lookup sortreport unknown" in sort_cleanup, sort_cleanup
            space_session = result.stdout.split("lookup fromhost tohost slang algol\n", 1)[1].split(
                "msrc=fromhost input\n", 1)[0]
            space = [tuple(map(int, match)) for match in re.findall(
                r"free disc space:\s+disc\s*:\s*(\d+) slices,\s*(\d+) segm\.",
                space_session)]
            assert len(space) == 4, space_session
            before_space, repeated, allocated_space, reclaimed = space
            assert before_space == repeated == reclaimed, space
            assert all(slices > 0 and segments == slices * 3
                       for slices, segments in space), space
            allocation = re.search(r"(?m)^spacetmp\s+=set (\d+) disc .*; temp$",
                                   space_session)
            assert allocation is not None, space_session
            allocated_segments = int(allocation[1])
            assert allocated_segments == 36, allocation[0]
            assert before_space[0] - allocated_space[0] == allocated_segments // 3, space
            assert before_space[1] - allocated_space[1] == allocated_segments, space
            assert "***lookup spacetmp unknown" in space_session.split(
                "clear temp spacetmp\n", 1)[1], space_session
            claim_reports = space_session.split("\x07claim\n")[1:]
            assert len(claim_reports) == 4, space_session
            claim_headers = []
            claim_rows = []
            for report in claim_reports:
                report = report.split("\x07", 1)[0]
                header = re.search(r"area (\d+)\s+buf (\d+)\s+size (\d+)\s+first core (\d+)", report)
                assert header is not None, report
                claim_headers.append(tuple(map(int, header.groups())))
                assert "disc: 3 segm/slice" in report, report
                rows = re.findall(r"(?m)^\s*(temp|login|perm)\s+(\d+) segm\s+(\d+) slices\s+(\d+) entr", report)
                assert [row[0] for row in rows] == ["temp", "login", "perm"], report
                parsed = {row[0]: tuple(map(int, row[1:])) for row in rows}
                assert all(segments == slices * 3 and entries > 0
                           for segments, slices, entries in parsed.values()), parsed
                claim_rows.append(parsed)
            assert all(header == claim_headers[0] for header in claim_headers), claim_headers
            assert claim_headers[0][2] == 100000, claim_headers
            assert claim_rows[0] == claim_rows[1] == claim_rows[3], claim_rows
            for scope in ("login", "perm"):
                assert claim_rows[0][scope] == claim_rows[2][scope], claim_rows
            before_temp = claim_rows[0]["temp"]
            during_temp = claim_rows[2]["temp"]
            assert tuple(before - during for before, during in zip(before_temp, during_temp)) == (
                allocated_segments, allocated_segments // 3, 1), claim_rows
            listings = result.stdout.split("list\n")[1:]
            assert len(listings) == 4, result.stdout
            assert re.search(
                r"(?m)^muser\s+\d+\s+100000\s+\d+\s+\d+\s+stop\s+s\s*$",
                listings[1]), listings[1]
            assert re.search(rf"(?m)^muser\s+{claim_headers[0][3]}\s+100000\s", listings[1]), listings[1]
            assert not re.search(r"(?m)^muser\s+\d", listings[2])
            assert not re.search(r"(?m)^mremove\s+\d", listings[3])
            assert (directory / "output").read_bytes() == text
            display = result.stdout.split("c=copy msrc\n", 1)[1].split(
                "mcopy=copy msrc\n", 1)[0]
            concatenated = result.stdout.split("c=copy msrc moved\n", 1)[1].split(
                "output=tohost moved\n", 1)[0]
            sample = text.decode("ascii")
            assert display.startswith(sample), display
            assert display.count(sample) == 1, display
            assert concatenated.startswith(sample * 2), concatenated
            assert concatenated.count(sample) == 2, concatenated
            assert re.search(r"(?m)^msrc\s+segm\.", display), display
            for name in ("msrc", "moved", "c"):
                assert re.search(rf"(?m)^{name}\s+segm\.", concatenated), concatenated
            assert (directory / "edited").read_bytes() == text.replace(
                b"Monitor", b"Edited", 1)
            identical = result.stdout.split("cmp msrc moved\n", 1)[1].split(
                "cmp msrc medit\n", 1)[0]
            assert "files are identical" in identical, identical
            difference = result.stdout.split("cmp msrc medit\n", 1)[1].split(
                "o cmpreport\n", 1)[0]
            comparison = (directory / "cmpout").read_text(encoding="ascii")
            for report in (difference, comparison):
                assert "DISCREPANCY NO. 1  msrc line 1, medit line 1  TEXT REPLACED" in report, report
                assert "Monitor manual sample." in report, report
                assert "Edited manual sample." in report, report
                assert "1 discrepancy found" in report, report
                assert "DISCREPANCY NO. 2" not in report, report
            # Comparing and redirecting the report must leave all inputs intact.
            assert (directory / "cmporig").read_bytes() == text
            assert (directory / "cmpcopy").read_bytes() == text
            assert (directory / "cmpedit").read_bytes() == text.replace(
                b"Monitor", b"Edited", 1)
            assert (directory / "edorig").read_bytes() == editor_input
            operations = (directory / "edops").read_bytes()
            assert operations == (
                b"First line.\nKeep this line.\nInserted line.\n"
                b"Literal comma, slash / preserved.\nLast line.\n"), operations
            assert (directory / "edcut").read_bytes() == (
                b"First line.\nKeep this line.\n")
            listing = (directory / "sllisting").read_text(encoding="ascii")
            assert "al w0 123" in listing, listing
            assert "al w1 321" in listing, listing
            assert "wa w0 2" in listing, listing
            assert "slang ok 1/12/1" in listing, listing
            assert "***" not in listing, listing
            # Source output stays in the guest listing; console output resumes
            # for the exports and the terminal command after command-file EOF.
            assert "al w0 123" not in result.stdout, result.stdout
            assert re.search(r"(?m)^slprog\s+=set 1 disc .* 2\.4 12\s+; temp$",
                             result.stdout), result.stdout
            print("Saved SLANG listing:\n" + listing)
            binary = (directory / "slbinary").read_bytes()
            # Explicit instruction words also used by the core assembly
            # checkpoint, not generated by the host instruction encoder.
            words = [0, 0, 0x2c007b, 0x2d0141, 0x1c0002, 0x348000]
            expected_code = b"".join(word.to_bytes(3, "big") for word in words)
            assert len(binary) == 768, len(binary)
            assert binary[:18] == expected_code, binary[:18].hex()
            nested = (directory / "nestedout").read_text(encoding="ascii")
            assert re.findall(r"(?m)^(\w+)\s+=set .*; system$", nested) == [
                "fromhost", "tohost", "algol"], nested
            assert "***" not in nested, nested
            nested_console = result.stdout.split("i outercmd\n", 1)[1].split(
                "errcmd=fromhost errinput\n", 1)[0]
            for name in ("fromhost", "tohost", "algol"):
                assert not re.search(rf"(?m)^{name}\s+=set", nested_console), nested_console
            assert re.search(r"(?m)^slang\s+=set .*; system$", nested_console), nested_console
            cleanup = nested_console.split("clear temp outercmd innercmd nestlog\n", 1)[1]
            for name in ("outercmd", "innercmd", "nestlog"):
                assert f"***lookup {name} unknown" in cleanup, cleanup
            for selector, names in (("i errcmd", ["fromhost", "tohost"]),
                                    ("i errtop", ["slang", "fromhost", "tohost", "algol"])):
                error_session, keyboard = result.stdout.split(selector + "\n", 1)[1].split(
                    "lookup slang\n", 1)
                assert re.findall(r"(?m)^(\w+)\s+=set .*; system$", error_session) == names, error_session
                assert re.findall(r"(?m)^\*\*\*[^\n]*", error_session) == [
                    "***fp name nosuchprog"], error_session
                before_error = re.search(r"(?m)^fromhost\s+=set", error_session)
                after_error = re.search(r"(?m)^tohost\s+=set", error_session)
                assert before_error is not None and after_error is not None, error_session
                assert before_error.start() < error_session.index(
                    "***fp name nosuchprog") < after_error.start(), error_session
                assert re.match(r"slang\s+=set .*; system\n", keyboard), keyboard
            error_cleanup = result.stdout.split("clear temp errcmd errtop\n", 1)[1].split(
                "\x07nosuchprog\n", 1)[0]
            for name in ("errcmd", "errtop"):
                assert f"***lookup {name} unknown" in error_cleanup, error_cleanup
            batch_error = result.stdout.split("i batchcmd\n", 1)[1].split(
                "lookup batchbad\n", 1)[0]
            assert "a1 undeclared" in batch_error, batch_error
            assert re.findall(r"(?m)^\*\*\*[^\n]*", batch_error) == [
                "***slang sorry 1 1/4/1"], batch_error
            partial = re.search(r"(?m)^batchbad\s+=set 1 disc .* 2\.4 4\s+; temp$",
                                batch_error)
            continued = re.search(r"(?m)^slang\s+=set .*; system$", batch_error)
            assert partial is not None and continued is not None, batch_error
            assert batch_error.index("***slang sorry") < partial.start() < continued.start(), batch_error
            after_batch = result.stdout.split("lookup batchbad\n", 1)[1].split(
                "\x07nosuchprog\n", 1)[0]
            assert "***lookup batchbad unknown" in after_batch, after_batch
            assert re.search(r"(?m)^slang\s+=set .*; system$", after_batch), after_batch
            for name in ("batchsrc", "batchcmd"):
                assert f"***lookup {name} unknown" in after_batch, after_batch
            conditional, keyboard = result.stdout.split("i condcmd\n", 1)[1].split(
                "lookup slang\n", 1)
            events = [name or error for name, error in re.findall(
                r"(?m)^(\w+)\s+=set .*; system$|^(\*\*\*[^\n]*)$", conditional)]
            assert events == [
                "slang", "algol", "slang", "***lookup absentone unknown",
                "fromhost", "***lookup absentone unknown", "tohost",
                "slang", "slang", "algol", "fromhost", "tohost"], conditional
            assert re.match(r"slang\s+=set .*; system\n", keyboard), keyboard
            conditional_cleanup = keyboard.split("clear temp condcmd\n", 1)[1].split(
                "\x07nosuchprog\n", 1)[0]
            assert "***lookup condcmd unknown" in conditional_cleanup, conditional_cleanup
            assembly_status = result.stdout.split("i checkcmd\n", 1)[1].split(
                "lookup checkbad checkgood\n", 1)[0]
            assert "a1 undeclared" in assembly_status, assembly_status
            assert re.findall(r"(?m)^\*\*\*[^\n]*", assembly_status) == [
                "***slang sorry 1 1/4/1"], assembly_status
            assert re.findall(r"(?m)^(\w+)\s+=set", assembly_status) == [
                "fromhost", "checkbad", "checkgood", "slang"], assembly_status
            assert re.search(r"(?m)^checkbad\s+=set 1 disc .* 2\.4 4\s+; temp$",
                             assembly_status), assembly_status
            assert re.search(r"(?m)^checkgood\s+=set 1 disc .* 2\.4 12\s+; temp$",
                             assembly_status), assembly_status
            assert assembly_status.index("***slang sorry") < assembly_status.index(
                "fromhost") < assembly_status.index("slang ok 1/12/1"), assembly_status
            checked_binary = (directory / "checkbin").read_bytes()
            assert not (directory / "badcheck").exists()
            assert len(checked_binary) == 768, len(checked_binary)
            assert checked_binary[:18] == expected_code, checked_binary[:18].hex()
            status_cleanup = result.stdout.split("lookup checkbad checkgood\n", 1)[1].split(
                "\x07nosuchprog\n", 1)[0]
            for name in ("checkbad", "checkgood", "checksrc", "checkvalid", "checkcmd"):
                assert f"***lookup {name} unknown" in status_cleanup, status_cleanup
            assert re.search(r"(?m)^slang\s+=set .*; system$", status_cleanup), status_cleanup
            missing = result.stdout.split("clear temp errcmd errtop\n", 1)[1].split(
                "\x07nosuchprog\n", 1)[1].split(
                "lookup slang\n", 1)[0]
            assert "***fp name nosuchprog" in missing, missing
            assert re.search(r"nosuchprog\s+unknown", missing), missing
            failure = result.stdout.split("badprog=slang badsrc\n", 1)[1].split(
                "fixsrc=fromhost slinput\n", 1)[0]
            assert "a1 undeclared" in failure, failure
            assert "***slang sorry 1 1/4/1" in failure, failure
            assert re.search(r"(?m)^badprog\s+=set 1 disc .* 2\.4 4\s+; temp$",
                             failure), failure
            assert "***lookup badprog unknown" in failure.split(
                "clear temp badprog\n", 1)[1], failure
            recovered = (directory / "recovered").read_bytes()
            assert len(recovered) == 768, len(recovered)
            assert recovered[:18] == expected_code, recovered[:18].hex()
            assert (directory / "sosstart").read_bytes() == (
                b"* new sos size 300000 function 1 2 3 4 5 buf 35 area 45\n"
                b"* perm disc 2500 200 base -8388607 8388605 prog bsos internal 1 \n"
                b"* key 3  run\n* unstack\n")
        assert hashlib.sha256(image.read_bytes()).digest() == before
    print("Monitor manual process, free disk space/allocation/reclamation, file display/concatenation, editor replacement/insertion/deletion, text comparison/report export, nested FP command input/output and missing-program/assembly-error continuation, SLANG listing/binary/error recovery, catalog, Algol, and cleanup examples passed")


if __name__ == "__main__":
    main()
