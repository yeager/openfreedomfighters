#!/usr/bin/env python3
"""Accept two identical, source-free startup coordinator observations.

Inputs must already be sanitized by ``startup_coordinator_pass_selection_trace``.
This repeat gate accepts neither raw observer output nor game material.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

import startup_coordinator_pass_selection_trace as trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = trace.OUTPUT_FORMAT
OUTPUT_FORMAT = "off.startup-coordinator-pass-selection-repeat-pair/v1"
MAX_PRIVATE_RECORD_BYTES = 8 * 1024 * 1024


def _validated_trace(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != frozenset(("format", "events")):
        raise ValueError("sanitized trace must contain only format and events")
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized sanitized trace format")
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": raw["events"]})


def _terminal(events: list[dict[str, Any]]) -> bool:
    return any(event["phase"] == "completion" or event["phase"] == "failure" for event in events)


def sanitize_repeat_pair(first: Any, second: Any) -> dict[str, Any]:
    """Return one trace only if two fresh observations agree exactly."""
    first_clean = _validated_trace(first)
    second_clean = _validated_trace(second)
    if first_clean["events"] != second_clean["events"]:
        raise ValueError("repeat observations disagree on startup coordinator pass selection")
    if not _terminal(first_clean["events"]):
        raise ValueError("repeat observation lacks a terminal coordinator boundary")
    return {"format": OUTPUT_FORMAT, "events": first_clean["events"]}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _private_parent(path: pathlib.Path, label: str) -> None:
    parent = path.parent
    if parent.is_symlink() or not parent.is_dir():
        raise ValueError(f"{label} parent must be an existing private directory")


def _read_private_json_no_follow(path: pathlib.Path, label: str) -> Any:
    _private_parent(path, label)
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError(f"platform cannot safely read {label}")
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NONBLOCK | no_follow)
    except OSError as error:
        raise ValueError(f"{label} must be a regular private file") from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_PRIVATE_RECORD_BYTES:
            raise ValueError(f"{label} must be a bounded regular private file")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(descriptor)


def _write_new_private_json_no_follow(path: pathlib.Path, record: dict[str, Any]) -> None:
    _private_parent(path, "output")
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError("platform cannot safely write output")
    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | no_follow, 0o600)
    except OSError as error:
        raise ValueError("refusing to overwrite private output") from error
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
            json.dump(record, stream, indent=2)
            stream.write("\n")
    finally:
        os.close(descriptor)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first", type=pathlib.Path, help="first private sanitized observation")
    parser.add_argument("second", type=pathlib.Path, help="second private sanitized observation")
    parser.add_argument("output", type=pathlib.Path, help="new private repeat-pair JSON path")
    args = parser.parse_args()
    try:
        first_path = _outside_repository(args.first, "first input")
        second_path = _outside_repository(args.second, "second input")
        output_path = _outside_repository(args.output, "output")
        if len({first_path, second_path, output_path}) != 3:
            raise ValueError("inputs and output paths must differ")
        if output_path.exists():
            raise ValueError("refusing to overwrite an existing private trace")
        result = sanitize_repeat_pair(_read_private_json_no_follow(first_path, "first input"),
                                      _read_private_json_no_follow(second_path, "second input"))
        _write_new_private_json_no_follow(output_path, result)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(result['events'])} source-free startup coordinator repeat records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
