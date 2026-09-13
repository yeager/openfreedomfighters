#!/usr/bin/env python3
"""Create a source-free receipt for a private intro-readiness probe."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import stat
import sys

REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
FORMAT = "off.intro-readiness-receipt/v1"
MAX_TRANSCRIPT_BYTES = 128 * 1024
_READER = re.compile(r"^reader-coverage-(discovered|recognized|applied|unapplied)=(\d+)$")
_LIFECYCLE = re.compile(
    r"^lifecycle-coverage=(readers|components|owners) required=(\d+) covered=(\d+) uncovered=(\d+)$")
_LIFECYCLE_STATUS = re.compile(
    r"^lifecycle-coverage-status=(reader-coverage|component-coverage|owner-coverage|complete)$")
_COLD = {
    "intro-readiness-lifecycle=not-admitted": ("lifecycle", "not_admitted"),
    "intro-readiness-renderer=not-created": ("renderer", "not_created"),
    "intro-readiness-audio=not-started": ("audio", "not_started"),
    "intro-readiness-playback=not-started": ("playback", "not_started"),
}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    """Resolve a private path only after rejecting every symlink component."""
    absolute = path if path.is_absolute() else pathlib.Path.cwd() / path
    current = pathlib.Path(absolute.anchor)
    for component in absolute.parts[1:]:
        if component == "..":
            raise ValueError(f"{label} must not contain parent traversal")
        current /= component
        if current.exists() and current.is_symlink():
            raise ValueError(f"{label} must not traverse a symlink")
    resolved = absolute.resolve()
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


def _one(records: dict[str, object], key: str, value: object, label: str) -> None:
    if key in records:
        raise ValueError(f"transcript has duplicate {label}")
    records[key] = value


def receipt_from_transcript(transcript: str) -> dict[str, object]:
    """Validate aggregate readiness evidence and retain only its safe schema."""
    lines = transcript.splitlines()
    if lines.count("intro-readiness-probe=completed") != 1:
        raise ValueError("transcript lacks one completed intro readiness probe")
    if lines.count("intro-readiness-reader-bracket=complete") != 1:
        raise ValueError("transcript lacks one complete reader bracket")

    reader: dict[str, int] = {}
    lifecycle: dict[str, dict[str, int]] = {}
    statuses: dict[str, str] = {}
    lifecycle_status: str | None = None
    for line in lines:
        if match := _READER.fullmatch(line):
            name, value = match.groups()
            if name in reader:
                raise ValueError("transcript has duplicate reader coverage")
            reader[name] = int(value)
        elif match := _LIFECYCLE.fullmatch(line):
            name, required, covered, uncovered = match.groups()
            if name in lifecycle:
                raise ValueError("transcript has duplicate lifecycle coverage")
            lifecycle[name] = {"required": int(required), "covered": int(covered),
                               "uncovered": int(uncovered)}
        elif match := _LIFECYCLE_STATUS.fullmatch(line):
            if lifecycle_status is not None:
                raise ValueError("transcript has duplicate lifecycle status")
            lifecycle_status = match.group(1)
        elif line in _COLD:
            name, value = _COLD[line]
            _one(statuses, name, value, "cold status")

    if set(reader) != {"discovered", "recognized", "applied", "unapplied"}:
        raise ValueError("transcript lacks complete reader coverage")
    if not reader["applied"] <= reader["recognized"] <= reader["discovered"] or (
            reader["unapplied"] != reader["discovered"] - reader["applied"]):
        raise ValueError("transcript has inconsistent reader coverage")
    if set(lifecycle) != {"readers", "components", "owners"}:
        raise ValueError("transcript lacks complete lifecycle coverage")
    for counts in lifecycle.values():
        if counts["covered"] > counts["required"] or (
                counts["uncovered"] != counts["required"] - counts["covered"]):
            raise ValueError("transcript has inconsistent lifecycle coverage")
    expected_status = ("reader-coverage" if lifecycle["readers"]["uncovered"] else
                       "component-coverage" if lifecycle["components"]["uncovered"] else
                       "owner-coverage" if lifecycle["owners"]["uncovered"] else "complete")
    if lifecycle_status != expected_status:
        raise ValueError("transcript lifecycle status disagrees with coverage")
    if statuses != dict(_COLD.values()):
        raise ValueError("transcript lacks complete cold statuses")

    return {"format": FORMAT, "probe": "intro-readiness", "reader_bracket": "complete",
            "reader_coverage": reader, "lifecycle_coverage": lifecycle,
            "lifecycle_status": lifecycle_status, **statuses}


def _write_new_receipt(path: pathlib.Path, receipt: dict[str, object]) -> None:
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
    print("wrote source-free intro readiness receipt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
