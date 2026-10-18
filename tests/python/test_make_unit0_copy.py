#!/usr/bin/env python3
"""Integration checks for the verified UNIT0 working-copy tool."""

from __future__ import annotations

import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SOURCE_DIR = Path(__file__).resolve().parents[2]
TOOL = SOURCE_DIR / "dev/python" / "make_unit0_copy.py"
UNIT0 = SOURCE_DIR / "src" / "disks" / "UNIT0.original"
UNIT0_SHA256 = "838621f65fef164cf55fca6ad60ff4f1b3ae5b5326fd7f29a38bfc75a4a5b22a"


class WorkingCopyTest(unittest.TestCase):
    def run_tool(self, *arguments: object) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(TOOL), *(str(value) for value in arguments)],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_verified_copy_and_exclusive_destination(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sim4000-unit0-") as directory:
            destination = Path(directory) / "UNIT0.work"
            result = self.run_tool(destination, "--source", UNIT0)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(destination.stat().st_size, 4_658_688)
            self.assertEqual(
                hashlib.sha256(destination.read_bytes()).hexdigest(),
                UNIT0_SHA256,
            )
            with destination.open("r+b") as stream:
                stream.seek(0)
                stream.write(b"WORK")

            result = self.run_tool(destination, "--source", UNIT0)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(destination.read_bytes()[:4], b"WORK")

    def test_native_image_is_the_default_source(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sim4000-native-copy-") as directory:
            destination = Path(directory) / "UNIT0.work"
            result = self.run_tool(destination)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(destination.stat().st_size, 4_660_224)
            self.assertEqual(hashlib.sha256(destination.read_bytes()).hexdigest(),
                "e5d293bd01c39385218c11531412156ae4b65afa542c1263b2dd3c919f630a1e")

    def test_rejects_non_pristine_source_without_destination(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sim4000-unit0-") as directory:
            bad_source = Path(directory) / "bad.img"
            destination = Path(directory) / "UNIT0.work"
            bad_source.write_bytes(b"not UNIT0")

            result = self.run_tool(destination, "--source", bad_source)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(destination.exists())


if __name__ == "__main__":
    unittest.main()
