#!/usr/bin/env python3
"""Rebuild host transfer utilities in a derived image using guest Algol."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from make_unit0_copy import CopyError, create_copy, verify_source


ROOT = Path(__file__).resolve().parents[2]
TEXT = b"Native host file exchange.\r\nSecond line.\n"
BINARY = bytes(range(256)) * 3


def verify_transfers(directory: Path) -> None:
    if (directory / "output").read_bytes() != TEXT.replace(b"\r", b""):
        raise CopyError("native text round trip differs")
    binary = (directory / "binoutput").read_bytes()
    if binary[:len(BINARY)] != BINARY or len(binary) % 768 != 0:
        raise CopyError("native binary transfer prefix or allocation differs")


def build_image(destination: Path, builder: Path, source: Path,
                monitor: Path, guest_sources: Path) -> str:
    destination = destination.absolute()
    if destination.exists() or destination.is_symlink():
        raise CopyError(f"destination already exists: {destination}")
    verify_source(source)
    with tempfile.TemporaryDirectory(prefix=".sim4000-host-",
                                     dir=destination.parent) as temporary:
        directory = Path(temporary)
        image = directory / "UNIT0.host"
        create_copy(source, image)
        for name in ("fromhost.alg", "tohost.alg"):
            shutil.copyfile(guest_sources / name, directory / name)
        (directory / "input").write_bytes(TEXT)
        (directory / "binary").write_bytes(BINARY)
        subprocess.run([str(builder), str(monitor), str(image), str(directory)],
                       check=True)
        verify_transfers(directory)
        (directory / "output").unlink()
        (directory / "binoutput").unlink()
        before = hashlib.sha256(image.read_bytes()).hexdigest()
        subprocess.run([str(builder), "--verify", str(monitor), str(image),
                        str(directory)], check=True)
        verify_transfers(directory)
        digest = hashlib.sha256(image.read_bytes()).hexdigest()
        if digest != before:
            raise CopyError("verification boot changed the derived base image")
        verify_source(source)
        with image.open("rb") as stream:
            os.fsync(stream.fileno())
        # Same filesystem; hard-link publication refuses races or overwrites.
        os.link(image, destination)
        return digest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--builder", type=Path,
                        default=ROOT / "build" / "sim4000_build_host_image")
    parser.add_argument("--source", type=Path,
                        default=ROOT / "src" / "disks" / "UNIT0.original")
    parser.add_argument("--monitor", type=Path,
                        default=ROOT / "src" / "monitor" / "MONITOR")
    parser.add_argument("--guest-sources", type=Path,
                        default=ROOT / "src" / "guest")
    args = parser.parse_args()
    try:
        digest = build_image(args.destination, args.builder.resolve(),
                             args.source.resolve(), args.monitor.resolve(),
                             args.guest_sources.resolve())
    except (CopyError, OSError, subprocess.CalledProcessError) as error:
        print(f"native image not created: {error}", file=sys.stderr)
        return 1
    print(f"native image created: {args.destination} sha256={digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
