#!/usr/bin/env python3
"""Sanitize one fresh, private intro-camera lifecycle observation.

This is deliberately a relation recorder, not a camera extractor: addresses,
identities, matrices, vectors, scalar values, images and timing never cross
this boundary.  An externally maintained observer supplies the raw record.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

INPUT_FORMAT = "off.intro-camera-lifecycle.raw/v1"
OUTPUT_FORMAT = "off.intro-camera-lifecycle/v1"
MAX_EVENTS = 64
MAX_CALLBACK_ORDINAL = 65535
MAX_PRIVATE_RECORD_BYTES = 8 * 1024 * 1024
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
STAGES = ("entry", "controller", "camera_owner", "transform", "view", "frame_delivery", "completion", "failure")
RANK = {stage: number for number, stage in enumerate(STAGES)}
VALUES = {
    "sequence": {"not_entered", "entered", "active", "failed"},
    "controller": {"not_observed", "entered", "ready", "failed"},
    "camera_owner": {"not_observed", "resolved", "rejected", "failed"},
    "transform": {"not_observed", "composed", "rejected", "failed"},
    "view": {"not_observed", "admitted", "rejected", "failed"},
    "frame_delivery": {"not_attempted", "delivered", "failed"},
    "outcome": {"pending", "success", "failure"},
}
FIELDS = frozenset(("observation_order", "stage", "callback_ordinal", *VALUES))


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value


def _validate(event: dict[str, Any]) -> None:
    stage = event["stage"]
    sequence, controller = event["sequence"], event["controller"]
    owner, transform, view = event["camera_owner"], event["transform"], event["view"]
    delivery, outcome = event["frame_delivery"], event["outcome"]
    if stage != "entry" and sequence != "active":
        raise ValueError("later camera phases require an active sequence")
    if controller in {"entered", "ready"} and (stage not in RANK or RANK[stage] < RANK["controller"]):
        raise ValueError("controller relation requires its phase")
    if owner in {"resolved", "rejected"} and (controller != "ready" or RANK[stage] < RANK["camera_owner"]):
        raise ValueError("camera owner requires a ready controller")
    if transform in {"composed", "rejected"} and (owner != "resolved" or RANK[stage] < RANK["transform"]):
        raise ValueError("transform requires a resolved camera owner")
    if view in {"admitted", "rejected"} and (transform != "composed" or RANK[stage] < RANK["view"]):
        raise ValueError("view requires a composed transform")
    if delivery == "delivered" and (view != "admitted" or stage not in {"frame_delivery", "completion"} or outcome != "success"):
        raise ValueError("frame delivery requires an admitted view and success")
    if delivery == "failed" and (stage not in {"frame_delivery", "failure"} or outcome != "failure"):
        raise ValueError("failed frame delivery requires failure")
    if stage == "completion" and not (sequence == "active" and controller == "ready" and owner == "resolved" and transform == "composed" and view == "admitted" and delivery == "delivered" and outcome == "success"):
        raise ValueError("completion requires delivered camera lifecycle")
    if stage == "failure":
        if outcome != "failure" or delivery == "delivered":
            raise ValueError("failure must not claim delivered frame")
        if not (sequence == "failed" or controller == "failed" or owner in {"rejected", "failed"} or transform in {"rejected", "failed"} or view in {"rejected", "failed"} or delivery == "failed"):
            raise ValueError("failure requires a rejected structural boundary")
    elif stage not in {"frame_delivery", "completion"} and outcome != "pending":
        raise ValueError("only delivery and terminal stages may report an outcome")


def sanitize_trace(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != {"format", "events"} or raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 64 events")
    clean: list[dict[str, Any]] = []
    previous_order = previous_stage = -1
    callback: int | None = None
    for record in events:
        if not isinstance(record, dict) or frozenset(record) != FIELDS:
            raise ValueError("trace record has an unsupported field")
        order = _natural(record["observation_order"], "observation_order", MAX_EVENTS)
        stage = record["stage"]
        if order <= previous_order or stage not in RANK or RANK[stage] < previous_stage:
            raise ValueError("trace order cannot move backward")
        current = {"observation_order": order, "stage": stage,
                   "callback_ordinal": _natural(record["callback_ordinal"], "callback_ordinal", MAX_CALLBACK_ORDINAL)}
        for field, allowed in VALUES.items():
            if record[field] not in allowed:
                raise ValueError(f"{field} has an unsupported value")
            current[field] = record[field]
        if callback is None:
            callback = current["callback_ordinal"]
        elif callback != current["callback_ordinal"]:
            raise ValueError("one trace must use one observer-local callback ordinal")
        _validate(current)
        clean.append(current); previous_order, previous_stage = order, RANK[stage]
    if clean[-1]["stage"] not in {"completion", "failure"}:
        raise ValueError("trace must have one terminal result")
    return {"format": OUTPUT_FORMAT, "events": clean}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    absolute = pathlib.Path(os.path.abspath(path)); current = pathlib.Path(absolute.anchor)
    for part in absolute.parts[1:]:
        current /= part
        try: metadata = os.lstat(current)
        except FileNotFoundError: break
        if stat.S_ISLNK(metadata.st_mode): raise ValueError(f"{label} must not contain a symlink")
    resolved = absolute.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT): raise ValueError(f"{label} must be outside the repository")
    return resolved


def _parent_fd(path: pathlib.Path, label: str) -> tuple[int, str]:
    try: fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    except OSError as error: raise ValueError(f"{label} parent must be a real directory") from error
    return fd, path.name


def _read_private_json_no_follow(path: pathlib.Path, label: str) -> Any:
    parent, name = _parent_fd(path, label)
    try:
        try: fd = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=parent)
        except OSError as error: raise ValueError(f"{label} must be a bounded regular private file") from error
        try:
            metadata = os.fstat(fd)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_PRIVATE_RECORD_BYTES: raise ValueError(f"{label} must be a bounded regular private file")
            with os.fdopen(fd, "r", encoding="utf-8", closefd=False) as stream: return json.load(stream)
        finally: os.close(fd)
    finally: os.close(parent)


def _write_new_private_json_no_follow(path: pathlib.Path, record: dict[str, Any]) -> None:
    parent, name = _parent_fd(path, "output")
    try:
        try: fd = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=parent)
        except OSError as error: raise ValueError("refusing to overwrite private output") from error
        try:
            with os.fdopen(fd, "w", encoding="utf-8", closefd=False) as stream: json.dump(record, stream, separators=(",", ":")); stream.write("\n")
        finally: os.close(fd)
    finally: os.close(parent)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("input", type=pathlib.Path); parser.add_argument("output", type=pathlib.Path); args = parser.parse_args()
    try:
        source, output = _outside_repository(args.input, "input"), _outside_repository(args.output, "output")
        if source == output or output.exists(): raise ValueError("input and new private output must differ")
        result = sanitize_trace(_read_private_json_no_follow(source, "input")); _write_new_private_json_no_follow(output, result)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr); return 1
    print(f"wrote {len(result['events'])} source-free intro-camera lifecycle records"); return 0


if __name__ == "__main__": raise SystemExit(main())
