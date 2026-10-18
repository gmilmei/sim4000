#!/usr/bin/env python3
"""Check integrated blank-disk creation and refusal to overwrite any path."""

from pathlib import Path
import subprocess
import sys
import tempfile

from test_operator_cli import Session


def main():
    executable = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="sim4000-mkdisc-") as temporary:
        work = Path(temporary)

        def invoke(*args, success=False):
            result = subprocess.run([str(executable), *args], cwd=work,
                                    capture_output=True, text=True, timeout=10)
            assert (result.returncode == 0) == success, (args, result)
            return result

        invoke("--mkdisc", "UNIT1", success=True)
        assert (work / "UNIT1").read_bytes() == bytes(180 * 768)
        invoke("--mkdisc=UNIT2", success=True)
        assert (work / "UNIT2").read_bytes() == bytes(180 * 768)
        (work / "keep").write_bytes(b"preserve")
        (work / "link").symlink_to("keep")
        (work / "dangling").symlink_to("absent")
        for name in ("UNIT1", "keep", "link", "dangling", "."):
            invoke("--mkdisc", name)
        assert (work / "keep").read_bytes() == b"preserve"
        assert (work / "UNIT1").read_bytes() == bytes(180 * 768)
        assert (work / "link").is_symlink() and (work / "dangling").is_symlink()
        assert not (work / "absent").exists()
        for args in (("--mkdisc",), ("--mkdisc", ""),
                     ("--mkdisc", "unused", "extra"),
                     ("--mkdisc", "unused", "--run"),
                     ("-i", "--mkdisc", "unused"),
                     ("--mkdisc", "unused", "-c", "missing.cfg"),
                     ("--mkdisc", "unused", "--ix"),
                     ("--mkdisc", "unused", "--steps", "1"),
                     ("--mkdisc", "unused", "--mkdisc", "other"),
                     ("--mkdisc", "missing/UNIT1")):
            invoke(*args)
        assert not (work / "unused").exists() and not (work / "other").exists()
        invoke("--mkdisc", "not-created", "--help", success=True)
        assert not (work / "not-created").exists()
        (work / "RC4000.cfg").write_text(
            f"MONITOR FILE={root / 'src/monitor/MONITOR'}\nCORESIZE=0,1048576\n", encoding="ascii")
        session = Session(executable, work)
        try:
            assert "created INTERACTIVE" in session.command(b"mkd\t INTERACTIVE\n")
            assert (work / "INTERACTIVE").read_bytes() == bytes(180 * 768)
            # Errors should be visible in the operator interface too.
            assert "usage: mkdisc FILE" in session.command(b"mkdisc\n")
            assert "cannot create" in session.command(b"mkdisc INTERACTIVE\n")
            assert "usage: mkdisc FILE" in session.command(b"mkdisc unused extra\n")
            session.finish()
        finally:
            session.close()
    print("Integrated mkdisc checks passed")


if __name__ == "__main__":
    main()
