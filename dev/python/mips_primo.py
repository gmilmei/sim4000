"""Fixed PRIMO capability and transfer evidence guards for private probes."""

import re

from make_unit0_copy import CopyError

PRIMO_TEXT = b"PRIMO private transport probe.\n"
DEVICE_COMMANDS = (
    "display", "signup console1 14", "signup console2 14", "signup console3 14",
    "signup console4 14", "display", "signoff console1", "signoff console2",
    "signoff console3", "signoff console4", "signup punch 12", "display",
    "signoff punch", "signup reader 10", "display", "signoff reader",
    "signup console2 8", "display", "signoff console2", "display")


def verify_primo_transfer(output):
    definitions = re.findall(r"(?m)^primo define 0 reply 0 transport (\d+)\r?$", output)
    waits = re.findall(r"(?m)^primo wait 0 reply 0 state 5 cause -1\r?$", output)
    if not output.startswith("prbprog\r\n") or "***" in output or "hard error" in output or \
            len(definitions) != 1 or int(definitions[0]) <= 0 or len(waits) != 1 or \
            len(re.findall(r"(?m)^from primo\r?$", output)) != 1 or \
            len(re.findall(r"(?m)^PRIMO private transport probe\.\r?$", output)) != 1 or \
            len(re.findall(r"(?m)^end\s+\d+\s*\r?$", output)) != 1:
        raise CopyError("PRIMO transport lacks exact output, normal return codes or completed state")
    return int(definitions[0])


def verify_primo_devices(observations):
    if [item["command"] for item in observations] != list(DEVICE_COMMANDS):
        raise CopyError("PRIMO capability command sequence differs")
    for item in observations:
        command, reply = item["command"], item["reply"]
        expected = "***no resources" if command in (
            "signup punch 12", "signup reader 10", "signup console2 8") else \
            "***device unknown" if command in (
                "signup console3 14", "signup console4 14", "signoff console3",
                "signoff console4", "signoff punch", "signoff reader") else "ready"
        endings = re.findall(r"(?m)^\d{2}\.\d{2} primo:([^\r\n]+)\r?$", reply)
        if not reply.startswith(command + "\r\n") or endings != [expected]:
            raise CopyError("PRIMO capability reply differs: " + command)
    assigned = observations[5]["reply"]
    for name in ("console1", "console2"):
        if len(re.findall(rf"(?m)^{name}\s+[^\r\n]*,idle\s*\r?$", assigned)) != 1:
            raise CopyError("PRIMO did not display both available idle printer assignments")
    if len(re.findall(r"(?m)^console\d+\s", assigned)) != 2:
        raise CopyError("PRIMO displayed an unexpected additional assignment")
    if re.search(r"(?m)^console\d+\s", observations[-1]["reply"]):
        raise CopyError("PRIMO assignments remain after signoff")
