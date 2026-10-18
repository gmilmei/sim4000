#!/usr/bin/env python3
"""Pure guards for protected MIPS/TS process-state diagnostics."""

from pathlib import Path
import hashlib
import json
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from probe_mips_protection import (process_state, catalog_fixture, saved_code_accessible,
                                   retained_session, KEEP_TEXT, verify_persistent_media,
                                   verify_isolation_fault, ISOLATION_CODE)
from probe_mips_ts import EMPTY_CATALOG, terminal_catalog_bytes
from probe_protected_upsoscat import verify_evidence as verify_updater, INPUT
from probe_rc4000_runtime import PROBE
from prepare_protected_mips_profile import configuration, PRIMARY, SECONDARY, ARTIFACTS
from prepare_mips_test import autostart_text
from make_unit0_copy import CopyError
from test_mips_files import manual_recipe


class ProtectedCandidateTests(unittest.TestCase):
    def test_unprotected_manual_file_recipe_matches_checked_dialogue(self):
        self.assertEqual(len(manual_recipe()), 8)

    def test_runtime_config_is_protected_loopback_and_separate_overlay_only(self):
        config = configuration(45678)
        lines = [line for line in config.splitlines() if line and not line.startswith(";")]
        self.assertIn("PROTECTION MODE=RC4000", lines)
        self.assertIn("MONITOR FILE=MONITOR.protected", lines)
        self.assertIn("TELNET DEVNO=4 ADDRESS=127.0.0.1 PORT=45678 ATTENTION=CTRL-A", lines)
        for unit, name in enumerate((PRIMARY, SECONDARY)):
            self.assertIn(f"DEFINE RC433X DEVNO={15 + unit} FILE={name} OVERLAY={name}.overlay", lines)
        self.assertNotIn("WRITABLE=YES", config)
        self.assertIn("DEFINE RC610 DEVNO=7 INTNO=10 FILE=printer.protected.txt APPEND=YES", lines)
        self.assertEqual(len(ARTIFACTS), 4)
        self.assertTrue(all("/" not in name and not name.endswith(".overlay") for name in ARTIFACTS))
        verify_persistent_media("".join(
            f"device={15 + unit} type=RC433X state=attached base=./{name} "
            f"mode=persistent-overlay overlay-sectors=2 overlay=./{name}.overlay\n"
            for unit, name in enumerate((PRIMARY, SECONDARY))), PRIMARY, SECONDARY)

    def test_autostart_keeps_two_levels_and_fixed_date(self):
        boot = autostart_text(b"startdate c\n* unstack\n")
        self.assertEqual(boot, b"* kit 16\n* date 86 09 30 08 00 00\n"
                         b"* read starttem\n* read startprimo\n* read startsos\n* unstack\n")
        self.assertNotIn(b"read startmips", boot)

    def test_candidate_rejects_unrecognized_boot_and_invalid_ports(self):
        for source in (b"unknown", b"startdate c\nstartdate c\n* unstack\n"):
            with self.assertRaises(CopyError):
                autostart_text(source)
        for port in (0, 1023, 65536):
            with self.assertRaises(AssertionError):
                configuration(port)


