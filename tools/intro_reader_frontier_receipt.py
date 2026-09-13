#!/usr/bin/env python3
"""Create a source-free receipt for a successful private reader-frontier probe."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import stat
import sys

REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
FORMAT = "off.intro-reader-frontier-receipt/v1"
MAX_TRANSCRIPT_BYTES = 128 * 1024
EXPECTED_FAMILY_RECORDS = 6
_FRONTIER = re.compile(
    r"^reader-frontier-family=[a-z-]+ owners=\d+ instances=\d+ "
    r"deferred-blocks=\d+ profiled-blocks=\d+ unprofiled-blocks=\d+ "
    r"distinct-bounded-shapes=\d+ largest-bounded-shape=\d+ "
    r"repeated-bounded-shape=(?:yes|no)$")


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _open_parent(path: pathlib.Path, label: str) -> tuple[int, str]:
    no_follow, directory = getattr(os, "O_NOFOLLOW", None), getattr(os, "O_DIRECTORY", None)
    if no_follow is None or directory is None:
        raise ValueError(f"platform cannot safely open {label}")
    try:
        descriptor = os.open(path.parent, os.O_RDONLY | directory | no_follow)
    except OSError as error:
        raise ValueError(f"{label} parent must be an existing real directory") from error
    if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise ValueError(f"{label} parent must be an existing real directory")
    return descriptor, path.name


def _read_transcript(path: pathlib.Path) -> str:
    directory, name = _open_parent(path, "transcript")
    try:
        try:
            descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=directory)
        except OSError as error:
            raise ValueError("transcript must be a regular private file") from error
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_TRANSCRIPT_BYTES:
                raise ValueError("transcript must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
                return stream.read()
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def receipt_from_transcript(transcript: str) -> dict[str, str]:
    """Validate aggregate frontier lines and discard every measurement."""
    lines = transcript.splitlines()
    frontier = [line for line in lines if line.startswith("reader-frontier-family=")]
    names = [line.split(" ", 1)[0] for line in frontier]
    if (len(frontier) != EXPECTED_FAMILY_RECORDS or len(set(names)) != EXPECTED_FAMILY_RECORDS or
            any(_FRONTIER.fullmatch(line) is None for line in frontier)):
        raise ValueError("transcript lacks a complete aggregate reader-frontier record")
    if "first-cut-cold-probe=completed" not in lines:
        raise ValueError("transcript does not attest completed cold probing")
    return {"format": FORMAT, "probe": "first-cut-cold", "reader_frontier": "observed",
            "reader_admission": "unchanged", "playback": "not_started"}


def _write_new_receipt(path: pathlib.Path, receipt: dict[str, str]) -> None:
    directory, name = _open_parent(path, "receipt")
    try:
        try:
            descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                 0o600, dir_fd=directory)
        except OSError as error:
            raise ValueError("refusing to overwrite receipt") from error
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
                json.dump(receipt, stream, indent=2, sort_keys=True)
                stream.write("\n")
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("transcript", type=pathlib.Path)
    parser.add_argument("receipt", type=pathlib.Path)
    args = parser.parse_args()
    try:
        transcript_path = _outside_repository(args.transcript, "transcript")
        receipt_path = _outside_repository(args.receipt, "receipt")
        if transcript_path == receipt_path:
            raise ValueError("transcript and receipt paths must differ")
        _write_new_receipt(receipt_path, receipt_from_transcript(_read_transcript(transcript_path)))
    except (OSError, UnicodeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote source-free intro reader-frontier receipt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
