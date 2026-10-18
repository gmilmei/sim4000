#!/usr/bin/env python3
"""Create a verified working copy of a shipped native or original disk image."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import stat
import sys


ORIGINAL_SIZE = 4_658_688
ORIGINAL_SHA256 = "838621f65fef164cf55fca6ad60ff4f1b3ae5b5326fd7f29a38bfc75a4a5b22a"
NATIVE_SIZE = 4_660_224
NATIVE_SHA256 = "e5d293bd01c39385218c11531412156ae4b65afa542c1263b2dd3c919f630a1e"
COPY_CHUNK_SIZE = 1024 * 1024


class CopyError(Exception):
    """The working copy could not be created safely."""


def digest_stream(stream: object) -> tuple[int, str]:
    digest = hashlib.sha256()
    size = 0
    while True:
        block = stream.read(COPY_CHUNK_SIZE)
        if not block:
            break
        size += len(block)
        digest.update(block)
    return size, digest.hexdigest()


def verify_source(path: Path, allow_native: bool = False) -> tuple[int, str]:
    try:
        with path.open("rb") as stream:
            mode = os.fstat(stream.fileno()).st_mode
            if not stat.S_ISREG(mode):
                raise CopyError(f"source is not a regular file: {path}")
            size, digest = digest_stream(stream)
    except OSError as error:
        raise CopyError(f"cannot read source {path}: {error}") from error
    accepted = {(ORIGINAL_SIZE, ORIGINAL_SHA256)}
    if allow_native:
        accepted.add((NATIVE_SIZE, NATIVE_SHA256))
    if (size, digest) not in accepted:
        raise CopyError(
            f"source is not an accepted pristine disk fixture: size={size}, "
            f"sha256={digest}"
        )
    return size, digest


def create_copy(source: Path, destination: Path,
                allow_native: bool = False) -> tuple[int, str]:
    expected_size, expected_digest = verify_source(source, allow_native)
    destination_created = False
    try:
        with source.open("rb") as input_stream:
            descriptor = os.open(
                destination,
                os.O_WRONLY | os.O_CREAT | os.O_EXCL,
                0o600,
            )
            destination_created = True
            with os.fdopen(descriptor, "wb") as output_stream:
                while True:
                    block = input_stream.read(COPY_CHUNK_SIZE)
                    if not block:
                        break
                    output_stream.write(block)
                output_stream.flush()
                os.fsync(output_stream.fileno())

        with destination.open("rb") as stream:
            copied_size, copied_digest = digest_stream(stream)
        if copied_size != expected_size or copied_digest != expected_digest:
            raise CopyError(
                "working-copy verification failed: "
                f"size={copied_size}, sha256={copied_digest}"
            )
    except FileExistsError as error:
        raise CopyError(f"destination already exists: {destination}") from error
    except (OSError, CopyError) as error:
        if destination_created:
            try:
                destination.unlink()
            except OSError:
                pass
        if isinstance(error, CopyError):
            raise
        raise CopyError(f"cannot create {destination}: {error}") from error
    return expected_size, expected_digest


def main() -> int:
    default_source = (
        Path(__file__).resolve().parents[2] / "src" / "disks" / "UNIT0"
    )
    parser = argparse.ArgumentParser(
        description=(
            "Verify a shipped native/original fixture and create an exclusive, "
            "writable byte-for-byte working copy."
        )
    )
    parser.add_argument("destination", type=Path)
    parser.add_argument(
        "--source",
        type=Path,
        default=default_source,
        help=f"source image (default: {default_source})",
    )
    arguments = parser.parse_args()
    try:
        size, digest = create_copy(arguments.source, arguments.destination,
                                   allow_native=True)
    except CopyError as error:
        print(f"working copy not created: {error}", file=sys.stderr)
        return 1
    print(
        f"working copy created: {arguments.destination} "
        f"size={size} sha256={digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
