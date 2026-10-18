"""Bounded disposable SOS interactive-session probe over a second console."""

import json
import re

from make_unit0_copy import CopyError
from test_operator_acceptance import Remote


def verify_interactive_child(listing):
    """Identify the child waiting for terminal input, not a batch run state."""
    parents = re.findall(r"(?m)^sos\s+(\d+)\s+300000\s+5\s+0\s+(?:run|wait)\s+s\s*\r?$", listing)
    children = re.findall(r"(?m)^test\s+(\d+)\s+30208\s+3\s+0\s+(?:run|wait|stop)\s+sos\s*\r?$", listing)
    if len(parents) != 1 or len(children) != 1 or len(re.findall(r"(?m)^test\s+\d+", listing)) != 1 or \
            not 8 <= int(parents[0]) < int(children[0]) < int(parents[0]) + 300000:
        raise CopyError("interactive child identity, bounds or parent differs")


def verify_interactive_message(output):
    if not output.startswith("message termprobe\r\n") or "***" in output or \
            len(re.findall(r"(?m)^ termprobe\r?$", output)) != 1:
        raise CopyError("interactive FP message did not produce its expected output")


def verify_interactive_enrollment(output, mode):
    enrolled = re.findall(r"(?m)^\d{2}\.\d{2} sos: test enrolled\r?$", output)
    headers = re.findall(r"(?m)^from tem\r?$", output)
    if mode not in ("go", "run") or len(enrolled) != 1 or "***" in output or \
            len(headers) != (1 if mode == "run" else 0):
        raise CopyError("remote enrollment origin or TEM/direct header differs")


def verify_interactive_lookup(output):
    if not output.startswith("lookup fp\r\n") or "***" in output or \
            len(re.findall(r"(?m)^fp\s+=set 18 disc [^\r\n]+\r?$", output)) != 1:
        raise CopyError("interactive FP lookup did not identify the expected FP area")


def verify_interactive_logout(output):
    if not output.startswith("finis\r\n") or "***" in output or \
            len(re.findall(r"(?m)^\d{2}\.\d{2} sos: test removed after finis\r?$", output)) != 1:
        raise CopyError("interactive logout did not report normal SOS removal")


def run_terminal(port, mode, destination, system, verify_healthy):
    remote = Remote(port)
    with (destination / "terminal.log").open("x", encoding="ascii") as log:
        def until(marker):
            result = remote.until(marker).decode("ascii")
            log.write(result)
            log.flush()
            return result

        def select_sos():
            remote.send(b"\x01")
            until(b"att")
            until(b"\x07")
            remote.send(b"sos\r\n")
            if not until(b"\x07").endswith(">\x07"):
                raise CopyError("remote SOS selection did not reach its command prompt")

        def login(password):
            select_sos()
            remote.send(f"{mode} test pass {password}\r\n".encode("ascii"))

        try:
            negotiation = remote.until(b"\xff\xfd\x03")
            if b"\xff\xfb\x01" not in negotiation:
                raise CopyError("remote Telnet negotiation does not match the test endpoint")
            remote.send(b"\xff\xfd\x01\xff\xfd\x03\xff\xfb\x03")
            login("wrong")
            rejected = until(b"bad password\r\n")
            if not re.search(r"\d{2}\.\d{2} \*\*\*sos: bad password\r\n$", rejected):
                raise CopyError("remote password rejection has an unexpected form")
            verify_healthy(system("list"))
            login("test")
            enrolled = until(b"test enrolled\r\n")
            verify_interactive_enrollment(enrolled, mode)
            until(b"\x07")
            listing = system("list")
            (destination / "terminal-active.txt").write_text(listing, encoding="ascii")
            verify_healthy(listing)
            verify_interactive_child(listing)
            remote.send(b"message termprobe\r\n")
            output = until(b"\x07")
            verify_interactive_message(output)
            remote.send(b"lookup fp\r\n")
            lookup = until(b"\x07")
            verify_interactive_lookup(lookup)
            remote.send(b"finis\r\n")
            finished = until(b"test removed after finis\r\n")
            verify_interactive_logout(finished)
            listing = system("list")
            (destination / "terminal-finished.txt").write_text(listing, encoding="ascii")
            verify_healthy(listing)
            if re.search(r"(?m)^test\s+\d+", listing):
                raise CopyError("interactive child still exists after finis")
            (destination / "terminal.json").write_text(json.dumps({
                "mode": mode, "device": 4, "loopback_only": True,
                "wrong_password_rejected": True, "login_verified": True,
                "message_verified": True, "lookup_verified": True,
                "normal_logout_verified": True, "child_removed_verified": True,
                "tem_path_requested": mode == "run", "protection": "off",
                "tem_header_verified": mode == "run",
            }, indent=2) + "\n", encoding="ascii")
        finally:
            if remote.pending:
                log.write(remote.pending.decode("ascii", errors="backslashreplace"))
            remote.close()
