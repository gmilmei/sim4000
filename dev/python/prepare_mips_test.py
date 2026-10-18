#!/usr/bin/env python3
"""Prepare an exclusive, disposable MIPS/TS runtime; never a release profile."""

import argparse
import json
import os
from pathlib import Path
import subprocess

from make_unit0_copy import CopyError
from probe_mips_ts import (ROOT, TERMINAL_INPUT, ACCOUNT_RESOURCE_CLAUSE, digest,
                           inspect, private_sos_start, read_system_reply,
                           verify_second_disc, verify_terminal_catalog,
                           verify_component_listing, verify_sos_startup)
from test_operator_cli import Session
from mips_terminal import run_terminal

KEEP_TEXT = b"Persistent MIPS/TS test profile.\n"


def instructions(port):
    return f"""# Disposable MIPS/TS test environment

This is an experimental compatibility-monitor environment, not a release
profile. Protection is off. It does not replace the standard configuration.
The guest starts at the fixed test date 30 September 1986; no date input is
needed. The dummy account and password are both `test`.

From this directory, run the simulator using its absolute path:

```sh
/path/to/sim4000 -a -c RC4000.mipsts.cfg
```

Wait for `sos started`. In a second terminal:

```sh
telnet 127.0.0.1 {port}
```

Press Ctrl-A, then enter `sos`. At the SOS prompt enter:

```text
run test pass test
```

This uses TEM to open an FP session. Try `message hello` or `lookup fp`.
Enter `finis` to log out before disconnecting. To stop the emulator, press
Ctrl-E in its original terminal and enter `quit` at `sim4000>`.

Only loopback connections are accepted. Telnet and these dummy credentials
are not suitable for network exposure. Only one test login is provisioned.

Keep MONITOR, UNIT0, UNIT1, RC4000.mipsts.cfg and the exchange directory
together. Python is used only to prepare and verify this test environment;
it is not needed to run it. Guest writes persist in UNIT0.overlay and
UNIT1.overlay, not in the base images. Keep both overlays when resuming.
With the emulator stopped, removing both overlays together resets the guest
to the prepared state and discards all subsequent guest changes.

The private startarea mounts device 16, sets the test date, and directly
reads starttem, startprimo and startsos. The monitor allows only two nested
input files, so startarea must not read startmips, which reads them again.
The private startsos reserves its normal 2500 segments/200 entries on disc1.

verification/result.json, when present, records two completed boots,
wrong-password rejection, TEM login/FP commands/logout and byte-for-byte
readback of an administrative user-scope file after restart. This is bounded
test evidence, not general MIPS/TS or protected-mode certification.
"""


def configuration(port, preparation=False):
    storage = "WRITABLE=YES" if preparation else "OVERLAY=UNIT0.overlay"
    second = "WRITABLE=YES" if preparation else "OVERLAY=UNIT1.overlay"
    network = "" if preparation else (
        "DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
        f"TELNET DEVNO=4 ADDRESS=127.0.0.1 PORT={port} ATTENTION=CTRL-A\n")
    return (
        "; Disposable MIPS/TS test environment. Not a release configuration.\n"
        "; Dummy account: test / test. Bind only to loopback.\n"
        "PROTECTION MODE=OFF\nCPU IX=YES\nMONITOR FILE=MONITOR\n"
        "CORESIZE=0,1048576\nOPERATOR ESCAPE=CTRL-E\nOPERATOR DATE=ALT-D\n"
        "INPUT DEL=CTRL-H\nDATE YEAROFFSET=-40\nTHROTTLE VALUE=50%\n"
        "DEFINE CONSOLE DEVNO=2 INTNO=6,3\nDEFINE CLOCK DEVNO=3 INTNO=11\n"
        "DEFINE RC610 DEVNO=7 INTNO=10 FILE=printer.mipsts.txt APPEND=YES\n"
        f"DEFINE RC433X DEVNO=15 FILE=UNIT0 {storage}\n"
        f"DEFINE RC433X DEVNO=16 FILE=UNIT1 {second}\n"
        "DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT=exchange\n" + network)


