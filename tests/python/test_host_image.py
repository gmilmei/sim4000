#!/usr/bin/env python3
"""Verify rebuilding and rebooting the native host-utility disk image."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--builder", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    tool = root / "dev/python" / "make_host_image.py"
    environment = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
    with tempfile.TemporaryDirectory(prefix="sim4000-native-image-test-") as directory:
        image = Path(directory) / "UNIT0.host"
        subprocess.run([sys.executable, str(tool), str(image),
                        "--builder", str(args.builder)],
                       check=True, env=environment)
        before = image.read_bytes()
        assert before == (root / "src" / "disks" / "UNIT0").read_bytes()
        result = subprocess.run([sys.executable, str(tool), str(image),
                                 "--builder", str(args.builder)],
                                capture_output=True, env=environment)
        assert result.returncode != 0
        assert b"destination already exists" in result.stderr
        assert image.read_bytes() == before
        assert not list(Path(directory).glob(".sim4000-host-*"))
    print("native host-image build, reboot, catalog, transfers and non-overwrite passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
