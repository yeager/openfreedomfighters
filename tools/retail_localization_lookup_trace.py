#!/usr/bin/env python3
"""Validate a private, source-free retail LOC lookup/formatting trace.

An independently maintained observer of an owned original installation may
record only structural lookup and formatting relations.  This utility does not
open game data, executables, dumps, logs, screenshots, or assets.  In
particular, a trace has no field for a lookup key, source text, formatted text,
address, path, byte sequence, image, or captured argument value.

The emitted catalog ordinal is an opaque ordinal in the user's already-private
catalog.  It is useful for checking that repeat observations agree; it is not
an implementation key and does not make a native lookup mapping available.
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
INPUT_FORMAT = "off.retail-localization-lookup.raw/v1"
OUTPUT_FORMAT = "off.retail-localization-lookup/v1"
MAX_EVENTS = 4096
MAX_ORDINAL = (1 << 63) - 1
MAX_ARGUMENTS = 32
MAX_TRACE_BYTES = 512 * 1024

_KEY_RELATIONS = frozenset(("new", "same-as-prior"))
_OUTCOMES = frozenset(("resolved", "missing", "failure"))
_RESULT_KINDS = frozenset(("catalog-value", "formatted-value", "no-value", "failure"))
_ARGUMENT_KINDS = frozenset(("boolean", "signed", "unsigned", "floating", "opaque-string"))


def _exact(record: dict[str, Any], expected: frozenset[str]) -> None:
    if frozenset(record) != expected:
        raise ValueError("trace record has an unsupported field")


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or value < 0 or value > maximum:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value


def _enum(value: Any, label: str, allowed: frozenset[str]) -> str:
    if value not in allowed:
        raise ValueError(f"{label} has an unsupported value")
    return value


def _optional_ordinal(value: Any, label: str) -> int | None:
    if value is None:
        return None
    return _natural(value, label, MAX_ORDINAL)


def _argument_kinds(value: Any) -> list[str]:
    if not isinstance(value, list) or len(value) > MAX_ARGUMENTS:
        raise ValueError("format_argument_kinds must be a bounded array")
    return [_enum(item, "format argument kind", _ARGUMENT_KINDS) for item in value]


def _validate(event: dict[str, Any]) -> None:
    outcome = event["lookup_outcome"]
    result = event["result_kind"]
    ordinal = event["catalog_ordinal"]
    arguments = event["format_argument_kinds"]
    if outcome == "resolved":
        if ordinal is None:
            raise ValueError("resolved lookup requires an opaque catalog ordinal")
        if result not in ("catalog-value", "formatted-value"):
            raise ValueError("resolved lookup requires a value result")
        if result == "catalog-value" and arguments:
            raise ValueError("unformatted catalog result cannot carry format arguments")
    else:
        if ordinal is not None:
            raise ValueError("unresolved lookup cannot name a catalog ordinal")
        if arguments:
            raise ValueError("unresolved lookup cannot carry format arguments")
        expected = "no-value" if outcome == "missing" else "failure"
        if result != expected:
            raise ValueError("unresolved lookup result does not match its outcome")
    if result == "formatted-value" and outcome != "resolved":
        raise ValueError("formatted result requires a resolved lookup")


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the only permitted structural LOC lookup trace."""
    if not isinstance(raw, dict):
        raise ValueError("trace must be a JSON object")
    _exact(raw, frozenset(("format", "events")))
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 4096 events")

    fields = frozenset((
        "observation_order", "call_ordinal", "lookup_key_relation",
        "catalog_ordinal", "lookup_outcome", "result_kind",
        "format_argument_kinds",
    ))
    cleaned: list[dict[str, Any]] = []
    prior_order = -1
    prior_call = -1
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("trace event must be a JSON object")
        _exact(event, fields)
        order = _natural(event["observation_order"], "observation_order", MAX_EVENTS)
        call = _natural(event["call_ordinal"], "call_ordinal", MAX_EVENTS)
        if order <= prior_order or call <= prior_call:
            raise ValueError("observation and call ordinals must be strictly increasing")
        relation = _enum(event["lookup_key_relation"], "lookup_key_relation", _KEY_RELATIONS)
        if relation == "same-as-prior" and not cleaned:
            raise ValueError("first lookup cannot be the same as a prior key")
        clean = {
            "observation_order": order,
            "call_ordinal": call,
            "lookup_key_relation": relation,
            "catalog_ordinal": _optional_ordinal(event["catalog_ordinal"], "catalog_ordinal"),
            "lookup_outcome": _enum(event["lookup_outcome"], "lookup_outcome", _OUTCOMES),
            "result_kind": _enum(event["result_kind"], "result_kind", _RESULT_KINDS),
            "format_argument_kinds": _argument_kinds(event["format_argument_kinds"]),
        }
        _validate(clean)
        prior_order, prior_call = order, call
        cleaned.append(clean)
    return {"format": OUTPUT_FORMAT, "events": cleaned}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _open_parent_no_follow(path: pathlib.Path, label: str) -> tuple[int, str]:
    """Bind a private protocol path to its already-existing real parent."""
    no_follow = getattr(os, "O_NOFOLLOW", None)
    directory = getattr(os, "O_DIRECTORY", None)
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


def _read_private_json(path: pathlib.Path, label: str) -> object:
    directory, name = _open_parent_no_follow(path, label)
    try:
        try:
            descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW,
                                 dir_fd=directory)
        except OSError as error:
            raise ValueError(f"{label} must be a regular private file") from error
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_TRACE_BYTES:
                raise ValueError(f"{label} must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
                return json.load(stream)
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def _write_new_private_json(path: pathlib.Path, record: object, label: str) -> None:
    directory, name = _open_parent_no_follow(path, label)
    try:
        try:
            descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                 0o600, dir_fd=directory)
        except OSError as error:
            raise ValueError(f"refusing to overwrite {label}") from error
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
    parser.add_argument("input", type=pathlib.Path, help="private structural JSON emitted by the observer")
    parser.add_argument("output", type=pathlib.Path, help="new private sanitized JSON path")
    args = parser.parse_args()
    try:
        input_path = _outside_repository(args.input, "input")
        output_path = _outside_repository(args.output, "output")
        if input_path == output_path:
            raise ValueError("input and output paths must differ")
        raw = _read_private_json(input_path, "input")
        sanitized = sanitize_trace(raw)
        _write_new_private_json(output_path, sanitized, "private trace")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(sanitized['events'])} source-free retail localization lookup records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