def autostart_text(original):
    if original.count(b"startdate c") != 1 or original.count(b"* unstack") != 1:
        raise CopyError("unexpected original startarea; inspect exchange/oldboot")
    # Use a fixed test date instead of the interactive date process. The
    # monitor permits only two nested reads: startarea plus one component.
    # Reading startmips here would need a third level for starttem, etc.
    return (b"* kit 16\n* date 86 09 30 08 00 00\n"
            b"* read starttem\n* read startprimo\n* read startsos\n* unstack\n")


def prepare(builder, simulator, destination, port):
    if not 1024 <= port <= 65535:
        raise CopyError("test Telnet port must be between 1024 and 65535")
    media = (ROOT / "src/disks/UNIT0", ROOT / "src/monitor/MONITOR")
    before = {p: digest(p) for p in media}
    inspect(builder, destination)
    result = subprocess.run([str(simulator), "--mkdisc", "UNIT1"], cwd=destination,
                            capture_output=True, text=True, timeout=10)
    if result.returncode != 0:
        raise CopyError(result.stderr)
    config = destination / "RC4000.cfg"
    config.write_text(configuration(port, True), encoding="ascii")
    session = None
    with (destination / "prepare.log").open("x", encoding="ascii") as log:
        def record(text):
            log.write(text)
            log.flush()
            return text

        def system(line):
            os.write(session.master, b"\x1b")
            record(session.until(b"att") + session.until(b"\x07"))
            os.write(session.master, b"s\r")
            record(session.until(b"\x07"))
            os.write(session.master, line.encode("ascii") + b"\r")
            return read_system_reply(session, record)

        def fp(line):
            os.write(session.master, line.encode("ascii") + b"\r")
            reply = record(session.until(b"\x07", timeout=30))
            if any(word in reply for word in ("***", "giveup", "hard error", "abnormal end", "syntax error")):
                raise CopyError("preparation FP command failed: " + line)
            return reply

        try:
            session = Session(simulator, destination, ("-a",), startup=b"initialize date:")
            os.write(session.master, b"86 09 30 08 00 00\r")
            record(session.until(b"pause startproc finis fp"))
            system("all mipsprep size 150000 run")
            record(session.until(b"\x07"))
            label = fp("kitlabel 16 disc1 catdisc1 slow 36 9 2030")
            mounted = fp("kiton devno.16")
            verify_second_disc(label, mounted, fp("show"))
            exchange = destination / "exchange"
            fp("oldboot=tohost startarea")
            original = (exchange / "oldboot").read_bytes()
            (exchange / "mipsboot").write_bytes(autostart_text(original))
            (exchange / "sosboot").write_bytes(private_sos_start(
                (exchange / "startsos").read_bytes(), 2500, 200, "disc1"))
            catalog = TERMINAL_INPUT.replace('\npass "test"', '\npass "test"' +
                ACCOUNT_RESOURCE_CLAUSE.replace("key1 0 0 key2 0 0 key3 0 0", "key1 2 6 key2 2 6 key3 2 6"))
            (exchange / "catinput").write_text(catalog + "\n", encoding="ascii")
            fp("catspec=fromhost catinput")
            response = fp("soscat=upsoscat catspec")
            descriptor = fp("lookup soscat")
            fp("catalogbin=tohost conv.bin soscat")
            verify_terminal_catalog(response, descriptor, (exchange / "catalogbin").read_bytes(), 30, True)
            fp("scope user soscat")
            for source, target in (("sosboot", "startsos"), ("mipsboot", "startarea")):
                fp(f"boottemp=fromhost {source}")
                fp(f"{target}=copy boottemp")
                fp(f"scope user {target}")
                fp("clear temp boottemp")
            os.write(session.master, b"finis\r")
            record(session.until(b"pause mipsprep finis fp"))
            os.write(session.master, b"\x05")
            record(session.until(b"\nsim4000> "))
            session.finish()
        finally:
            if session is not None:
                session.close()
            if {p: digest(p) for p in media} != before:
                raise CopyError("shipped media changed during test preparation")
    config.unlink()
    (destination / "RC4000.mipsts.cfg").write_text(configuration(port), encoding="ascii")
    (destination / "README.md").write_text(instructions(port), encoding="ascii")
    (destination / "test-profile.json").write_text(json.dumps({
        "test_only": True, "port": port, "protection": "off", "dummy_account": "test",
        "source_sha256": {p.name: value for p, value in before.items()},
        "prepared_sha256": {name: digest(destination / name) for name in ("MONITOR", "UNIT0", "UNIT1")},
    }, indent=2) + "\n", encoding="ascii")


