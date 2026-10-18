#!/usr/bin/env python3
"""Test monitor rebuild preparation and protected runtime probe guards."""

import hashlib
from collections import Counter
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from make_unit0_copy import CopyError
from rebuild_monitor import (BUILD_FILE, PROTECTED_BUILD_FILE, PROTECTION_BYPASS,
                             listing_succeeded, prepare_sources,
                             validate_protection_source, validate_protected_inputs,
                             verify_guest_protection_edit)
from make_host_image import BINARY, TEXT
from probe_protected_cmp import (verify_evidence as verify_cmp_evidence,
                                  probe as cmp_probe, TEXT as CMP_TEXT)
from probe_rc4000_runtime import (verify_algol_transfers, verify_isolation_transfers,
                                  verify_lifecycle_transfers, verify_base_diagnostics,
                                  verify_nested_checkpoint)
from test_protected_telnet import (LOCAL_TEXT, REMOTE_TEXT, claim as telnet_claim,
                                   run as protected_telnet_run, verify_exports)
import prepare_monitor_profiles as profiles
from test_regnetest_cli import verify_iteration
from test_protected_editor import (EXPORTS as EDITOR_EXPORTS, SAMPLE as EDITOR_SAMPLE,
                                   LINES as EDITOR_LINES, verify_exports as verify_editor_exports,
                                   comparison_report, verify_cmp_break,
                                   verify_host_comparison, run as editor_run)
from test_profile_persistence import (descriptor as persistence_descriptor,
                                      verify_exports as verify_persistence_exports,
                                      run as persistence_run)
from test_protected_catalog import (packed_catalog, catstat_report, claim_report,
                                    free_space, sorted_rows, sorted_names,
                                    descriptor_names, run as catalog_run)
from test_protected_commands import (CODE as COMMAND_CODE, events as command_events,
                                     verify_binary as verify_command_binary,
                                     verify_listing as verify_command_listing,
                                     loop_state, verify_loop_code,
                                     run as commands_run)


