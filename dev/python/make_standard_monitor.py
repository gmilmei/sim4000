#!/usr/bin/env python3
"""Build and verify the DFC403 monitor from maintained sim4000.mops."""

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from make_host_image import ROOT
from make_unit0_copy import CopyError, verify_source


def build_monitor(destination: Path, builder: Path, image: Path,
                  monitor: Path) -> str:
    destination = destination.absolute()
    if destination.exists() or destination.is_symlink():
        raise CopyError(f"destination already exists: {destination}")
    verify_source(image, allow_native=True)
    with tempfile.TemporaryDirectory(prefix=".sim4000-standard-",
                                     dir=destination.parent) as temporary:
        directory = Path(temporary)
        generation = directory / "generation"
        subprocess.run([sys.executable, str(ROOT / "dev/python/rebuild_monitor.py"),
                        str(generation), "--builder", str(builder),
                        "--image", str(image), "--monitor", str(monitor),
                        "--dfc403", "--verify-boot"], check=True)
        standard = generation / "candidate"
        data = standard.read_bytes()
        if len(data) != 38400:
            raise CopyError("unexpected standard monitor allocation size")
        verify_source(image, allow_native=True)
        with standard.open("rb") as stream:
            os.fsync(stream.fileno())
        os.link(standard, destination)
        return hashlib.sha256(data).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--builder", type=Path,
                        default=ROOT / "build" / "sim4000_build_host_image")
    parser.add_argument("--image", type=Path,
                        default=ROOT / "src" / "disks" / "UNIT0")
    parser.add_argument("--monitor", type=Path,
                        default=ROOT / "src" / "monitor" / "MONITOR")
    args = parser.parse_args()
    try:
        digest = build_monitor(args.destination, args.builder.resolve(),
                               args.image.resolve(), args.monitor.resolve())
    except (CopyError, OSError, subprocess.CalledProcessError) as error:
        print(f"standard monitor not created: {error}", file=sys.stderr)
        return 1
    print(f"standard monitor created: {args.destination} sha256={digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
