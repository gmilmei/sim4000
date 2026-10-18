#!/usr/bin/env python3
"""Pure guards for RC610 monitor inventory and private source preparation."""

from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
import probe_rc610_monitor as probe


def put(core, address, value):
    index = address // 2 * 3
    core[index:index + 3] = value.to_bytes(3, "big")


def fixture(printer=True):
    core = bytearray(524288 * 3)
    put(core, 74, 512)
    put(core, 76, 640)
    put(core, 78, 644)
    for device, address, kind, name in ((0, 1024, 10, b"reader"),
                                       (1, 1080, 12, b"punch"), (7, 1136, 14, b"printer")):
        if device == 7 and not printer:
            continue
        put(core, 512 + device * 2, address)
        put(core, address, kind)
        offset = (address + 2) // 2 * 3
        core[offset:offset + 12] = name.ljust(12, b"\0")
        put(core, address + 10, device * 64)
        if device == 7:
            put(core, address + 28, 451)
    return core


class PrinterMonitorTests(unittest.TestCase):
    def test_inventory_follows_slots_and_descriptor_fields(self):
        rows = probe.peripherals(fixture())
        self.assertEqual([(r["name"], r["device_number"], r["kind"])
                          for r in rows], [("reader", 0, 10), ("punch", 1, 12), ("printer", 7, 14)])
        self.assertEqual(rows[2]["device_field"], 448)
        self.assertEqual(rows[2]["parameters"][3], 451)

    def test_embedded_strings_and_area_slots_are_not_devices(self):
        core = fixture(False)
        put(core, 640, 1136)  # Outside b4..b5, even though it is inside b4..b6.
        put(core, 1136, 14)
        core[1707:1719] = b"printer".ljust(12, b"\0")
        self.assertEqual([r["name"] for r in probe.peripherals(core)], ["reader", "punch"])

    def test_invalid_lengths_and_table_ranges_are_rejected(self):
        for core in (b"", fixture()[:-1], fixture() + b"\0"):
            with self.assertRaises(ValueError):
                probe.peripherals(core)
        for address, value in ((74, 513), (76, 512), (76, 1048576), (76, 5000)):
            core = fixture()
            put(core, address, value)
            with self.assertRaises(ValueError):
                probe.peripherals(core)

    def test_invalid_descriptor_or_wrong_control_layout_is_rejected(self):
        for address, value in ((512, 1137), (512, 1048574), (512, 0), (1024, 12)):
            core = fixture()
            put(core, address, value)
            with self.assertRaises(ValueError):
                probe.peripherals(core)

    def test_source_preparation_changes_one_guarded_option_only(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "monsrc"
            source.mkdir()
            original = b"<:\nt.18 i.10 d.7 n.printer             ; RC610 line printer\nf. :>\n"
            (source / "sim4000.mops").write_bytes(original)
            (source / "hjmops.new").write_bytes(b"historical options, do not edit\n")
            (source / "montxt4").write_bytes(b"unchanged driver\n")
            destination = root / "candidate"
            with patch.object(probe, "ROOT", root), \
                 patch.object(probe, "OPTION_SHA256", probe.digest(source / "sim4000.mops")):
                probe.prepare_sources(destination)
                self.assertEqual((source / "sim4000.mops").read_bytes(), original)
                self.assertEqual((destination / "hjmops.new").read_bytes(),
                                 b"historical options, do not edit\n")
                self.assertEqual((destination / "montxt4").read_bytes(), b"unchanged driver\n")
                changed = (destination / "sim4000.mops").read_bytes()
                self.assertEqual(changed, original)
                before = {p.name: p.read_bytes() for p in destination.iterdir()}
                with self.assertRaises(FileExistsError):
                    probe.prepare_sources(destination)
                self.assertEqual(before, {p.name: p.read_bytes() for p in destination.iterdir()})

    def test_unknown_options_are_rejected_before_creating_destination(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "monsrc").mkdir()
            (root / "monsrc/sim4000.mops").write_bytes(b"unknown options\n")
            destination = root / "candidate"
            with patch.object(probe, "ROOT", root), self.assertRaises(ValueError):
                probe.prepare_sources(destination)
            self.assertFalse(destination.exists())


if __name__ == "__main__":
    unittest.main()
