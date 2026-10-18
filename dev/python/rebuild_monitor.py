#!/usr/bin/env python3
"""Build the monitor from sim4000.mops with the guest SLANG assembler."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys

from make_host_image import BINARY, TEXT, verify_transfers
from make_unit0_copy import CopyError, create_copy, verify_source

ROOT = Path(__file__).resolve().parents[2]
BUILD_FILE = "sim4000.mops"
PROTECTED_BUILD_FILE = "sim4000-protected.mops"
PROTECTION_SOURCE_SHA256 = "e57db6c30a20095044d0da160406f5f0c2d5f755af838faf6e57c959eb88cae2"
PROTECTION_BYPASS = b"     jl       x2               ; 94-06-01 21:03 HJ , skip set pk action\n"


def listing_succeeded(listing: bytes) -> bool:
    """Distinguish an assembly result from harmless monitor message comments."""
    return (b"slang ok" in listing and b"***slang" not in listing and
            diagnostics_clean(listing))


def diagnostics_clean(data: bytes) -> bool:
    return not any(marker in data.lower() for marker in
                   (b"***edit", b"***slang", b"***fp", b"***device status",
                    b"syntax error:", b"input nesting"))


def validate_protection_source(data: bytes) -> None:
    """Read-only identity/uniqueness check; all source editing happens in FP."""
    if (hashlib.sha256(data).hexdigest() != PROTECTION_SOURCE_SHA256 or
            data.count(PROTECTION_BYPASS) != 1 or data.count(b"e22: c.") != 1):
        raise CopyError("protected build requires the recognized, unambiguous montxt10 input")


def validate_protected_inputs(common: bytes, entry: bytes) -> None:
    """The console calls the wrapper (depth 1), which calls common (depth 2)."""
    includes = rb"(?m)^[ \t]*i[ \t]+([^;\r\n]+)"
    if re.findall(includes, common) or re.findall(includes, entry) != [b"mcommon"]:
        raise CopyError("protected build must use exactly two FP input levels")


def verify_guest_protection_edit(original: bytes, edited: bytes, restored: bytes) -> None:
    """Check exported guest results; never prepare or write a patched source."""
    validate_protection_source(original)
    before, bypass, after = original.partition(PROTECTION_BYPASS)
    if not bypass or edited != before + after or restored != original:
        raise CopyError("guest e22 edit or original-source restoration differs")


def prepare_sources(source: Path, destination: Path, *, protected: bool = False,
                    dfc403: bool = False) -> None:
    """Prepare the maintained build inputs, preserving originals."""
    names = [f"montxt{i}" for i in range(1, 12)] + ["mon8s", "mon8init"]
    names.append(BUILD_FILE)
    if protected:
        # Validate before writing any prepared source. No Python patching.
        validate_protection_source((source / "montxt10").read_bytes().replace(b"\r\n", b"\n"))
        validate_protected_inputs((source / BUILD_FILE).read_bytes(),
                                  (source / PROTECTED_BUILD_FILE).read_bytes())
        names.append(PROTECTED_BUILD_FILE)
    manifest = []
    for name in names:
        data = (source / name).read_bytes()
        normalized = data.replace(b"\r\n", b"\n")
        # The only high bytes in the delivered options are CP850 Danish
        # letters in comments. Use the historical RC 7-bit positions for
        # small a-ring (}) and ae ({), as mon8mops already does.
        for line in normalized.splitlines():
            if any(b > 127 for b in line) and (
                    not line.lstrip().startswith(b";") or
                    any(b > 127 and b not in (0x86, 0x91) for b in line)):
                raise CopyError(f"unsupported non-ASCII source text in {name}")
        normalized = normalized.translate(bytes.maketrans(b"\x86\x91", b"}{"))
        if dfc403 and name == BUILD_FILE:
            native = (b"t.7 d.15     n.disc  c.144.36.73080  ; disc\n"
                      b"t.7 d.16 r.7 n.disc1 c.144.36.73080  ; disc1-disc7\n")
            hardware = b";t.8  i.22.23 d.15 c.144.36.73080   ; 15 disc\n"
            if normalized.count(native) != 1 or normalized.count(hardware) != 1:
                raise CopyError("DFC403 build requires recognized disk options in sim4000.mops")
            normalized = normalized.replace(native, b"", 1).replace(hardware, hardware[1:], 1)
        if name == BUILD_FILE:
            target = "mcommon" if protected else "mbuild"
        elif name == PROTECTED_BUILD_FILE:
            target = "mbuild"
        else:
            target = name.lower()
        (destination / target).write_bytes(normalized)
        manifest.append(f"{name}: sha256={hashlib.sha256(data).hexdigest()} "
                        f"normalized_sha256={hashlib.sha256(normalized).hexdigest()}")
    (destination / "sources.txt").write_text("\n".join(manifest) + "\n", encoding="ascii")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new work directory; must not exist")
    parser.add_argument("--sources", type=Path, default=ROOT / "monsrc",
                        help="monitor source directory (default: repository monsrc)")
    parser.add_argument("--builder", type=Path,
                        default=ROOT / "build-release" / "sim4000_build_host_image")
    parser.add_argument("--image", type=Path, default=ROOT / "src/disks/UNIT0")
    parser.add_argument("--monitor", type=Path, default=ROOT / "src/monitor/MONITOR")
    parser.add_argument("--verify-boot", action="store_true",
                        help="boot the candidate and verify text/binary host transfers")
    parser.add_argument("--protected", action="store_true",
                        help="use the guest-editor Protected MOPS entry; verify with RC4000 protection")
    parser.add_argument("--dfc403", action="store_true",
                        help="select hardware DFC403 disk options in sim4000.mops")
    args = parser.parse_args()
    try:
        verify_source(args.image, allow_native=True)
        work = args.destination.resolve()
        work.mkdir()  # Refuse existing directories and never overwrite an earlier attempt.
        if args.dfc403 and args.protected:
            raise CopyError("combined DFC403/protected profile is not validated")
        prepare_sources(args.sources, work, protected=args.protected, dfc403=args.dfc403)
        if args.protected:
            (work / "profile.txt").write_text(
                "protection-mode=rc4000\nsource-transform=guest-editor-e22-v1\n"
                "entry=sim4000-protected.mops\nshared=sim4000.mops\nfp-input-depth=2\n",
                encoding="ascii")
        create_copy(args.image, work / "UNIT0", allow_native=True)
        with (work / "session.log").open("wb") as log:
            result = subprocess.run([str(args.builder.resolve()), "--monitor-build",
                                     str(args.monitor.resolve()), str(work / "UNIT0"),
                                     str(work)], stdout=log, stderr=subprocess.STDOUT,
                                    timeout=300)
        verify_source(args.image, allow_native=True)
        verify_source(work / "UNIT0", allow_native=True)
        listing = (work / "buildlist").read_bytes() if (work / "buildlist").exists() else b""
        # Monitor message comments themselves include harmless '***' markers.
        if result.returncode != 0 or not listing_succeeded(listing):
            raise CopyError(f"guest generation failed; inspect {work / 'session.log'} "
                            f"and {work / 'buildlist'}")
        if args.protected:
            editor_log = (work / "pkeditlog").read_bytes()
            if b"edit end." not in editor_log or not diagnostics_clean(editor_log):
                raise CopyError("guest protected editor failed; inspect pkeditlog")
            verify_guest_protection_edit((work / "montxt10").read_bytes(),
                                         (work / "pkedited").read_bytes(),
                                         (work / "pkrestored").read_bytes())
        data = (work / "candidate").read_bytes()
        if not data or len(data) % 768 != 0:
            raise CopyError("candidate is empty or not a complete disk allocation")
        print(f"Monitor candidate: {work / 'candidate'} size={len(data)} "
              f"sha256={hashlib.sha256(data).hexdigest()}")
        identical = data == args.monitor.read_bytes()
        print(f"Byte-identical to bootstrap monitor: {'yes' if identical else 'no'}")
        if args.protected:
            with (work / "profile.txt").open("a", encoding="ascii") as profile:
                profile.write(f"candidate-sha256={hashlib.sha256(data).hexdigest()}\n"
                              f"candidate-bytes={len(data)}\n")
            (work / "RC4000.protected.cfg").write_text(
                "; Experimental protected monitor; keep separate from the stable profile.\n"
                "PROTECTION MODE=RC4000\nCORESIZE=0,1048576\n"
                "MONITOR FILE=candidate\n"
                "DEFINE CONSOLE DEVNO=2 INTNO=6,3\n"
                "DEFINE CLOCK DEVNO=3 INTNO=11\n"
                "DEFINE RC610 DEVNO=7 INTNO=10 FILE=printer.protected.txt APPEND=YES\n"
                "DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
                "DEFINE RC433X DEVNO=15 FILE=UNIT0\n"
                "DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT=.\n", encoding="ascii")
        if args.verify_boot:
            (work / "input").write_bytes(TEXT)
            (work / "binary").write_bytes(BINARY)
            with (work / "boot.log").open("wb") as log:
                mode = "--verify-protected-profile" if args.protected else "--verify-dfc" if args.dfc403 else "--verify"
                verification_image = work / "UNIT0"
                if args.protected:
                    from prepare_monitor_profiles import PROFILE_SHA256, verified_bytes
                    verification_image = ROOT / "src/disks/UNIT0.protected"
                    verified_bytes(verification_image, 4660224, PROFILE_SHA256)
                subprocess.run([str(args.builder.resolve()), mode,
                                str(work / "candidate"), str(verification_image),
                                str(work)], stdout=log, stderr=subprocess.STDOUT,
                               check=True, timeout=120)
            verify_transfers(work)
            if args.protected:
                verified_bytes(verification_image, 4660224, PROFILE_SHA256)
            verify_source(work / "UNIT0", allow_native=True)
            profile = "RC4000 protection" if args.protected else "compatibility mode"
            print(f"Candidate boot and text/binary host transfers verified ({profile}).")
        else:
            print("Candidate boot has not been tested; use --verify-boot to verify it.")
        verify_source(args.image, allow_native=True)
        print("The shipped monitor and images have not been replaced.")
    except (CopyError, OSError, subprocess.TimeoutExpired, subprocess.CalledProcessError) as error:
        print(f"monitor rebuild not completed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
