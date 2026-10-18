#!/usr/bin/env python3
"""Check shipped printer configuration, live visibility and restart append."""
import hashlib
import os
from pathlib import Path
import re
import sys
import tempfile

from test_operator_cli import Session


def main():
    simulator = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[2]
    media = [root / "src/monitor" / name for name in ("MONITOR", "MONITOR.protected", "STDMON")]
    media += [root / "src/disks" / name for name in ("UNIT0", "UNIT0.protected")]
    before = {p: hashlib.sha256(p.read_bytes()).digest() for p in media}
    for profile in ("RC4000.cfg", "RC4000.protected.cfg", "RC4000.dfc403.cfg"):
        with tempfile.TemporaryDirectory(prefix="sim4000-printer-") as temporary:
            work = Path(temporary)
            (work / "exchange").mkdir()
            text = (root / "src/config" / profile).read_text(encoding="ascii")
            text = re.sub(r"(?m)^MONITOR FILE=(\S+)",
                          lambda m: "MONITOR FILE=" + str(root / "src/monitor" / m[1]), text)
            text = re.sub(r"(?m)^(DEFINE (?:RC433X|RC4819) .*FILE=)(\S+)",
                          lambda m: m[1] + str(root / "src/disks" / m[2]), text)
            text = text.replace("THROTTLE VALUE=50%", "THROTTLE VALUE=OFF")
            (work / "RC4000.cfg").write_text(text, encoding="ascii")
            output = work / re.search(r"(?m)^DEFINE RC610 .*FILE=(\S+) APPEND=YES$", text)[1]
            prefix = b"Existing printer output.\n"
            output.write_bytes(prefix)
            for restart in range(2):
                session = Session(simulator, work, ("-a",), startup=b"initialize date:")
                try:
                    assert output.read_bytes() == prefix
                    os.write(session.master, b"86 09 30 08 00 00\r")
                    session.until(b"pause startproc finis fp")
                    os.write(session.master, b"\x1b")
                    session.until(b"att")
                    session.until(b"\x07")
                    os.write(session.master, b"s\r")
                    session.until(b"\x07")
                    os.write(session.master, b"print 16 16\r")
                    session.until(b"ready")
                    printed = output.read_bytes()
                    assert printed.startswith(prefix) and len(printed) > len(prefix)
                    assert printed[len(prefix):].endswith(b"\n")
                    os.write(session.master, b"\x05")
                    session.until(b"\nsim4000> ")
                    status = session.command(b"show media\n")
                    assert "type=RC610 output=open status=000000" in status, status
                    assert "prints=1 failed=0" in status, status
                    assert "output detached" in session.command(b"detach 7\n")
                    assert "cannot attach" in session.command(f"attach 7 {output.name}\n".encode())
                    assert output.read_bytes() == printed
                    assert "output attached" in session.command(f"attach 7 {output.name} append\n".encode())
                    session.command(b"reset\n")
                    assert output.read_bytes() == printed
                    session.finish()
                    prefix = printed
                finally:
                    session.close()
            print(f"{profile}: automatic output, live visibility, reset and restart append passed")
    assert before == {p: hashlib.sha256(p.read_bytes()).digest() for p in media}


if __name__ == "__main__":
    main()
