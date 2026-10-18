#!/usr/bin/env python3
"""Verify standard monitor export, DFC403 boot, transfers, and safe publication."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main() -> None:
    builder = Path(sys.argv[1]).resolve()
    simulator = Path(sys.argv[2]).resolve()
    root = Path(__file__).resolve().parents[2]
    tool = root / "dev/python" / "make_standard_monitor.py"
    environment = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
    with tempfile.TemporaryDirectory(prefix="sim4000-standard-test-") as temporary:
        directory = Path(temporary)
        monitor = directory / "STDMON"
        args = [sys.executable, str(tool), str(monitor), "--builder", str(builder)]
        subprocess.run(args, check=True, env=environment, timeout=120)
        data = monitor.read_bytes()
        assert len(data) == 38400
        result = subprocess.run(args, capture_output=True, env=environment, timeout=10)
        assert result.returncode != 0
        assert b"destination already exists" in result.stderr
        assert monitor.read_bytes() == data
        assert not list(directory.glob(".sim4000-standard-*"))
        # Exercise the real runtime wiring and optional configuration as well.
        config = directory / "RC4000.dfc403.cfg"
        text = (root / "src/config/RC4000.dfc403.cfg").read_text()
        (directory / "exchange").mkdir()
        config.write_text(text, encoding="ascii")
        args = [str(simulator), "--run", "--steps", "4000000",
                "--set-disc-load-flag", "-c", str(config), "--monitor",
                str(monitor), "--disk", str(root / "src/disks/UNIT0")]
        result = subprocess.run(args, input="", capture_output=True,
                                text=True, env=environment, timeout=60)
        assert result.returncode == 0, result.stderr
        assert "initialize date:" in result.stdout, result.stdout
        subprocess.run([sys.executable, str(root / "tests/python/test_catstat.py"),
                        str(builder), str(monitor)], check=True,
                       env=environment, timeout=90)
        subprocess.run([sys.executable, str(root / "tests/python/test_catstat_ix.py"),
                        str(builder), str(simulator), str(monitor)], check=True,
                       env=environment, timeout=90)
        subprocess.run([sys.executable, str(root / "tests/python/test_montest_help.py"),
                        str(simulator), str(monitor)], check=True,
                       env=environment, timeout=90)
        # Two manual variants, each bounded to 300 seconds.
        subprocess.run([sys.executable, str(root / "tests/python/test_monitor_manual.py"),
                        str(builder), str(monitor)], check=True,
                       env=environment, timeout=660)
    print("Standard monitor export, DFC403 boot, host transfers, and runtime wiring passed")


if __name__ == "__main__":
    main()
