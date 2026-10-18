#!/usr/bin/env python3
"""Pure endpoint-evidence guards for the opt-in idle SOS reconnect probe."""

from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dev/python"))
from probe_mips_reconnect import verify_endpoint, verify_completed_partial


class ReconnectTests(unittest.TestCase):
    def test_manual_partial_line_recipe_matches_live_inputs(self):
        text = (ROOT / "doc/usermanual/19-mips-ts.tex").read_text(encoding="utf-8")
        blocks = re.findall(r"% mips-reconnect: ([a-z-]+)\n\\begin\{commandexample\}\{[^\n]*\}\n(.*?)\\end\{commandexample\}", text, re.S)
        self.assertEqual(len(blocks), 3)
        self.assertEqual(dict(blocks), {"prefix": "message par\n", "suffix": "tial\n",
                                        "correction-prefix": "message wrong\n"})
        self.assertIn("press Ctrl-H exactly five times", text)
        self.assertIn("then type \\texttt{right} and Enter", text)

    def fixture(self):
        return ("device=4 type=CONSOLE listen=127.0.0.1:4004 state=connected "
                "input-buffer=0 output-buffer=0 attention=CTRL-A attention-requests=2 "
                "accepted=2 rejected=0 dropped-input=0 dropped-output=0\n")

    def test_expected_connection_and_listening_counts(self):
        verify_endpoint(self.fixture(), "connected", 2)
        verify_endpoint(self.fixture().replace("connected", "listening"), "listening", 2)

    def test_wrapped_cli_fields(self):
        text = self.fixture()
        verify_endpoint("\n".join(text[i:i + 80] for i in range(0, len(text), 80)), "connected", 2)

    def test_partial_line_requires_exact_completed_message(self):
        verify_completed_partial("tial\r\n partial\r\n\x07", "partial")
        verify_completed_partial("\b \b" * 5 + "right\r\n right\r\n\x07", "right")
        for wrong in (" partial\r\n partial\r\n", " partial\r\n***error",
                      " wrong\r\n", "from tem\r\n partial\r\n", " partial\r\nnew enrolled"):
            with self.subTest(reply=wrong), self.assertRaises(AssertionError):
                verify_completed_partial(wrong, "partial")

    def test_wrong_endpoint_state_counts_or_drops_fail(self):
        for old, new in (("device=4", "device=5"), ("127.0.0.1", "0.0.0.0"),
                         ("connected", "listening"), ("accepted=2", "accepted=1"),
                         ("rejected=0", "rejected=1"), ("dropped-input=0", "dropped-input=1"),
                         ("dropped-output=0", "dropped-output=1")):
            with self.subTest(field=old), self.assertRaises(AssertionError):
                verify_endpoint(self.fixture().replace(old, new), "connected", 2)
        with self.assertRaises(AssertionError):
            verify_endpoint(self.fixture() + self.fixture(), "connected", 2)


if __name__ == "__main__":
    unittest.main()