class UpdaterFaultTests(unittest.TestCase):
    def fixture(self, root):
        transcript = ("Candidate verification: RC4000 protection enabled\n"
                      "break 0 tofrom1\ncalled from line 1094-1099\n"
                      "pause hostcheck break fp\n"
                      "IRQ0 pc=00d3be raw=730022 saved=00d3c0 ea=000022 PR=bf key=1 monitor=0 W=000008,021771,02292b,0217e9\n"
                      "UPSOSCAT probe: task PL address=34 IRQ0 saved-next=yes PR=bf key=1\n"
                      "***fp reinitialized\n"
                      "UPSOSCAT known incompatibility captured; FP recovered, not a catalog build\n")
        (root / "session.log").write_text(transcript, encoding="ascii")
        binary = bytearray(114 * 768)
        binary[:len(PROBE)] = PROBE
        binary[756:763] = b"tofrom1"
        binary[1524:1531] = b"tofrom2"
        (root / "upbinary").write_bytes(binary)
        (root / "upmetadata").write_text("upsoscat =set 114 disc d.930330 ; system\n", encoding="ascii")
        for name in ("input", "inputafter"):
            (root / name).write_bytes(INPUT)
        return transcript

    def test_exact_updater_fault_and_ordinary_monitor_calls(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            transcript = self.fixture(root)
            (root / "session.log").write_text(transcript +
                "IRQ0 pc=000100 raw=380812 saved=000102 ea=fff812 PR=bf key=1 monitor=0 W=0,0,0,0\n", encoding="ascii")
            self.assertEqual(verify_updater(root)["fault_pc"], 54206)

    def test_wrong_instruction_context_and_duplicate_faults_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            transcript = self.fixture(root)
            fault = next(line for line in transcript.splitlines() if line.startswith("IRQ0"))
            for bad in (transcript.replace("730022", "730024"), transcript.replace("saved=00d3c0", "saved=00d3c2"),
                        transcript.replace("monitor=0", "monitor=1"), transcript.replace("key=1 monitor", "key=0 monitor"),
                        transcript.replace("PR=bf key", "PR=80 key"), transcript + fault + "\n"):
                (root / "session.log").write_text(bad, encoding="ascii")
                with self.assertRaises(AssertionError):
                    verify_updater(root)

    def test_binary_identity_and_preserved_input_are_required(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.fixture(root)
            (root / "inputafter").write_bytes(b"different\n")
            with self.assertRaises(AssertionError):
                verify_updater(root)
            self.fixture(root)
            binary = bytearray((root / "upbinary").read_bytes())
            binary[756:763] = b"unknown"
            (root / "upbinary").write_bytes(binary)
            with self.assertRaises(AssertionError):
                verify_updater(root)


class IsolationFaultTests(unittest.TestCase):
    def setUp(self):
        self.core = bytearray(524288 * 3)
        self.base = 80000
        offset = self.base // 2 * 3
        self.core[offset:offset + len(ISOLATION_CODE)] = ISOLATION_CODE
        self.response = "badstore\r\n\r\n***break 0 80012\r\n08.00 sos: test removed after break\r"

    def test_exact_store_fault_matches_code_and_child_bounds(self):
        self.assertEqual(verify_isolation_fault(self.response, self.core, (79000, 81000)), 80012)

    def test_fp_reserved_words_are_not_instruction_bytes(self):
        offset = self.base // 2 * 3
        self.core[offset:offset + 6] = bytes.fromhex("123456 abcdef")
        self.assertEqual(verify_isolation_fault(self.response, self.core, (79000, 81000)), 80012)

    def test_wrong_cause_pc_code_removal_or_bounds_is_rejected(self):
        for response in (self.response.replace("break 0", "break 8"),
                         self.response.replace("80012", "80014"),
                         self.response.replace("after break", "after finis"),
                         self.response + self.response):
            with self.assertRaises(AssertionError):
                verify_isolation_fault(response, self.core, (79000, 81000))
        with self.assertRaises(AssertionError):
            verify_isolation_fault(self.response, self.core, (81000, 82000))
        self.core[self.base // 2 * 3 + 15] ^= 1
        with self.assertRaises(AssertionError):
            verify_isolation_fault(self.response, self.core, (79000, 81000))


class RetainedSessionTests(unittest.TestCase):
    def test_persistent_media_accepts_wrapping_but_not_wrong_storage(self):
        media = "".join(f"device={15 + unit} type=RC433X state=attached base=./UNIT{unit}.protected "
                        f"mode=persistent-over\nlay overlay-sectors=2 overlay=./UNIT{unit}.protected.overlay\n"
                        for unit in (0, 1))
        verify_persistent_media(media)
        for bad in (media.replace("persistent-over\nlay", "volatile-overlay", 1),
                    media.replace("overlay=./UNIT1.protected.overlay", "overlay=./UNIT0.protected.overlay"),
                    media + media):
            with self.assertRaises(AssertionError):
                verify_persistent_media(bad)

    def exchange(self, create, descriptor=None, content=None):
        commands = []
        replies = (["keepsake=copy keepseed\r\n\x07", "scope user keepsake\r\n\x07"]
                   if create else [])
        replies += [descriptor or "lookup keepsake\r\nkeepsake =set 1 disc d.860930 ; user\r\n\x07",
                    content or "c=copy keepsake\r\n" + KEEP_TEXT.decode("ascii").replace("\n", "\r\n") + "\x07"]
        def until(marker):
            self.assertEqual(marker, b"\x07")
            return replies.pop(0)
        retained_session(create)(commands.append, until)
        self.assertFalse(replies)
        return commands

    def test_first_boot_creates_and_retains_file_remotely(self):
        self.assertEqual(self.exchange(True), [b"keepsake=copy keepseed\r\n",
                         b"scope user keepsake\r\n", b"lookup keepsake\r\n", b"c=copy keepsake\r\n"])

    def test_second_boot_only_reads_existing_file(self):
        self.assertEqual(self.exchange(False), [b"lookup keepsake\r\n", b"c=copy keepsake\r\n"])

    def test_temporary_or_missing_file_is_rejected(self):
        for descriptor in ("keepsake =set 1 disc d.860930 ; temp\r\n\x07",
                           "***lookup keepsake unknown\r\n\x07"):
            with self.assertRaises(AssertionError):
                self.exchange(False, descriptor=descriptor)

    def test_wrong_or_duplicate_content_is_rejected(self):
        for content in ("different text\r\n\x07",
                        KEEP_TEXT.decode("ascii").replace("\n", "\r\n") * 2 + "\x07"):
            with self.assertRaises(AssertionError):
                self.exchange(False, content=content)


class CatalogFixtureTests(unittest.TestCase):
    def test_demo_catalog_requires_correct_declared_kind_and_exact_bytes(self):
        with tempfile.TemporaryDirectory(prefix="sim4000-demo-guard-") as temporary:
            root = Path(temporary)
            (root / "exchange").mkdir()
            data = terminal_catalog_bytes(30, True)
            packed = root / "exchange/catalogbin"
            packed.write_bytes(data)
            manifest = {"guest_updater_verified": True, "protection": "off",
                        "account_provisioned": True,
                        "catalog_sha256": hashlib.sha256(data).hexdigest()}
            result = root / "catalog-result.json"
            result.write_text(json.dumps(manifest), encoding="ascii")
            self.assertEqual(catalog_fixture(root), data)
            for value in (False, "true"):
                result.write_text(json.dumps({**manifest, "account_provisioned": value}), encoding="ascii")
                with self.assertRaises(AssertionError):
                    catalog_fixture(root)
            result.write_text(json.dumps(manifest), encoding="ascii")
            packed.write_bytes(bytes([data[0] ^ 1]) + data[1:])
            with self.assertRaises(AssertionError):
                catalog_fixture(root)

    def test_import_requires_exact_guest_catalog_and_matching_manifest(self):
        with tempfile.TemporaryDirectory(prefix="sim4000-catalog-guard-") as temporary:
            root = Path(temporary)
            (root / "exchange").mkdir()
            packed = root / "exchange/catalogbin"
            packed.write_bytes(EMPTY_CATALOG)
            manifest = {"guest_updater_verified": True, "protection": "off",
                        "catalog_sha256": hashlib.sha256(EMPTY_CATALOG).hexdigest()}
            result = root / "catalog-result.json"
            result.write_text(json.dumps(manifest), encoding="ascii")
            self.assertEqual(catalog_fixture(root), EMPTY_CATALOG)
            for field, value in (("guest_updater_verified", False), ("protection", "rc4000"),
                                 ("catalog_sha256", "0" * 64)):
                result.write_text(json.dumps({**manifest, field: value}), encoding="ascii")
                with self.assertRaises(AssertionError):
                    catalog_fixture(root)
            result.write_text(json.dumps(manifest), encoding="ascii")
            packed.write_bytes(EMPTY_CATALOG[:-1] + b"\0")
            with self.assertRaises(AssertionError):
                catalog_fixture(root)

    def test_absent_provenance_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="sim4000-catalog-missing-") as temporary:
            root = Path(temporary)
            (root / "exchange").mkdir()
            (root / "exchange/catalogbin").write_bytes(EMPTY_CATALOG)
            with self.assertRaises(FileNotFoundError):
                catalog_fixture(root)


class ProcessStateTests(unittest.TestCase):
    def test_saved_code_access_uses_pr_not_pk(self):
        state = {"pr": 0xe7, "pk": 3}
        self.assertFalse(saved_code_accessible(state, 5))
        self.assertTrue(saved_code_accessible(state, 3))
        self.assertTrue(saved_code_accessible(state, 4))
        self.assertFalse(saved_code_accessible(state, 0))
        self.assertFalse(saved_code_accessible(state, 2))
        self.assertFalse(saved_code_accessible(state, 8))

    def test_child_descriptor_requires_sos_parent(self):
        self.name(256, b"sos")
        self.assertEqual(process_state(self.data, "tem", 1000, 100, "sos")["pk"], 1)
        with self.assertRaises(AssertionError):
            process_state(self.data, "tem", 1000, 100)

    def setUp(self):
        self.data = bytearray(4096 * 3)
        self.descriptor(128)
        self.name(256, b"s")

    def word(self, address, value):
        offset = address // 2 * 3
        self.data[offset:offset + 3] = value.to_bytes(3, "big")

    def name(self, address, name):
        offset = (address + 2) // 2 * 3
        self.data[offset:offset + 12] = name.ljust(12, b"\0")

    def descriptor(self, address):
        self.name(address, b"tem")
        for delta, value in ((18, address + 18), (20, address + 18), (22, 1000),
                             (24, 1100), (32, 0xbf * 4096 + 1), (48, 1040), (50, 256)):
            self.word(address + delta, value)

    def test_live_fields(self):
        state = process_state(self.data, "tem", 1000, 100)
        self.assertEqual((state["descriptor"], state["pr"], state["pk"], state["saved_ic"]),
                         (128, 0xbf, 1, 1040))

    def test_copied_descriptor_is_not_live(self):
        begin = 128 // 2 * 3
        clone = 1024 // 2 * 3
        self.data[clone:clone + 120] = self.data[begin:begin + 120]
        self.assertEqual(process_state(self.data, "tem", 1000, 100)["descriptor"], 128)

    def test_stopped_task_preserves_fault_context(self):
        self.word(128 + 10, 4096 + 143)
        for delta, value in ((34, 0x1fffff), (36, 1080), (38, 1), (40, 2),
                             (42, 3), (44, 4), (46, 0x200000), (48, 1042)):
            self.word(128 + delta, value)
        state = process_state(self.data, "tem", 1000, 100)
        self.assertEqual((state["stop_count"], state["saved_ic"], state["exceptions"]),
                         (1, 1042, 0x200000))
        self.assertEqual(state["w"], [1, 2, 3, 4])

    def test_two_live_candidates_are_rejected(self):
        self.descriptor(512)
        with self.assertRaises(AssertionError):
            process_state(self.data, "tem", 1000, 100)

    def test_invalid_links_parent_or_bounds_are_rejected(self):
        for address, bad in ((128 + 18, 8190), (128 + 20, 147),
                             (128 + 24, 1200), (128 + 50, 8192)):
            original = self.data[:]
            self.word(address, bad)
            with self.assertRaises(AssertionError):
                process_state(self.data, "tem", 1000, 100)
            self.data = original


if __name__ == "__main__":
    unittest.main()
