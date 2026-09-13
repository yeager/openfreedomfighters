#!/usr/bin/env python3
"""Validate one private, source-free soundtrack cue comparison trace.

The external observer owns all decoding and comparison.  This boundary keeps
only opaque cue and album ordinals plus categorical comparison results.  It
cannot carry titles, paths, audio samples, durations, hashes, addresses, or
game data.
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
INPUT_FORMAT = "off.soundtrack-cue-observation.raw/v1"
OUTPUT_FORMAT = "off.soundtrack-cue-observation/v1"
MAX_EVENTS = 256
MAX_FILE_BYTES = 256 * 1024
_FORMATS = frozenset(("flac", "mp3"))
_OUTCOMES = frozenset(("verified-match", "rejected"))
_RELATIONS = frozenset(("exact", "loop-compatible", "not-equivalent"))


def _exact(value: Any, fields: frozenset[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict) or frozenset(value) != fields:
        raise ValueError(f"{label} has an unsupported field")
    return value


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(f"{label} must be a bounded unsigned integer")
    return value


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the only permitted source-free cue-comparison record."""
    trace = _exact(raw, frozenset(("format", "method_version",
                                   "verified_data_manifest_fingerprint", "platform",
                                   "architecture", "events")), "trace")
    if trace["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    if type(trace["method_version"]) is not int or trace["method_version"] != 1:
        raise ValueError("unsupported observation method")
    fingerprint = trace["verified_data_manifest_fingerprint"]
    if (not isinstance(fingerprint, str) or len(fingerprint) != 64 or
            any(c not in "0123456789abcdef" for c in fingerprint)):
        raise ValueError("manifest fingerprint must be a lowercase SHA-256")
    if trace["platform"] not in ("windows", "linux", "macos"):
        raise ValueError("unsupported observation platform")
    if trace["architecture"] not in ("x86", "x86_64", "arm64"):
        raise ValueError("unsupported observation architecture")
    events = trace["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain bounded events")
    fields = frozenset(("observation_order", "cue_token", "album_ordinal",
                        "format", "comparison_outcome", "timing_relation"))
    result: list[dict[str, Any]] = []
    prior = -1
    tokens: set[int] = set()
    editions: set[tuple[int, str]] = set()
    for item in events:
        event = _exact(item, fields, "trace event")
        order = _natural(event["observation_order"], "observation_order", MAX_EVENTS)
        token = _natural(event["cue_token"], "cue_token", (1 << 64) - 1)
        album = _natural(event["album_ordinal"], "album_ordinal", 255)
        if order <= prior or token == 0 or album == 0:
            raise ValueError("trace event ordering or opaque identity is invalid")
        if token in tokens or (album, event["format"]) in editions:
            raise ValueError("trace repeats a cue token or soundtrack edition")
        outcome, relation = event["comparison_outcome"], event["timing_relation"]
        if event["format"] not in _FORMATS or outcome not in _OUTCOMES or relation not in _RELATIONS:
            raise ValueError("trace event has an unsupported categorical value")
        if (outcome == "verified-match") != (relation in ("exact", "loop-compatible")):
            raise ValueError("comparison outcome and timing relation disagree")
        prior = order
        tokens.add(token)
        editions.add((album, event["format"]))
        result.append({"observation_order": order, "cue_token": token,
                       "album_ordinal": album, "format": event["format"],
                       "comparison_outcome": outcome, "timing_relation": relation})
    return {"format": OUTPUT_FORMAT, "method_version": 1,
            "verified_data_manifest_fingerprint": fingerprint, "platform": trace["platform"],
            "architecture": trace["architecture"], "events": result}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _open_parent(path: pathlib.Path, label: str) -> tuple[int, str]:
    try:
        descriptor = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    except OSError as error:
        raise ValueError(f"{label} parent must be a real existing directory") from error
    if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise ValueError(f"{label} parent must be a real existing directory")
    return descriptor, path.name


def _read(path: pathlib.Path, label: str) -> Any:
    directory, name = _open_parent(path, label)
    try:
        descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=directory)
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_FILE_BYTES:
                raise ValueError(f"{label} must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
                return json.load(stream)
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def _write_new(path: pathlib.Path, record: dict[str, Any]) -> None:
    directory, name = _open_parent(path, "output")
    try:
        descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600, dir_fd=directory)
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
                json.dump(record, stream, indent=2)
                stream.write("\n")
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        input_path, output_path = (_outside_repository(args.input, "input"),
                                   _outside_repository(args.output, "output"))
        if input_path == output_path or output_path.exists():
            raise ValueError("input and new output must differ")
        record = sanitize_trace(_read(input_path, "input"))
        _write_new(output_path, record)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(record['events'])} source-free soundtrack cue observations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
