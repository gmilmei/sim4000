#!/usr/bin/env python3
"""Opt-in: retain files across restarts with separate compatibility/protected overlays."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from test_operator_cli import Session

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def descriptor(output, name, scope):
    matches = re.findall(rf"(?m)^{re.escape(name)}[ \t]*=set[ \t]+([^\n;]+);[ \t]*(\w+)[ \t]*\n"
                         r"[ \t]*;[ \t]*(\d+)[ \t]+(\d+)[ \t]+(\d+)[ \t]+(-?\d+)[ \t]+(-?\d+)[ \t]*$",
                         output.replace("\r", ""))
    assert len(matches) == 1, output
    fields, found_scope, *details = matches[0]
    fields = fields.split()
    assert scope in ("temp", "user") and found_scope == scope and len(fields) >= 2, output
    assert int(fields[0]) > 0 and fields[1] == "disc", output
    details = list(map(int, details))
    assert details[2] == (3 if scope == "user" else 0), output
    assert details[3:] == [8388605, 8388605], output
    return {"tail": fields, "scope": found_scope, "details": details}


def verify_exports(exchange, text, binary):
    for name in ("beforetext", "aftertext"):
        assert (exchange / name).read_bytes() == text, name
    before = (exchange / "beforebin").read_bytes()
    assert before[:len(binary)] == binary and len(before) % 768 == 0
    assert (exchange / "afterbin").read_bytes() == before
    assert (exchange / "input").read_bytes() == text
    assert (exchange / "bininput").read_bytes() == binary


def run(simulator, directory):
    copies = {
        "sim4000": simulator,
        "MONITOR": ROOT / "src/monitor/MONITOR",
        "UNIT0": ROOT / "src/disks/UNIT0",
        "MONITOR.protected": ROOT / "src/monitor/MONITOR.protected",
        "UNIT0.protected": ROOT / "src/disks/UNIT0.protected",
    }
    source_files = {*copies.values(), ROOT / "src/disks/UNIT0.original",
                    ROOT / "src/config/RC4000.cfg", ROOT / "src/config/RC4000.protected.cfg"}
    source_hashes = {path: digest(path) for path in source_files}
    directory.mkdir()  # Never reuse personal disks or a prior test workspace.
    runtime = directory / "runtime"
    runtime.mkdir()
    for name, source in copies.items():
        shutil.copy2(source, runtime / name)
    profiles = {}
    for profile in ("compat", "protected"):
        suffix = ".protected" if profile == "protected" else ""
        cfg_source = ROOT / ("src/config/RC4000.protected.cfg" if suffix else "src/config/RC4000.cfg")
        declaration = f"DEFINE RC433X{' ' if suffix else '  '}DEVNO=15 FILE=UNIT0{suffix}\n"
        config = cfg_source.read_text(encoding="ascii")
        assert config.count(declaration) == 1
        config = config.replace(declaration, declaration.rstrip("\n") +
                                f" OVERLAY=UNIT0{suffix}.overlay\n")
        config = config.replace("ROOT=exchange\n", f"ROOT=exchange.{profile}\n")
        config = config.replace("THROTTLE VALUE=50%\n", "THROTTLE VALUE=OFF\n")
        filename = f"RC4000.{profile}.cfg"
        (runtime / filename).write_text(config, encoding="ascii")
        exchange = runtime / f"exchange.{profile}"
        exchange.mkdir()
        text = f"Retained {profile} text.\nSecond line.\n".encode("ascii")
        binary = bytes(range(256) if profile == "compat" else reversed(range(256))) * 3
        (exchange / "input").write_bytes(text)
        (exchange / "bininput").write_bytes(binary)
        profiles[profile] = (filename, exchange, text, binary, runtime / f"UNIT0{suffix}.overlay")
    unchanged = {runtime / name: digest(runtime / name) for name in copies}
    unchanged.update({runtime / cfg: digest(runtime / cfg) for cfg, *_ in profiles.values()})
    reports = {}
    try:
        for profile, (cfg, exchange, text, binary, overlay) in profiles.items():
            before_metadata = None
            for phase in ("write", "restart"):
                session = None
                with (directory / f"{profile}-{phase}.log").open("x", encoding="ascii") as log:
                    def record(output):
                        log.write(output)
                        log.flush()
                        return output

                    def guest(line, expected=None):
                        os.write(session.master, line.encode("ascii") + b"\r")
                        output = record(session.until(b"\x07"))
                        assert "***" not in output and " break fp" not in output, output
                        if expected is not None:
                            assert expected in output, output
                        return output

                    try:
                        # The restarted process uses a different host CWD;
                        # all copied configuration paths must still resolve.
                        cwd = runtime if phase == "write" else directory
                        path = cfg if phase == "write" else f"runtime/{cfg}"
                        session = Session(runtime / "sim4000", cwd, ("-a", "-c", path),
                                          startup=b"initialize date:")
                        record("initialize date:\n")
                        os.write(session.master, b"86 09 30 08 00 00\r")
                        record(session.until(b"pause startproc finis fp"))
                        os.write(session.master, b"\x1b")
                        record(session.until(b"att") + session.until(b"\x07"))
                        guest("s")
                        guest("new keeper size 100000 temp disc 200 10 perm disc 200 10 run", "to keeper")
                        claim = guest("claim", "first core")
                        match = re.search(r"size\s+100000\s+first core\s+(\d+)", claim)
                        assert match, claim
                        first = int(match[1])
                        assert first >= 8 and first % 2 == 0 and first + 100000 <= 1048576
                        transfer_in, transfer_out = ("rcin", "rcout") if profile == "protected" else ("fromhost", "tohost")
                        temporary_metadata = {}
                        if phase == "write":
                            guest(f"keeptext={transfer_in} input", "bytes")
                            guest(f"keepbin={transfer_in} conv.bin bininput", "bytes")
                            for name in ("keeptext", "keepbin"):
                                temporary_metadata[name] = descriptor(guest("lookup " + name), name, "temp")
                                guest("scope user " + name)
                        metadata = {name: descriptor(guest("lookup " + name), name, "user")
                                    for name in ("keeptext", "keepbin")}
                        if phase == "write":
                            for name, temporary in temporary_metadata.items():
                                expected = {**temporary, "scope": "user",
                                            "details": [*temporary["details"][:2], 3, 8388605, 8388605]}
                                assert metadata[name] == expected, (metadata[name], expected)
                            before_metadata = metadata
                        else:
                            assert metadata == before_metadata, (metadata, before_metadata)
                        reports[f"{profile}-{phase}"] = metadata
                        target = "before" if phase == "write" else "after"
                        guest(f"{target}text={transfer_out} keeptext", "bytes")
                        guest(f"{target}bin={transfer_out} conv.bin keepbin", "bytes")
                        os.write(session.master, b"\x05")
                        record(session.until(b"\nsim4000> "))
                        cpu = record(session.command(b"show cpu\n"))
                        assert f"protection={'rc4000' if profile == 'protected' else 'off'} " in cpu and "ix=on" in cpu, cpu
                        media_output = record(session.command(b"show media\n"))
                        # Operator output wraps at 80 columns, including within
                        # this token when the profile's relative path is longer.
                        assert "mode=persistent-overlay" in media_output.replace("\r", "").replace("\n", ""), media_output
                        if profile == "protected":
                            for address in (first, first + 50000, first + 99998):
                                keys = record(session.command(f"show keys {address}\n".encode("ascii")))
                                assert f"{address:06x}: key=1 " in keys, keys
                        os.write(session.master, b"continue\n")
                        os.write(session.master, b"finis\r")
                        record(session.until(b"pause keeper finis fp"))
                        os.write(session.master, b"\x05")
                        record(session.until(b"\nsim4000> "))
                        session.finish()
                    finally:
                        if session is not None:
                            if session.pending:
                                record(session.pending.decode("ascii", errors="backslashreplace"))
                            session.close()
                assert overlay.stat().st_size > 64
                assert {p: digest(p) for p in unchanged} == unchanged, "copied base/config changed"
            verify_exports(exchange, text, binary)

        overlays = {profile: data[4] for profile, data in profiles.items()}
        overlay_hashes = {p: digest(p) for p in overlays.values()}
        assert len(set(overlay_hashes.values())) == 2
        for profile, (cfg, _, _, _, _) in profiles.items():
            other = overlays["compat" if profile == "protected" else "protected"]
            result = subprocess.run(
                [str(runtime / "sim4000"), "--run", "-c", str(runtime / cfg),
                 "--disk-overlay", str(other), "--steps", "1"], input=b"",
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=directory, timeout=10)
            (directory / f"{profile}-wrong-overlay.log").write_bytes(result.stdout)
            assert result.returncode != 0 and b"overlay does not match the base image" in result.stdout, result.stdout
            assert {p: digest(p) for p in overlays.values()} == overlay_hashes, "rejected overlay changed"
        assert {p: digest(p) for p in unchanged} == unchanged
        assert {p: digest(p) for p in source_files} == source_hashes
        (directory / "metadata.json").write_text(json.dumps(reports, indent=2, sort_keys=True) + "\n", encoding="ascii")
        (directory / "result.txt").write_text(
            "Both profiles retain text/binary files and user-scope descriptors across clean restarts.\n"
            "Distinct overlays reject cross-profile reuse; source and copied bases remain unchanged.\n",
            encoding="ascii")
    finally:
        assert {p: digest(p) for p in unchanged} == unchanged, "copied base/config changed"
        assert {p: digest(p) for p in source_files} == source_hashes, "source media changed"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("simulator", type=Path)
    parser.add_argument("destination", type=Path, help="new exclusive workspace; must not exist")
    args = parser.parse_args()
    run(args.simulator.resolve(), args.destination.resolve())
    print("Compatibility/protected restart persistence and overlay separation passed")


if __name__ == "__main__":
    main()
