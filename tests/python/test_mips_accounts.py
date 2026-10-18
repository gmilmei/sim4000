#!/usr/bin/env python3
"""Pure guards for the fixed optional larger-account SOS fixture."""

from pathlib import Path
import io
import sys
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from probe_mips_account import (catalog_text, verify_catalog, verify_develop_listing,
                                verify_fp_reply, verify_host_inclusion)
from probe_mips_ts import terminal_catalog_bytes
from probe_mips_sessions import Terminal, verify_sessions, verify_project_seed


class OptionalAccountTests(unittest.TestCase):
    def fixture(self):
        original = terminal_catalog_bytes(30, True)
        data = bytearray(b"\xff" * (3 * 768))
        data[:15] = b"test" + bytes(8) + bytes.fromhex("000001")
        data[15:30] = b"develop" + bytes(5) + bytes.fromhex("000002")
        data[759:768] = bytes.fromhex("000002 000032 ffffff")
        data[768:1314] = original[768:1314]
        record = bytearray(original[768:1314])
        record[21:33] = b"develop" + bytes(5)
        for offset, value in ((33, 100000), (36, 150000), (126, 20), (129, 500),
                              (132, 4), (135, 100), (138, 4), (141, 100), (144, 4), (147, 100)):
            record[offset:offset + 3] = value.to_bytes(3, "big")
        data[1536:2082] = record
        data[2082:2121] = original[1314:1353]
        return data

    def test_exact_two_account_catalog(self):
        verify_catalog(self.fixture(), "soscat =set 3 disc d.860930 ; temp\r\n")

    def test_second_terminal_is_explicit_and_strict(self):
        data = self.fixture()
        for offset in (3, 6, 9, 12, 18):
            data[1536 + offset:1536 + offset + 3] = (100).to_bytes(3, "big")
        second = (b"terminal1" + bytes(3) + b"2\0\0" + b"develop" + bytes(5) +
                  bytes.fromhex("03c002") + bytes(9))
        data[2121:2160] = second
        descriptor = "soscat =set 3 disc d.860930 ; temp\r\n"
        verify_catalog(data, descriptor, two_terminals=True)
        with self.assertRaises(AssertionError):
            verify_catalog(data, descriptor)
        for offset in (1539, 1545, 1551, 1554, 2121, 2133, 2136, 2148, 2151, 2160):
            broken = bytearray(data)
            broken[offset] ^= 1
            with self.assertRaises(AssertionError):
                verify_catalog(broken, descriptor, two_terminals=True)
        self.assertEqual(catalog_text(True), catalog_text().replace("proc develop\n", "proc develop\nstdbase 100 100\nuserbase 100 100\nmaxbase 0 100\n") + 'term terminal1 "2" "develop" 60 2\n')

    def test_simultaneous_process_bounds_and_removal(self):
        parent = ("tem 45000 11000 1 0 wait s\r\nprimo 56000 16938 2 0 wait s\r\n"
                  "sos 73000 300000 5 0 wait s\r\n")
        demo = "test 78000 30208 3 0 stop sos\r\n"
        develop = "develop 110000 150016 4 0 stop sos\r\n"
        listing = parent + demo + develop
        verify_sessions(listing)
        verify_sessions(listing.replace("110000", "78000"))
        verify_sessions(parent + develop, ("develop",))
        verify_sessions(parent, ())
        for broken in (parent + demo, listing + demo, listing.replace("110000", "400000"),
                       listing.replace("150016", "30208"), listing.replace("4 0 stop", "4 1 stop"),
                       listing.replace("stop sos", "stop s")):
            with self.assertRaises(AssertionError):
                verify_sessions(broken)
        with self.assertRaises(AssertionError):
            verify_sessions(listing, ("develop",))

    def test_telnet_negotiation_remains_binary(self):
        sent = []
        terminal = Terminal.__new__(Terminal)
        wire = b"\x1b[2J\x1b[H\xff\xfb\x01\xff\xfd\x03"
        terminal.remote = SimpleNamespace(until=lambda marker: wire, send=sent.append)
        terminal.wire = io.BytesIO()
        terminal.connect()
        self.assertEqual(terminal.wire.getvalue(), wire)
        self.assertEqual(sent, [b"\xff\xfd\x01\xff\xfd\x03\xff\xfb\x03"])

    def test_shared_seed_requires_numeric_bases_not_just_scope_label(self):
        reply = ("lookup devseed\r\ndevseed =set 1 disc d.860930 0 0 0 0 ; system\r\n"
                 " ; 1019 47 3 0 100\r\n\x07")
        verify_project_seed(reply)
        for bad in (reply.replace("3 0 100", "3 0 0"), reply.replace("3 0 100", "3 100 100"),
                    reply.replace("set 1", "set 36"), reply + "***error"):
            with self.assertRaises(AssertionError):
                verify_project_seed(bad)

    def test_changed_demo_resources_slots_terminal_or_padding_are_rejected(self):
        for offset in (0, 14, 20, 30, 759, 768, 768 + 129, 1314, 1536 + 21,
                       1536 + 33, 1536 + 129, 2082, 2121):
            data = self.fixture()
            data[offset] ^= 1
            with self.assertRaises(AssertionError):
                verify_catalog(data, "soscat =set 3 disc d.860930 ; temp\r\n")

    def test_wrong_length_or_allocation_is_rejected(self):
        with self.assertRaises(AssertionError):
            verify_catalog(self.fixture()[:-1], "soscat =set 3 disc d.860930 ; temp\r\n")
        with self.assertRaises(AssertionError):
            verify_catalog(self.fixture(), "soscat =set 2 disc d.860930 ; temp\r\n")

    def test_recipe_adds_account_without_enlarging_demo(self):
        text = catalog_text()
        self.assertEqual(text.count("proc test\nmins 20000\nmaxs 30000"), 1)
        self.assertEqual(text.count("proc develop\nmins 100000\nmaxs 150000"), 1)
        self.assertIn("key0 6 30 key1 2 6 key2 2 6 key3 2 6", text)
        self.assertIn("key0 20 500 key1 4 100 key2 4 100 key3 4 100", text)
        self.assertEqual(text.count('term console1 "1" "test" 60 2'), 1)

    def test_nonstar_transfer_failure_is_not_success(self):
        verify_fp_reply("copy devseed\r\nend 10\r\n\x07")
        for reply in ("reserve host-file process result 2", "host-file process not found 0",
                      "***scope user devprog no resources", "syntax error"):
            with self.assertRaises(AssertionError):
                verify_fp_reply(reply)

    def test_explicit_host_inclusion_requires_own_normal_completion(self):
        valid = "include 63\r\n08.00 sos: ready\r\n"
        verify_host_inclusion(valid)
        for bad in (valid.replace("63", "7"), valid.replace("ready", "include not ok"),
                    "08.00 sos: ready\r\n", valid + "***error\r\n", valid + valid):
            with self.assertRaises(AssertionError):
                verify_host_inclusion(bad)

    def test_child_identity_size_bounds_and_parent(self):
        listing = ("tem 45000 11000 1 0 wait s\r\nprimo 56000 16938 2 0 wait s\r\n"
                   "sos 73000 300000 5 0 wait s\r\ndevelop 78000 150016 3 0 stop sos\r\n")
        verify_develop_listing(listing)
        for bad in (listing.replace("150016", "30208"), listing.replace("stop sos", "stop s"),
                    listing.replace("78000", "300000"), listing.replace("3 0 stop", "3 1 stop"),
                    listing + "develop 78000 150016 3 0 stop sos\r\n"):
            with self.assertRaises(AssertionError):
                verify_develop_listing(bad)


if __name__ == "__main__":
    unittest.main()
