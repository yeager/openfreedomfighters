#!/usr/bin/env python3
"""Sanitize one private, vector-free C03A map-transform observation.

The input is supplied by an external private observer.  It deliberately has
no scene identifiers, object names, scalar/vector values, matrices, bytes, or
runtime addresses.  The result records only the ordering of transform
relations needed to decide whether a later native implementation may be
admitted for review.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

INPUT_FORMAT = "off.scene-map-transform.raw/v1"
OUTPUT_FORMAT = "off.scene-map-transform/v1"
MAX_EVENTS = 64
MAX_CALLBACK_ORDINAL = 65535
MAX_PRIVATE_RECORD_BYTES = 8 * 1024 * 1024
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
STAGES = ("entry", "parent_relation", "local_relation", "world_relation", "render_boundary", "completion", "failure")
RANK = {value: index for index, value in enumerate(STAGES)}
VALUES = {
    "scene": {"not_entered", "entered", "active", "failed"},
    "parent_relation": {"not_observed", "absent", "ready"},
    "local_relation": {"not_observed", "absent", "ready"},
    "world_relation": {"not_observed", "resolved"},
    "render_boundary": {"not_entered", "consumed"},
    "outcome": {"pending", "success", "failure"},
}
FIELDS = frozenset(("observation_order", "stage", "callback_ordinal", *VALUES))


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value


def _validate(event: dict[str, Any]) -> None:
    stage = event["stage"]
    if stage not in RANK:
        raise ValueError("stage has an unsupported value")
    scene = event["scene"]
    parent, local = event["parent_relation"], event["local_relation"]
    world, render, outcome = event["world_relation"], event["render_boundary"], event["outcome"]
    if stage != "entry" and scene != "active":
        raise ValueError("later transform relations require an active scene")
    if parent in {"absent", "ready"} and RANK[stage] < RANK["parent_relation"]:
        raise ValueError("parent relation requires its phase")
    if local in {"absent", "ready"}:
        if RANK[stage] < RANK["local_relation"] or parent not in {"absent", "ready"}:
            raise ValueError("local relation requires a settled parent relation")
    if world == "resolved":
        if RANK[stage] < RANK["world_relation"] or parent not in {"absent", "ready"} or local not in {"absent", "ready"}:
            raise ValueError("world relation requires settled parent and local relations")
    if render == "consumed" and (RANK[stage] < RANK["render_boundary"] or world != "resolved"):
        raise ValueError("render consumption requires a resolved world relation")
    if stage == "completion" and not (scene == "active" and parent in {"absent", "ready"} and local in {"absent", "ready"} and world == "resolved" and render == "consumed" and outcome == "success"):
        raise ValueError("completion requires a consumed resolved transform")
    if stage == "failure":
        if outcome != "failure" or render == "consumed" or scene not in {"active", "failed"}:
            raise ValueError("failure must not claim successful render consumption")
    elif stage != "completion" and outcome != "pending":
        raise ValueError("only terminal stages may set an outcome")


def sanitize_trace(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != {"format", "events"} or raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 64 events")
    clean: list[dict[str, Any]] = []
    prior_order = prior_stage = -1
    callback: int | None = None
    for raw_event in events:
        if not isinstance(raw_event, dict) or frozenset(raw_event) != FIELDS:
            raise ValueError("trace record has an unsupported field")
        order = _natural(raw_event["observation_order"], "observation_order", MAX_EVENTS)
        stage = raw_event["stage"]
        if order <= prior_order:
            raise ValueError("observation_order must be strictly increasing")
        if stage not in RANK or RANK[stage] < prior_stage:
            raise ValueError("stage order cannot move backward")
        current = {"observation_order": order, "stage": stage,
                   "callback_ordinal": _natural(raw_event["callback_ordinal"], "callback_ordinal", MAX_CALLBACK_ORDINAL)}
        for field, permitted in VALUES.items():
            if raw_event[field] not in permitted:
                raise ValueError(f"{field} has an unsupported value")
            current[field] = raw_event[field]
        if callback is None:
            callback = current["callback_ordinal"]
        elif callback != current["callback_ordinal"]:
            raise ValueError("one trace must use one observer-local callback ordinal")
        _validate(current)
        clean.append(current)
        prior_order, prior_stage = order, RANK[stage]
    if clean[-1]["stage"] not in {"completion", "failure"}:
        raise ValueError("trace must have one terminal result")
    return {"format": OUTPUT_FORMAT, "events": clean}


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


def _open_parent_no_follow(path: pathlib.Path, label: str) -> tuple[int, str]:
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


def _read_private_json_no_follow(path: pathlib.Path, label: str) -> Any:
    directory, name = _open_parent_no_follow(path, label)
    try:
        try:
            descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=directory)
        except OSError as error:
            raise ValueError(f"{label} must be a bounded regular private file") from error
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_PRIVATE_RECORD_BYTES:
                raise ValueError(f"{label} must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
                return json.load(stream)
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def _write_new_private_json_no_follow(path: pathlib.Path, record: dict[str, Any]) -> None:
    directory, name = _open_parent_no_follow(path, "output")
    try:
        try:
            descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=directory)
        except OSError as error:
            raise ValueError("refusing to overwrite private output") from error
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
                json.dump(record, stream, separators=(",", ":"))
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
        input_path, output_path = _outside_repository(args.input, "input"), _outside_repository(args.output, "output")
        if input_path == output_path or output_path.exists():
            raise ValueError("input and new private output must differ")
        result = sanitize_trace(_read_private_json_no_follow(input_path, "input"))
        _write_new_private_json_no_follow(output_path, result)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(result['events'])} vector-free map-transform records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