def verify_profile(simulator, destination, port, primary="UNIT0", secondary="UNIT1"):
    bases = {name: digest(destination / name) for name in ("MONITOR", primary, secondary)}
    for boot in (1, 2):
        evidence = destination / "verification" / f"boot{boot}"
        evidence.mkdir(parents=True)
        session = None
        with (evidence / "boot.log").open("x", encoding="ascii") as log:
            def record(text):
                log.write(text)
                log.flush()
                return text

            def system(line):
                os.write(session.master, b"\x1b")
                record(session.until(b"att") + session.until(b"\x07"))
                os.write(session.master, b"s\r")
                record(session.until(b"\x07"))
                os.write(session.master, line.encode("ascii") + b"\r")
                return read_system_reply(session, record)

            def fp(line):
                os.write(session.master, line.encode("ascii") + b"\r")
                reply = record(session.until(b"\x07", timeout=30))
                if "***" in reply or "giveup" in reply:
                    raise CopyError("restart FP check failed: " + line)
                return reply

            try:
                session = Session(simulator, destination,
                    ("-a", "-c", "RC4000.mipsts.cfg"), startup=b"from s")
                record("from s")
                record(session.until(b"message sos started\r\n", timeout=20))
                listing = system("list")
                verify_sos_startup((evidence / "boot.log").read_text(encoding="ascii"), listing)
                system("all verify base 0 0 login 0 0 user 0 0 project 0 0 run")
                record(session.until(b"\x07"))
                if boot == 1:
                    (destination / "exchange/keepinput").write_bytes(KEEP_TEXT)
                    for command in ("keeptmp=fromhost keepinput", "keepsake=copy keeptmp", "scope user keepsake"):
                        fp(command)
                fp("lookup keepsake")
                fp(f"keepcheck{boot}=tohost keepsake")
                if (destination / f"exchange/keepcheck{boot}").read_bytes() != KEEP_TEXT:
                    raise CopyError("persistent test file differs after boot")
                os.write(session.master, b"finis\r")
                record(session.until(b"pause verify finis fp"))
                run_terminal(port, "run", evidence, system,
                    lambda output: verify_component_listing(output, ("tem", "primo", "sos")))
                os.write(session.master, b"\x05")
                record(session.until(b"\nsim4000> "))
                session.finish()
            finally:
                if session is not None:
                    if session.pending:
                        record(session.pending.decode("ascii", errors="backslashreplace"))
                    session.close()
    if {name: digest(destination / name) for name in bases} != bases:
        raise CopyError("prepared base media changed during overlay-only verification")
    (destination / "verification/result.json").write_text(json.dumps({
        "boots_verified": 2, "automatic_subsystem_start": True,
        "tem_login_logout_verified": True, "user_file_survives_restart": True,
        "prepared_bases_unchanged": True, "port": port,
    }, indent=2) + "\n", encoding="ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--port", type=int, default=4004)
    parser.add_argument("--simulator", type=Path, default=ROOT / "build/sim4000")
    parser.add_argument("--builder", type=Path, default=ROOT / "build-protection/sim4000_build_host_image")
    parser.add_argument("--verify", action="store_true", help="verify two full boots, TEM login and persistent file contents")
    args = parser.parse_args()
    prepare(args.builder.resolve(), args.simulator.resolve(), args.destination.resolve(), args.port)
    if args.verify:
        verify_profile(args.simulator.resolve(), args.destination.resolve(), args.port)
    print("Disposable MIPS/TS test profile prepared" + (" and restart-verified." if args.verify else "; restart verification still required."))


if __name__ == "__main__":
    main()
