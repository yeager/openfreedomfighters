#!/usr/bin/env python3
"""Join two matching private soundtrack comparisons and one rejection trace."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

import soundtrack_cue_observation_trace as trace

REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
OUTPUT_FORMAT = "off.soundtrack-cue-evidence-bundle/v1"
MAX_FILE_BYTES = 256 * 1024


def _validated(value: Any) -> dict[str, Any]:
    if not isinstance(value, dict) or frozenset(value) != frozenset((
            "format", "method_version", "verified_data_manifest_fingerprint", "platform",
            "architecture", "events")) or value.get("format") != trace.OUTPUT_FORMAT:
        raise ValueError("input must be a sanitized soundtrack observation")
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                 **{key: value[key] for key in value if key != "format"}})


def sanitize_bundle(first: Any, second: Any, rejected: Any) -> dict[str, Any]:
    """Retain only reviewed, repeatable positive cue bindings and negative coverage."""
    left, right, negative = _validated(first), _validated(second), _validated(rejected)
    if left != right:
        raise ValueError("independent successful observations must agree exactly")
    metadata = ("method_version", "verified_data_manifest_fingerprint", "platform", "architecture")
    if any(tuple(item[key] for key in metadata) != tuple(left[key] for key in metadata)
           for item in (right, negative)):
        raise ValueError("observation metadata must agree")
    if any(event["comparison_outcome"] != "verified-match" for event in left["events"]):
        raise ValueError("successful observations may not retain a rejected candidate")
    rejected_events = [event for event in negative["events"] if event["comparison_outcome"] == "rejected"]
    if not rejected_events or any(event["comparison_outcome"] != "rejected" for event in negative["events"]):
        raise ValueError("negative observation must contain only rejected candidates")
    positive_keys = {(event["cue_token"], event["album_ordinal"], event["format"])
                     for event in left["events"]}
    negative_keys = {(event["cue_token"], event["album_ordinal"], event["format"])
                     for event in negative["events"]}
    if positive_keys & negative_keys:
        raise ValueError("positive and negative observations must be distinct")
    return {"format": OUTPUT_FORMAT, **{key: left[key] for key in metadata},
            "positive_repeat_count": 2, "negative_run_count": 1,
            "bindings": [{key: event[key] for key in ("cue_token", "album_ordinal", "format")}
                         for event in left["events"]],
            "negative_candidate_count": len(rejected_events)}


def _outside(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _read(path: pathlib.Path) -> Any:
    try:
        directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        descriptor = os.open(path.name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=directory)
    except OSError as error:
        raise ValueError("input must be a regular private file") from error
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode) or os.fstat(descriptor).st_size > MAX_FILE_BYTES:
            raise ValueError("input must be a bounded regular private file")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(descriptor)
        os.close(directory)


def _write(path: pathlib.Path, value: dict[str, Any]) -> None:
    directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        descriptor = os.open(path.name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600, dir_fd=directory)
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
                json.dump(value, stream, indent=2)
                stream.write("\n")
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first", type=pathlib.Path)
    parser.add_argument("second", type=pathlib.Path)
    parser.add_argument("rejected", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        paths = [_outside(getattr(args, key), key) for key in ("first", "second", "rejected", "output")]
        if len(set(paths)) != len(paths) or paths[-1].exists():
            raise ValueError("private inputs and new output must be distinct")
        result = sanitize_bundle(*(_read(path) for path in paths[:-1]))
        _write(paths[-1], result)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(result['bindings'])} source-free reviewed soundtrack cue bindings")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
