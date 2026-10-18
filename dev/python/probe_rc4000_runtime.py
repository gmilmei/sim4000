#!/usr/bin/env python3
"""Probe older transfer-library bindings on an exclusive disposable disk copy."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import sys

from make_host_image import BINARY, TEXT, verify_transfers
from make_unit0_copy import CopyError, create_copy, verify_source

ROOT = Path(__file__).resolve().parents[2]
PROBE = bytes.fromhex("730022 a3004f a3003b")


def labels(data: bytes) -> list[bytes]:
    return [data[i + 756:i + 768].rstrip(b"\0 ")
            for i in range(0, len(data), 768)]


def check_compiled(data: bytes) -> None:
    """Verify the older transfer module; do not mistake nsystem for RC4000 code."""
    names = labels(data)
    if b"tofrom" not in names or b"tofrom1" in names or b"tofrom2" in names:
        raise CopyError("compiled transfer does not select the single older tofrom module")
    hits = [i for i in range(0, len(data), 3) if data.startswith(PROBE, i)]
    if not hits or any(names[i // 768] != b"system0" for i in hits):
        raise CopyError("unexpected GG probe layout after replacing only tofrom")


def run_session(builder: Path, mode: str, monitor: Path, image: Path,
                work: Path, log: Path) -> None:
    with log.open("xb") as output:
        result = subprocess.run([str(builder), mode, str(monitor), str(image), str(work)],
                                stdout=output, stderr=subprocess.STDOUT, timeout=120)
    if result.returncode != 0:
        raise CopyError(f"{mode} failed; inspect {log}")


def verify_lifecycle_transfers(directory: Path) -> None:
    for name in ("resumed", "recreated"):
        if (directory / name).read_bytes() != TEXT.replace(b"\r", b""):
            raise CopyError(f"{name} text transfer differs")
    binary = (directory / "binresume").read_bytes()
    if binary[:len(BINARY)] != BINARY or len(binary) % 768 != 0:
        raise CopyError("resumed binary transfer differs")


def verify_isolation_transfers(directory: Path) -> None:
    for test in range(4):
        if (directory / f"after{test}").read_bytes() != TEXT.replace(b"\r", b""):
            raise CopyError(f"sibling transfer after fault {test} differs")


def verify_algol_transfers(directory: Path) -> None:
    if (directory / "algtext").read_bytes() != TEXT.replace(b"\r", b""):
        raise CopyError("protected-compiled text transfer differs")
    binary = (directory / "algbinary").read_bytes()
    if binary[:len(BINARY)] != BINARY or len(binary) % 768 != 0:
        raise CopyError("protected-compiled binary transfer differs")
    for name, segments in (("pcinbin", 54), ("pcoutbin", 46)):
        data = (directory / name).read_bytes()
        if len(data) != segments * 768:
            raise CopyError(f"protected-compiled program allocation differs: {name}")
        check_compiled(data)
    for name in ("hellobin", "arithbin"):
        data = (directory / name).read_bytes()
        if not data or len(data) % 768 != 0:
            raise CopyError(f"missing or incomplete Algol program export: {name}")


def verify_base_diagnostics(transcript: bytes, system_binding: bool) -> None:
    binding = b"system" if system_binding else b"temporary"
    status = b"absent" if system_binding else b"present"
    marker = b"Algol transfer binding=" + binding + b" base-diagnostic=" + status + b"\n"
    if transcript.count(marker) != 2 or \
            transcript.count(b"oldalgollib bases") != (0 if system_binding else 2):
        raise CopyError("protected Algol library-base comparison differs")


def verify_nested_checkpoint(transcript: bytes) -> None:
    for size in (2, 20, 26):
        for stage, parent, child in (("running", 2, 1), ("tree-stopped", 0, 0),
                ("tree-resumed", 2, 1), ("child-stopped-by-parent", 2, 2),
                ("tree-removed", 0, 0)):
            marker = (f"Nested keys {stage}: parent={parent} child={child} size={size} "
                      "full-area=checked\n").encode("ascii")
            if transcript.count(marker) != 1:
                raise CopyError(f"missing/duplicate nested key observation: {stage}, size {size}")
        expression = (rf"Nested guest case size={size} passed; privilege-widening=denied "
                      rf"stop-answer=released child-steps=([1-9][0-9]*)\n").encode("ascii")
        if len(re.findall(expression, transcript)) != 1:
            raise CopyError(f"nested guest execution/delegation evidence differs: size {size}")
    if transcript.count(b"Protected nested-process checkpoint passed\n") != 1 or \
            transcript.count(b"Remaining GG probe executions=0\n") != 1:
        raise CopyError("nested final checkpoint evidence differs")


def probe(work: Path, builder: Path, candidate: Path, lifecycle: bool = False,
          isolation: bool = False, algol: bool = False, system_binding: bool = False,
          nested: bool = False) -> None:
    if nested:
        system_binding = True
    source = ROOT / "src/disks/UNIT0"
    monitor = ROOT / "src/monitor/MONITOR"
    before_monitor = hashlib.sha256(monitor.read_bytes()).hexdigest()
    before_candidate = hashlib.sha256(candidate.read_bytes()).hexdigest()
    work.mkdir()  # Never overwrite or reuse an earlier experiment.
    image = work / "UNIT0"
    create_copy(source, image, allow_native=True)
    build = work / "compile"
    verify = work / "verify"
    build.mkdir()
    verify.mkdir()
    for directory in (build, verify):
        (directory / "input").write_bytes(TEXT)
        (directory / "binary").write_bytes(BINARY)
    for name in ("fromhost.alg", "tohost.alg"):
        shutil.copyfile(ROOT / "src/guest" / name, build / name)
    if algol:
        shutil.copyfile(ROOT / "tests/fixtures/hello.alg", verify / "alginput")
        shutil.copyfile(ROOT / "tests/fixtures/protected_algol.alg", verify / "arithinput")
        shutil.copyfile(ROOT / "src/guest/fromhost.alg", verify / "frominput")
        shutil.copyfile(ROOT / "src/guest/tohost.alg", verify / "toinput")
    try:
        build_mode = "--build-rc4000-profile" if system_binding else "--build-rc4000-transfers"
        run_session(builder, build_mode, monitor, image, build,
                    work / "compile.log")
        verify_transfers(build)
        for name in ("rcinbinary", "rcoutbinary"):
            check_compiled((build / name).read_bytes())
        compiled_digest = hashlib.sha256(image.read_bytes()).hexdigest()
        mode = ("--verify-protected-nested" if nested else
                "--verify-protected-algol-profile" if system_binding else
                "--verify-protected-algol" if algol else
                "--verify-protected-isolation" if isolation else
                "--verify-protected-lifecycle" if lifecycle else "--verify-rc4000-transfers")
        run_session(builder, mode, candidate, image, verify,
                    work / "verify.log")
        verify_transfers(verify)
        if lifecycle:
            verify_lifecycle_transfers(verify)
        if isolation:
            verify_isolation_transfers(verify)
        if algol:
            verify_algol_transfers(verify)
            verify_base_diagnostics((work / "verify.log").read_bytes(), system_binding)
        if nested:
            verify_nested_checkpoint((work / "verify.log").read_bytes())
        if hashlib.sha256(image.read_bytes()).hexdigest() != compiled_digest:
            raise CopyError("protected verification changed its base image")
        (work / "result.txt").write_text(
            "Experimental older-runtime text/binary checkpoint passed.\n"
            f"process-lifecycle={'passed' if lifecycle else 'not tested'}\n"
            f"process-isolation={'passed' if isolation else 'not tested'}\n"
            f"guest-algol={'passed' if algol else 'not tested'}\n"
            f"nested-processes={'passed' if nested else 'not tested'}\n"
            f"tofrom-binding={'derived-system' if system_binding else 'original-system'}\n"
            f"library-base-diagnostic={'not tested' if not algol else 'absent' if system_binding else 'present'}\n"
            "This is not monitor acceptance or an image promotion.\n"
            f"monitor-sha256={before_candidate}\n"
            f"disk-sha256={compiled_digest}\n", encoding="ascii")
    finally:
        verify_source(source, allow_native=True)
        if hashlib.sha256(monitor.read_bytes()).hexdigest() != before_monitor:
            raise CopyError("stable monitor changed during experiment")
        if hashlib.sha256(candidate.read_bytes()).hexdigest() != before_candidate:
            raise CopyError("candidate monitor changed during experiment")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new private work directory")
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--lifecycle", action="store_true",
                        help="also verify stop/start/remove/recreation and whole-area keys")
    parser.add_argument("--isolation", action="store_true",
                        help="verify independent consoles and deliberate sibling/privilege faults")
    parser.add_argument("--algol", action="store_true",
                        help="compile and execute guest Algol programs with protection enabled")
    parser.add_argument("--nested", action="store_true",
                        help="verify genuine nested guest processes on the private system-binding profile")
    parser.add_argument("--system-binding", action="store_true",
                        help="with --algol, retain the older system binding only in the private copy")
    parser.add_argument("--builder", type=Path,
                        default=ROOT / "build-protection/sim4000_build_host_image")
    args = parser.parse_args()
    if sum((args.lifecycle, args.isolation, args.algol, args.nested)) > 1:
        parser.error("use separate work directories for lifecycle, isolation, Algol and nested probes")
    if args.system_binding and not (args.algol or args.nested):
        parser.error("--system-binding requires --algol or --nested")
    try:
        probe(args.destination.resolve(), args.builder.resolve(), args.candidate.resolve(),
              args.lifecycle, args.isolation, args.algol, args.system_binding or args.nested,
              args.nested)
    except (CopyError, OSError, subprocess.TimeoutExpired) as error:
        print(f"older-runtime checkpoint not accepted: {error}", file=sys.stderr)
        return 1
    print(f"older-runtime checkpoint passed: {args.destination}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
