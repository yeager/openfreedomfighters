#!/usr/bin/env python3
"""Sanitize a private launch-time intro-audio file-access observation.

The collector and its private path matcher are deliberately out of tree.  This
boundary accepts only opaque inventory ordinals and categorical access results;
it cannot carry file names, paths, retail metadata, hashes, bytes, timing, or
debugger/executable details.  An opened stream establishes file access only,
not audio playback or a cue-to-stream binding.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.intro-audio-launch-access.raw/v1"
OUTPUT_FORMAT = "off.intro-audio-launch-access/v1"
MAX_EVENTS = 512
MAX_STREAM_ORDINAL = 65535
MAX_PRIVATE_RECORD_BYTES = 256 * 1024
FIELDS = frozenset(("observation_order", "intro_stream_ordinal", "bank", "access_result"))
BANKS = frozenset(("local", "global"))
RESULTS = frozenset(("opened", "missing", "denied"))


def _exact(value: Any, fields: frozenset[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict) or frozenset(value) != fields:
        raise ValueError(f"{label} has an unsupported field")
    return value


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(f"{label} must be a bounded unsigned integer")
    return value


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the sole permitted structural opening-cinematic access record."""
    trace = _exact(raw, frozenset(("format", "fresh_isolated", "events")), "trace")
    if trace["format"] != INPUT_FORMAT or trace["fresh_isolated"] is not True:
        raise ValueError("trace must be a fresh isolated launch observation")
    events = trace["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain bounded events")
    prior = -1
    seen: set[tuple[int, str, str]] = set()
    clean: list[dict[str, Any]] = []
    for record in events:
        event = _exact(record, FIELDS, "trace event")
        order = _natural(event["observation_order"], "observation_order", MAX_EVENTS)
        stream = _natural(event["intro_stream_ordinal"], "intro_stream_ordinal", MAX_STREAM_ORDINAL)
        bank, result = event["bank"], event["access_result"]
        if order <= prior or stream == 0 or bank not in BANKS or result not in RESULTS:
            raise ValueError("trace event has an invalid order or categorical value")
        identity = (stream, bank, result)
        if identity in seen:
            raise ValueError("trace repeats an identical stream-access result")
        seen.add(identity)
        clean.append({"observation_order": order, "intro_stream_ordinal": stream,
                      "bank": bank, "access_result": result})
        prior = order
    return {"format": OUTPUT_FORMAT, "fresh_isolated": True, "events": clean}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    absolute = pathlib.Path(os.path.abspath(path))
    current = pathlib.Path(absolute.anchor)
    for part in absolute.parts[1:]:
        current /= part
        try:
            metadata = os.lstat(current)
        except FileNotFoundError:
            break
        if stat.S_ISLNK(metadata.st_mode):
            raise ValueError(f"{label} must not contain a symlink")
    resolved = absolute.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _parent_fd(path: pathlib.Path, label: str) -> tuple[int, str]:
    try:
        return os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW), path.name
    except OSError as error:
        raise ValueError(f"{label} parent must be a real directory") from error


def _read(path: pathlib.Path) -> Any:
    parent, name = _parent_fd(path, "input")
    try:
        descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=parent)
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_PRIVATE_RECORD_BYTES:
                raise ValueError("input must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
                return json.load(stream)
        finally:
            os.close(descriptor)
    finally:
        os.close(parent)


def _write_new(path: pathlib.Path, record: dict[str, Any]) -> None:
    parent, name = _parent_fd(path, "output")
    try:
        descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600, dir_fd=parent)
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
                json.dump(record, stream, separators=(",", ":"))
                stream.write("\n")
        finally:
            os.close(descriptor)
    finally:
        os.close(parent)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        source = _outside_repository(args.input, "input")
        output = _outside_repository(args.output, "output")
        if source == output or output.exists():
            raise ValueError("input and new output must differ")
        record = sanitize_trace(_read(source))
        _write_new(output, record)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(record['events'])} source-free intro-audio access records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