class MonitorRebuildTests(unittest.TestCase):
    def test_slang_loop_requires_sum_task_state_and_exact_loaded_instructions(self) -> None:
        good = ("IC=00b532 W0=0001bc W1=000141 W2=023186 W3=023186\n"
                "IR=000000 IM=9fffff interrupts=on monitor=no\n"
                "reset=no stopped=yes exceptions=000 calls=6030000\n"
                "cpu=RC4000 ix=on\nprotection=rc4000 PR=bf\n")
        self.assertEqual(loop_state(good, 44840), 0xb532)
        for old, new in (("W0=0001bc", "W0=0001bd"), ("W1=000141", "W1=000140"),
                         ("monitor=no", "monitor=yes"), ("PR=bf", "PR=80"),
                         ("exceptions=000", "exceptions=001"), ("ix=on", "ix=off"),
                         ("stopped=yes", "stopped=no"), ("IC=00b532", "IC=00b533"),
                         ("IC=00b532", "IC=000010"), ("protection=rc4000", "protection=off")):
            with self.assertRaises(AssertionError):
                loop_state(good.replace(old, new), 44840)
        compatible = good.replace("protection=rc4000 PR=bf", "protection=off PR=80").replace("monitor=no", "monitor=yes")
        self.assertEqual(loop_state(compatible, 44840, compatibility=True), 0xb532)
        code = (" 00b52c: 2c007b  al w0 123\n 00b52e: 2d0141  al w1 321\n"
                " 00b530: 1c0002  wa w0 2\n>00b532: 348000  jl. w0 0\n")
        verify_loop_code(code, 0xb532)
        for old, new in (("348000", "348002"), ("00b530", "00b52e"), (">00b532:", ">00b534:")):
            with self.assertRaises(AssertionError):
                verify_loop_code(code.replace(old, new), 0xb532)

    def test_slang_listing_requires_order_status_binary_and_unchanged_source(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            listing = (b"    al w0 123\n    al w1 321\n    wa w0 2\n    jl. 0\n"
                       b"slang ok 1/12/1\n")
            good = {"sllisting": listing,
                    "slbinary": COMMAND_CODE + bytes(768 - len(COMMAND_CODE)),
                    "sloriginal": (ROOT / "tests/fixtures/slang_integer.sl").read_bytes()}
            for name, data in good.items():
                (directory / name).write_bytes(data)
            verify_command_listing(directory)
            for name, data in good.items():
                (directory / name).write_bytes(b"corrupt")
                with self.assertRaises(AssertionError):
                    verify_command_listing(directory)
                (directory / name).unlink()
                with self.assertRaises(FileNotFoundError):
                    verify_command_listing(directory)
                (directory / name).write_bytes(data)
            for old, new in ((b"w0 123", b"w0 124"), (b"wa w0 2", b"wa w0 4"),
                             (b"1/12/1", b"1/4/1"), (b"al w1 321\n", b""),
                             (b"slang ok", b"***slang sorry"), (b"\n", b"\r\n")):
                (directory / "sllisting").write_bytes(listing.replace(old, new))
                with self.assertRaises(AssertionError):
                    verify_command_listing(directory)
            (directory / "sllisting").write_bytes(listing + listing)
            with self.assertRaises(AssertionError):
                verify_command_listing(directory)

    def test_host_comparison_checks_statuses_report_and_missing_input(self) -> None:
        difference = ("--- original\n+++ edited\n@@ -1,2 +1,2 @@\n"
                      "-Monitor manual sample.\n+Edited manual sample.\n Second line.\n")
        good = [subprocess.CompletedProcess([], status, output, error)
                for status, output, error in ((0, "", ""), (1, "", ""), (2, "", "missing"),
                                              (0, "", ""), (1, difference, ""), (2, "", "missing"))]
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            with patch("test_protected_editor.subprocess.run", side_effect=good) as runner:
                report = verify_host_comparison(directory)
                self.assertEqual(runner.call_count, 6)
                self.assertIn("diff cmporig cmpedit: status=1", report)
            for index in range(6):
                bad = good.copy()
                bad[index] = subprocess.CompletedProcess([], 0 if index % 3 else 1, "", "")
                with patch("test_protected_editor.subprocess.run", side_effect=bad):
                    with self.assertRaises(AssertionError):
                        verify_host_comparison(directory)
            for index, output, error in ((0, "unexpected", ""), (1, "", "unexpected"),
                                         (4, difference.replace("Edited", "Wrong"), "")):
                bad = good.copy()
                bad[index] = subprocess.CompletedProcess([], good[index].returncode, output, error)
                with patch("test_protected_editor.subprocess.run", side_effect=bad):
                    with self.assertRaises(AssertionError):
                        verify_host_comparison(directory)
            (directory / "absent").write_bytes(b"not absent")
            with patch("test_protected_editor.subprocess.run", side_effect=good):
                with self.assertRaises(AssertionError):
                    verify_host_comparison(directory)

    def test_cmp_instruction_evidence_requires_fault_binary_and_surviving_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            transcript = (
                "Candidate verification: RC4000 protection enabled\n"
                "IRQ0 pc=00fa28 raw=730022 saved=00fa2a ea=000022 PR=bf key=1 monitor=0 W=0\n"
                "CMP probe: task PL address=34 IRQ0 saved-next=yes PR=bf key=1\n"
                "break 0 tofrom1\npause hostcheck break fp\n***fp reinitialized\n"
                "CMP known incompatibility localized; FP recovered, not a passing comparison\n")
            binary = bytearray(80 * 768)
            binary[768:777] = bytes.fromhex("730022 a3004f a3003b")
            binary[768 + 756:768 + 763] = b"tofrom1"
            binary[1536 + 756:1536 + 763] = b"tofrom2"
            good = {"session.log": transcript.encode("ascii"), "cmpbinary": bytes(binary),
                    "cmpmetadata": b"cmp =set 80 disc 0 ; system\n",
                    "input": CMP_TEXT, "origafter": CMP_TEXT, "copyafter": CMP_TEXT}
            for name, data in good.items():
                (directory / name).write_bytes(data)
            self.assertEqual(verify_cmp_evidence(directory), [768])
            for name, data in good.items():
                (directory / name).write_bytes(b"corrupt")
                with self.assertRaises(CopyError):
                    verify_cmp_evidence(directory)
                (directory / name).write_bytes(data)
            for old, new in (("saved=00fa2a", "saved=00fa28"), ("PR=bf", "PR=80"),
                             ("monitor=0", "monitor=1"), ("raw=730022", "raw=730024"),
                             ("***fp reinitialized", "***fp unexpected")):
                (directory / "session.log").write_text(transcript.replace(old, new), encoding="ascii")
                with self.assertRaises(CopyError):
                    verify_cmp_evidence(directory)
            (directory / "session.log").write_text(transcript + transcript, encoding="ascii")
            with self.assertRaises(CopyError):
                verify_cmp_evidence(directory)
            (directory / "session.log").write_text(transcript, encoding="ascii")
            for old, new in ((b"tofrom1", b"tofromx"), (b"tofrom2", b"tofromx"),
                             (bytes.fromhex("730022"), bytes.fromhex("730024"))):
                (directory / "cmpbinary").write_bytes(bytes(binary).replace(old, new))
                with self.assertRaises(CopyError):
                    verify_cmp_evidence(directory)

    def test_cmp_instruction_probe_preserves_existing_work(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            marker = directory / "keep"
            marker.write_bytes(b"preserve")
            with self.assertRaises(FileExistsError):
                cmp_probe(Path("unused"), directory)
            self.assertEqual(list(directory.iterdir()), [marker])
            self.assertEqual(marker.read_bytes(), b"preserve")

    def test_command_events_preserve_error_and_descriptor_order(self) -> None:
        output = ("i errcmd\r\nfromhost =set 47 disc 0 ; system\r\n"
                  "***fp name nosuchprog\r\ntohost =set 47 disc 0 ; system\r\n\x07")
        self.assertEqual(command_events(output),
                         ["fromhost", "***fp name nosuchprog", "tohost"])
        self.assertEqual(command_events("echo only\n"), [])
        self.assertEqual(command_events(output + "\n***fp reinitialized\n")[-1],
                         "***fp reinitialized")

    def test_command_export_requires_valid_code_and_no_failed_export(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            binary = directory / "checkbin"
            with self.assertRaises(FileNotFoundError):
                verify_command_binary(directory)
            good = COMMAND_CODE + bytes(768 - len(COMMAND_CODE))
            binary.write_bytes(good)
            verify_command_binary(directory)
            for bad in (good[:-1], good + b"x", bytes(768), good[:9] + b"x" + good[10:]):
                binary.write_bytes(bad)
                with self.assertRaises(AssertionError):
                    verify_command_binary(directory)
            binary.write_bytes(good)
            (directory / "badcheck").write_bytes(b"")
            with self.assertRaises(AssertionError):
                verify_command_binary(directory)

    def test_command_probe_preserves_existing_work(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            marker = directory / "keep"
            marker.write_bytes(b"preserve")
            with self.assertRaises(FileExistsError):
                commands_run(Path("unused-simulator"), directory)
            self.assertEqual(list(directory.iterdir()), [marker])
            self.assertEqual(marker.read_bytes(), b"preserve")

    def test_catsort_preserves_complete_descriptor_rows(self) -> None:
        rows = ["alpha        bs    utility                2048.22 83.2665 0",
                "saved            36 disc                   d.860930.0759 0",
                "trz         2056.10 reader                 d.890207.1014 0"]
        report = "catsort  page 1, name of catalog: catalog\r\n\r\n"
        report += "\r\n".join(rows) + "\r\n total: 36 segm. 3 entr.\f\n"
        self.assertEqual(sorted_rows(report), rows)
        self.assertEqual(sorted_names(report), ["alpha", "saved", "trz"])
        changed = report.replace("83.2665", "83.2666")
        self.assertNotEqual(sorted_rows(changed), rows)
        self.assertEqual(sorted_names(changed), sorted_names(report))

    def test_search_descriptor_names_allow_touching_equals_not_duplicates(self) -> None:
        report = "elevenchars=set 36 disc 0 0 0 0 0 ; system\n"
        report += "           ; 1 2 3 -8388607 8388605\n"
        report += "saved      =set 36 disc 0 0 0 0 0 ; user\n"
        self.assertEqual(descriptor_names(report), {"elevenchars", "saved"})
        with self.assertRaises(AssertionError):
            descriptor_names(report + "saved=set 36 disc 0 ; user\n")

    def test_catalog_raw_decode_distinguishes_retained_and_temporary(self) -> None:
        raw = bytearray(b"\xff" * (72 * 768))
        for slot, name, permission in ((0, b"retained", 3), (1, b"temporary", 0)):
            offset = slot * 51
            raw[offset:offset + 51] = bytes(51)
            raw[offset:offset + 3] = permission.to_bytes(3, "big")
            raw[offset + 9:offset + 9 + len(name)] = name
        self.assertEqual(packed_catalog(raw), Counter({(0, "retained"): 1, (0, "temporary"): 1}))
        self.assertEqual(packed_catalog(raw, retained_only=True), Counter({(0, "retained"): 1}))
        for bad in (raw[:-1], raw + b"x"):
            with self.assertRaises(AssertionError):
                packed_catalog(bad)
        raw[9:21] = bytes(12)
        with self.assertRaises(AssertionError):
            packed_catalog(raw)

    def test_catstat_requires_all_segments_and_consistent_counts(self) -> None:
        body = "catstat\nsegment 0 2 entries\n" + " " * 14 + f"{'retained':12}{'temporary':12}\n"
        body += "".join(f"segment {n} 0 entries\n" for n in range(1, 72))
        good = body + "\nend 18\n\x07"
        self.assertEqual(catstat_report(good), Counter({(0, "retained"): 1, (0, "temporary"): 1}))
        for old, new in (("segment 0 2", "segment 0 3"),
                         ("segment 71 0 entries\n", ""),
                         ("segment 70 0", "segment 69 0"),
                         (" " * 14 + "retained", "retained"),
                         ("end 18", "end 19")):
            with self.assertRaises(AssertionError):
                catstat_report(good.replace(old, new))

    def test_catalog_resource_reports_require_consistent_units(self) -> None:
        good = ("area 5 buf 7 size 100000 first core 44840\n"
                "disc: 3 segm/slice\n"
                "temp 201 segm 67 slices 10 entr\n"
                "login 801 segm 267 slices 20 entr\n"
                "perm 801 segm 267 slices 20 entr\n")
        header, claims = claim_report(good)
        self.assertEqual(header, (5, 7, 100000, 44840))
        self.assertEqual(claims["temp"], (201, 67, 10))
        for old, new in (("size 100000", "size 90000"), ("44840", "44841"),
                         ("201 segm", "200 segm"), ("10 entr", "0 entr"),
                         ("perm 801 segm 267 slices 20 entr\n", "")):
            with self.assertRaises(AssertionError):
                claim_report(good.replace(old, new))
        space = "free disc space:\ndisc : 991 slices, 2973 segm.\n"
        self.assertEqual(free_space(space), (991, 2973))
        for bad in (space.replace("2973", "2972"), "", space + space):
            with self.assertRaises(AssertionError):
                free_space(bad)

    def test_catalog_probe_preserves_existing_work(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            marker = directory / "keep"
            marker.write_bytes(b"preserve")
            with self.assertRaises(FileExistsError):
                catalog_run(Path("unused-simulator"), directory)
            self.assertEqual(list(directory.iterdir()), [marker])
            self.assertEqual(marker.read_bytes(), b"preserve")

    def test_persistence_requires_complete_user_metadata(self) -> None:
        good = ("keeptext =set 36 disc d.860930.0759 0 0 0 0 ; user\r\n"
                "           ; 1005 70 3 8388605 8388605\r\n\x07")
        result = persistence_descriptor(good, "keeptext", "user")
        self.assertEqual(result["details"], [1005, 70, 3, 8388605, 8388605])
        self.assertEqual(result["tail"][:2], ["36", "disc"])
        for old, new in (("36 disc", "0 disc"), ("36 disc", "-1 disc"),
                         ("36 disc", "36 other"), ("keeptext", "wrongname"),
                         ("; user", "; temp"), ("70 3", "70 0"),
                         ("8388605 8388605", "8388604 8388605"),
                         ("           ; 1005 70 3 8388605 8388605\r\n", "")):
            with self.assertRaises(AssertionError):
                persistence_descriptor(good.replace(old, new), "keeptext", "user")
        with self.assertRaises(AssertionError):
            persistence_descriptor(good + "\n" + good, "keeptext", "user")
        temporary = good.replace("; user", "; temp").replace("70 3", "70 0")
        self.assertEqual(persistence_descriptor(temporary, "keeptext", "temp")["details"][2], 0)

    def test_persistence_checks_text_binary_padding_and_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            text = b"Retained sample.\n"
            binary = bytes(range(256)) * 3
            padded = binary + b"x" * 768
            expected = {"input": text, "beforetext": text, "aftertext": text,
                        "bininput": binary, "beforebin": padded, "afterbin": padded}
            for name, data in expected.items():
                (directory / name).write_bytes(data)
            verify_persistence_exports(directory, text, binary)
            for name, data in expected.items():
                path = directory / name
                path.write_bytes(b"wrong contents")
                with self.assertRaises(AssertionError):
                    verify_persistence_exports(directory, text, binary)
                path.unlink()
                with self.assertRaises(OSError):
                    verify_persistence_exports(directory, text, binary)
                path.write_bytes(data)
            for bad in (binary + b"x", binary, binary + b"y" * 768):
                (directory / "afterbin").write_bytes(bad)
                with self.assertRaises(AssertionError):
                    verify_persistence_exports(directory, text, binary)

    def test_persistence_preserves_existing_work(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            marker = directory / "keep"
            marker.write_bytes(b"preserve")
            with self.assertRaises(FileExistsError):
                persistence_run(Path(sys.executable), directory)
            self.assertEqual(list(directory.iterdir()), [marker])
            self.assertEqual(marker.read_bytes(), b"preserve")

    def test_editor_requires_every_result_and_preserves_source_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            expected = {**EDITOR_EXPORTS, "input": EDITOR_SAMPLE, "edinput": EDITOR_LINES}
            for name, data in expected.items():
                (directory / name).write_bytes(data)
            verify_editor_exports(directory)
            for name, data in expected.items():
                path = directory / name
                path.write_bytes(b"wrong editor result")
                with self.assertRaises(AssertionError):
                    verify_editor_exports(directory)
                path.unlink()
                with self.assertRaises(OSError):
                    verify_editor_exports(directory)
                path.write_bytes(data)
            verify_editor_exports(directory)

    def test_comparison_report_requires_exact_replacement_and_one_difference(self) -> None:
        good = ("cmp msrc medit\r\n"
                "DISCREPANCY NO. 1  msrc line 1, medit line 1  TEXT REPLACED\r\n"
                "=========>> msrc:\r\nMonitor manual sample.\r\n"
                "=========>> medit:\r\nEdited manual sample.\r\n"
                " 1 discrepancy found\r\n\x07")
        self.assertEqual(len(comparison_report(good)), 6)
        for old, new in (("line 1", "line 2"), ("TEXT REPLACED", "TEXT DELETED"),
                         ("Edited manual sample.", "Wrong text."),
                         ("1 discrepancy found", "2 discrepancies found"),
                         ("DISCREPANCY NO. 1", "DISCREPANCY NO. 2")):
            with self.assertRaises(AssertionError):
                comparison_report(good.replace(old, new))
        with self.assertRaises(AssertionError):
            comparison_report(good + good)

    def test_cmp_break_probe_rejects_compatibility_mode(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(ValueError):
                editor_run(Path("unused"), Path(temporary), compatibility=True, check_cmp_break=True)

    def test_cmp_break_requires_the_specific_failure_not_success(self) -> None:
        good = ("cmp msrc moved\r\nbreak        0  tofrom1\r\n"
                "called from     line 219-220\r\ncalled from     line 380-381\r\n"
                "from s\r\npause edcheck break fp")
        verify_cmp_break(good)
        for old, new in (("tofrom1", "elsewhere"), ("219-220", "218-219"),
                         ("break        0", "break        1"), ("edcheck", "other")):
            with self.assertRaises(AssertionError):
                verify_cmp_break(good.replace(old, new))
        for extra in ("\n***fp reinitialized", "\nfiles are identical", "\nDISCREPANCY NO. 1"):
            with self.assertRaises(AssertionError):
                verify_cmp_break(good + extra)

    def test_editor_preserves_existing_work(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            marker = directory / "keep"
            marker.write_bytes(b"preserve")
            with self.assertRaises(FileExistsError):
                editor_run(Path("unused-simulator"), directory)
            self.assertEqual(list(directory.iterdir()), [marker])
            self.assertEqual(marker.read_bytes(), b"preserve")

    def test_regnetest_requires_both_sums_and_exact_run_counters(self) -> None:
        good = ("sum af tal= 360.700643456 test ok\r\n"
                "sum af tal= 1.85598980864'3 test ok\r\n"
                "antal okruns = 1\r\nantal fejlruns = 0\r\n")
        verify_iteration(good)
        for old, new in (("360.700643456", "360.7"),
                         ("1.85598980864'3", "1.855"),
                         ("test ok", "test failed"),
                         ("antal okruns = 1", "antal okruns = 0"),
                         ("antal fejlruns = 0", "antal fejlruns = 1"),
                         ("antal fejlruns = 0\r\n", "")):
            with self.assertRaises(AssertionError):
                verify_iteration(good.replace(old, new))
        for failure in ("pause test break fp", "***fp reinitialized"):
            with self.assertRaises(AssertionError):
                verify_iteration(good + failure)

    def test_regnetest_preserves_existing_log_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            marker = directory / "keep"
            marker.write_bytes(b"preserve")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tests/python/test_regnetest_cli.py"),
                 "unused-simulator", "--protected", "--output", str(directory)],
                capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"File exists", result.stderr)
            self.assertEqual(list(directory.iterdir()), [marker])
            self.assertEqual(marker.read_bytes(), b"preserve")

    def test_profile_configuration_is_explicit_and_relative(self) -> None:
        for profile, mode in (("compat", "OFF"), ("protected", "RC4000")):
            config = profiles.configuration(profile)
            self.assertIn(f"PROTECTION MODE={mode}\n", config)
            suffix = ".protected" if profile == "protected" else ""
            self.assertIn(f"MONITOR FILE=MONITOR{suffix}\n", config)
            self.assertIn(f"FILE=UNIT0{suffix}\n", config)
            self.assertIn("ROOT=exchange\n", config)
            self.assertEqual(config.count("PROTOCOL=CLASSIC"), 2)
            self.assertNotIn("\r", config)
            self.assertNotIn("WRITABLE=YES", config)
        with self.assertRaises(CopyError):
            profiles.configuration("v2")

    def test_profile_preparation_refuses_unknown_input_before_writing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            unknown = work / "unknown"
            unknown.write_bytes(b"not the accepted monitor")
            destination = work / "profiles"
            with self.assertRaises(CopyError):
                profiles.prepare(destination, unknown, ROOT / "src/disks/UNIT0")
            self.assertFalse(destination.exists())
            candidate = work / "candidate"
            candidate.write_bytes(b"\x22" * 38400)
            with patch.object(profiles, "PROTECTED_SHA256",
                              hashlib.sha256(candidate.read_bytes()).hexdigest()):
                with self.assertRaises(CopyError):
                    profiles.prepare(destination, candidate, unknown)
            self.assertFalse(destination.exists())

    def test_profile_preparation_copies_and_preserves_existing_work(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            candidate = work / "candidate"
            candidate.write_bytes(b"\x22" * 38400)
            source = ROOT / "src/disks/UNIT0"
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            # Synthetic accepted monitor/disk identities are local to this test.
            with patch.object(profiles, "PROTECTED_SHA256",
                              hashlib.sha256(candidate.read_bytes()).hexdigest()), \
                 patch.object(profiles, "PROFILE_SHA256", digest):
                destination = work / "profiles"
                profiles.prepare(destination, candidate, source)
                self.assertEqual((destination / "MONITOR.protected").read_bytes(),
                                 candidate.read_bytes())
                self.assertEqual((destination / "MONITOR").read_bytes(),
                                 (ROOT / "src/monitor/MONITOR").read_bytes())
                for name in ("compat", "protected"):
                    suffix = ".protected" if name == "protected" else ""
                    self.assertEqual((destination / ("UNIT0" + suffix)).read_bytes(),
                                     source.read_bytes())
                self.assertTrue((destination / "exchange").is_dir())
                before = {p.relative_to(destination): p.read_bytes()
                          for p in destination.rglob("*") if p.is_file()}
                with self.assertRaises(FileExistsError):
                    profiles.prepare(destination, candidate, source)
                self.assertEqual(before, {p.relative_to(destination): p.read_bytes()
                                          for p in destination.rglob("*") if p.is_file()})
            self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), digest)

    def test_source_preparation_preserves_originals(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source"
            work = Path(temporary) / "work"
            source.mkdir()
            work.mkdir()
            names = [f"montxt{i}" for i in range(1, 12)] + ["mon8s", "mon8init"]
            for name in names:
                (source / name).write_bytes(b"; source comment\r\nb. w. 0 e.\r\n")
            data = b"head cpu\r\n; comment \x86 \x91\r\n"
            (source / BUILD_FILE).write_bytes(data)
            originals = {p.name: p.read_bytes() for p in source.iterdir()}
            prepare_sources(source, work)
            self.assertEqual((work / "mbuild").read_bytes(),
                             b"head cpu\n; comment } {\n")
            self.assertEqual((work / "mon8init").read_bytes(),
                             b"; source comment\nb. w. 0 e.\n")
            self.assertIn(hashlib.sha256(data).hexdigest(),
                          (work / "sources.txt").read_text())
            self.assertEqual(originals, {p.name: p.read_bytes() for p in source.iterdir()})
            (source / "montxt1").write_bytes(b"w. \x86\n")
            with self.assertRaises(CopyError):
                prepare_sources(source, work)
            (source / "montxt1").write_bytes(b"; unsupported \xff\n")
            with self.assertRaises(CopyError):
                prepare_sources(source, work)

    def test_maintained_build_file_preserves_initial_generation_commands(self) -> None:
        self.assertEqual(BUILD_FILE, "sim4000.mops")
        historical = (ROOT / "monsrc/hjmops.new").read_bytes()
        normalized = historical.replace(b"\r\n", b"\n").translate(
            bytes.maketrans(b"\x86\x91", b"}{"))
        maintained = (ROOT / "monsrc" / BUILD_FILE).read_bytes()
        self.assertNotIn(b"\r", maintained)
        self.assertTrue(maintained.isascii())
        header, commands = maintained.split(b"\n\n", 1)
        self.assertIn(b"keep hjmops* unchanged", header)
        printer = b"t.18 i.10 d.7 n.printer             ; RC610 line printer\n"
        self.assertEqual(commands.count(printer), 1)
        self.assertEqual(commands.replace(printer, b"").rstrip(), normalized.rstrip())
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            prepare_sources(ROOT / "monsrc", work)
            self.assertEqual((work / "mbuild").read_bytes(), maintained)
            self.assertFalse(any(work.glob("hjmops*")))

    def test_historical_variant_option_is_not_supported(self) -> None:
        result = subprocess.run([sys.executable, str(ROOT / "dev/python/rebuild_monitor.py"),
                                 "unused", "--variant", "hjmops.new"],
                                capture_output=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"unrecognized arguments", result.stderr)

    def test_listing_status(self) -> None:
        success = b"23782 33168 *** procfunc test start\nslang ok 11/24980/49\n"
        self.assertTrue(listing_succeeded(success))
        self.assertFalse(listing_succeeded(b""))
        for failure in (b"***slang sorry 1 0/0/0", b"***edit", b"syntax error:"):
            self.assertFalse(listing_succeeded(success + failure))
        for failure in (b"***fp syntax", b"input nesting", b"***EDIT", b"***device status binmon"):
            self.assertFalse(listing_succeeded(success + failure))

    def test_protected_preparation_is_separate_and_guarded(self) -> None:
        source = ROOT / "monsrc"
        before = {p.name: p.read_bytes() for p in source.iterdir()}
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            prepare_sources(source, work, protected=True)
            prepared = (work / "montxt10").read_bytes()
            self.assertEqual(prepared, before["montxt10"])
            self.assertIn(PROTECTION_BYPASS, prepared)
            self.assertIn(b"g0:  ks  w0  x1-20", prepared)
            self.assertIn(b"g1:  ks  w0  x1 -2", prepared)
            self.assertIn(hashlib.sha256(prepared).hexdigest(),
                          (work / "sources.txt").read_text())
            self.assertEqual((work / "mbuild").read_bytes(), before[PROTECTED_BUILD_FILE])
            self.assertEqual((work / "mcommon").read_bytes(), before[BUILD_FILE])
            for name in [f"montxt{i}" for i in range(1, 12)] + ["mon8s", "mon8init"]:
                if name != "montxt10":
                    self.assertEqual((work / name).read_bytes(), before[name])
        self.assertEqual(before, {p.name: p.read_bytes() for p in source.iterdir()})
        original = before["montxt10"]
        for invalid in (b"", original + original, original.replace(PROTECTION_BYPASS, b""),
                        original.replace(b"g0:  ks", b"g0:  jl"),
                        original.replace(b"94-06-01", b"94-06-02")):
            with self.assertRaises(CopyError):
                validate_protection_source(invalid)

    def test_guest_protection_result_requires_exact_edit_and_restoration(self) -> None:
        original = (ROOT / "monsrc/montxt10").read_bytes()
        # Synthetic exported result, never a prepared/patched build source.
        edited = original.replace(PROTECTION_BYPASS, b"")
        verify_guest_protection_edit(original, edited, original)
        for wrong, restored in ((original, original), (b"", original),
                                (edited + b"extra\n", original), (edited, edited)):
            with self.assertRaises(CopyError):
                verify_guest_protection_edit(original, wrong, restored)

    def test_protected_input_depth_is_two(self) -> None:
        common = (ROOT / "monsrc" / BUILD_FILE).read_bytes()
        entry = (ROOT / "monsrc" / PROTECTED_BUILD_FILE).read_bytes()
        validate_protected_inputs(common, entry)
        for bad_common, bad_entry in ((common + b"\ni third\n", entry),
                                     (common, entry + b"\ni third\n"),
                                     (common, entry.replace(b"i mcommon", b"i wrong"))):
            with self.assertRaises(CopyError):
                validate_protected_inputs(bad_common, bad_entry)

    def test_protected_bad_input_is_rejected_before_preparing_sources(self) -> None:
        original = (ROOT / "monsrc/montxt10").read_bytes()
        bad_inputs = (None, b"", original + original,
                      original.replace(PROTECTION_BYPASS, b""),
                      original.replace(b"skip set pk action", b"changed bypass tag"))
        for bad in bad_inputs:
            with self.subTest(missing=bad is None, size=None if bad is None else len(bad)):
                with tempfile.TemporaryDirectory() as temporary:
                    source = Path(temporary) / "source"
                    work = Path(temporary) / "work"
                    source.mkdir()
                    work.mkdir()
                    for name in (BUILD_FILE, PROTECTED_BUILD_FILE):
                        (source / name).write_bytes((ROOT / "monsrc" / name).read_bytes())
                    if bad is not None:
                        (source / "montxt10").write_bytes(bad)
                    before = {p.name: p.read_bytes() for p in source.iterdir()}
                    with self.assertRaises((CopyError, FileNotFoundError)):
                        prepare_sources(source, work, protected=True)
                    self.assertEqual(list(work.iterdir()), [])
                    self.assertEqual(before, {p.name: p.read_bytes() for p in source.iterdir()})

    def test_existing_destination_is_not_overwritten(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            marker = work / "keep"
            marker.write_bytes(b"preserve me")
            result = subprocess.run([sys.executable, str(ROOT / "dev/python/rebuild_monitor.py"),
                                     str(work), "--sources", str(work)],
                                    capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"File exists", result.stderr)
            self.assertEqual(marker.read_bytes(), b"preserve me")


class RuntimeProbeTests(unittest.TestCase):
    def test_nested_evidence_requires_every_size_and_transition(self) -> None:
        lines = []
        for size in (2, 20, 26):
            for stage, parent, child in (("running", 2, 1), ("tree-stopped", 0, 0),
                    ("tree-resumed", 2, 1), ("child-stopped-by-parent", 2, 2),
                    ("tree-removed", 0, 0)):
                lines.append((f"Nested keys {stage}: parent={parent} child={child} size={size} "
                              "full-area=checked\n").encode("ascii"))
            lines.append((f"Nested guest case size={size} passed; privilege-widening=denied "
                          "stop-answer=released child-steps=1000\n").encode("ascii"))
        lines += [b"Protected nested-process checkpoint passed\n",
                  b"Remaining GG probe executions=0\n"]
        transcript = b"".join(lines)
        verify_nested_checkpoint(transcript)
        for line in lines:
            for invalid in (transcript.replace(line, b""), transcript + line):
                with self.assertRaises(CopyError):
                    verify_nested_checkpoint(invalid)
        with self.assertRaises(CopyError):
            verify_nested_checkpoint(transcript.replace(b"child-steps=1000", b"child-steps=0"))

    def test_protected_telnet_claim_bounds(self) -> None:
        self.assertEqual(telnet_claim(b"area 5 buf 7 size 100000 first core 44840\r\n"),
                         (44840, 144840))
        for invalid in (b"", b"*** claim failed", b"size 0 first core 44840",
                        b"size 100000 first core 1", b"size 100000 first core 44841",
                        b"size 100000 first core 1000000"):
            with self.assertRaises(AssertionError):
                telnet_claim(invalid)

    def test_protected_telnet_export_checks(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            for name in ("localout", "localagain", "localfinal"):
                (work / name).write_bytes(LOCAL_TEXT)
            for name in ("remoteout", "recreated", "reconnected"):
                (work / name).write_bytes(REMOTE_TEXT)
            verify_exports(work)
            for path in work.iterdir():
                original = path.read_bytes()
                path.write_bytes(b"wrong contents")
                with self.assertRaises(AssertionError):
                    verify_exports(work)
                path.unlink()
                with self.assertRaises(OSError):
                    verify_exports(work)
                path.write_bytes(original)

    def test_protected_telnet_rejects_unknown_monitor_before_writing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary) / "unused"
            with self.assertRaisesRegex(AssertionError, "unexpected protected monitor"):
                protected_telnet_run(Path("unused-simulator"), ROOT / "src/monitor/MONITOR",
                                     Path("unused-image"), work)
            self.assertFalse(work.exists())

    def test_library_base_diagnostic_comparison_is_strict(self) -> None:
        temporary = (b"9. oldalgollib bases\n"
                     b"Algol transfer binding=temporary base-diagnostic=present\n") * 2
        system = b"Algol transfer binding=system base-diagnostic=absent\n" * 2
        verify_base_diagnostics(temporary, False)
        verify_base_diagnostics(system, True)
        for transcript, system_binding in ((b"", False), (b"", True),
                (temporary, True), (system, False), (system + b"oldalgollib bases", True),
                (temporary + b"oldalgollib bases", False), (system + system, True)):
            with self.assertRaises(CopyError):
                verify_base_diagnostics(transcript, system_binding)

    def test_system_binding_requires_algol_before_writing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary) / "unused"
            result = subprocess.run(
                [sys.executable, str(ROOT / "dev/python/probe_rc4000_runtime.py"),
                 str(work), "--candidate", str(ROOT / "src/monitor/MONITOR"),
                 "--system-binding"], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 2)
            self.assertIn(b"requires --algol", result.stderr)
            self.assertFalse(work.exists())

    def test_protected_algol_export_guards(self) -> None:
        # Synthetic allocation/module records exercise the host validators;
        # they are not executable guest programs or compilation evidence.
        def linked_fixture(segments):
            data = bytearray(segments * 768)
            data[0:9] = bytes.fromhex("730022 a3004f a3003b")
            data[756:763] = b"system0"
            data[1524:1530] = b"tofrom"
            return bytes(data)

        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            expected = {"algtext": TEXT.replace(b"\r", b""),
                        "algbinary": BINARY + bytes(768),
                        "pcinbin": linked_fixture(54),
                        "pcoutbin": linked_fixture(46),
                        "hellobin": bytes(768), "arithbin": bytes(768)}
            for name, data in expected.items():
                (work / name).write_bytes(data)
            verify_algol_transfers(work)
            for name, data in expected.items():
                path = work / name
                path.write_bytes(b"wrong")
                with self.assertRaises(CopyError):
                    verify_algol_transfers(work)
                path.unlink()
                with self.assertRaises(OSError):
                    verify_algol_transfers(work)
                path.write_bytes(data)
            for data in (BINARY[:-1], BINARY + b"x", bytes(len(BINARY))):
                (work / "algbinary").write_bytes(data)
                with self.assertRaises(CopyError):
                    verify_algol_transfers(work)
            (work / "algbinary").write_bytes(expected["algbinary"])
            for changed_module in (True, False):
                data = bytearray(expected["pcinbin"])
                if changed_module:
                    data[1524:1531] = b"tofrom1"
                else:
                    data[768:777] = bytes.fromhex("730022 a3004f a3003b")
                (work / "pcinbin").write_bytes(data)
                with self.assertRaises(CopyError):
                    verify_algol_transfers(work)

    def test_each_sibling_recovery_export_is_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            for test in range(4):
                (work / f"after{test}").write_bytes(TEXT.replace(b"\r", b""))
            verify_isolation_transfers(work)
            for test in range(4):
                path = work / f"after{test}"
                path.write_bytes(b"wrong contents")
                with self.assertRaises(CopyError):
                    verify_isolation_transfers(work)
                path.unlink()
                with self.assertRaises(OSError):
                    verify_isolation_transfers(work)
                path.write_bytes(TEXT.replace(b"\r", b""))

    def test_combined_probe_modes_are_rejected_before_writing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary) / "unused"
            for modes in (("--lifecycle", "--isolation"), ("--algol", "--isolation"),
                          ("--algol", "--lifecycle"), ("--nested", "--lifecycle"),
                          ("--nested", "--isolation"), ("--nested", "--algol")):
                result = subprocess.run(
                    [sys.executable, str(ROOT / "dev/python/probe_rc4000_runtime.py"),
                     str(work), "--candidate", str(ROOT / "src/monitor/MONITOR"), *modes],
                    capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 2)
                self.assertIn(b"separate work directories", result.stderr)
                self.assertFalse(work.exists())

    def test_lifecycle_exports_are_checked_independently(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            expected = {"resumed": TEXT.replace(b"\r", b""),
                        "recreated": TEXT.replace(b"\r", b""),
                        "binresume": BINARY + bytes(768)}
            for name, data in expected.items():
                (work / name).write_bytes(data)
            verify_lifecycle_transfers(work)
            for name in expected:
                (work / name).write_bytes(b"wrong contents")
                with self.assertRaises(CopyError):
                    verify_lifecycle_transfers(work)
                (work / name).write_bytes(expected[name])
            for data in (BINARY[:-1], BINARY + b"x"):
                (work / "binresume").write_bytes(data)
                with self.assertRaises(CopyError):
                    verify_lifecycle_transfers(work)
            (work / "binresume").unlink()
            with self.assertRaises(OSError):
                verify_lifecycle_transfers(work)

    def test_lifecycle_probe_preserves_existing_destination(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            marker = work / "keep"
            marker.write_bytes(b"preserve me")
            result = subprocess.run(
                [sys.executable, str(ROOT / "dev/python/probe_rc4000_runtime.py"),
                 str(work), "--candidate", str(ROOT / "src/monitor/MONITOR"), "--lifecycle"],
                capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"File exists", result.stderr)
            self.assertEqual(list(work.iterdir()), [marker])
            self.assertEqual(marker.read_bytes(), b"preserve me")


if __name__ == "__main__":
    unittest.main()
