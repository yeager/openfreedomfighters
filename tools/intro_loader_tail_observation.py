#!/usr/bin/env python3
"""Sanitize one private, source-free intro loader-tail observation.

The record proves ordering of concrete tail boundaries only.  It accepts no
game data, executable details, identities, addresses, paths, timing, text,
images, bytes, or hashes.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

INPUT_FORMAT = "off.intro-loader-tail.raw/v1"
OUTPUT_FORMAT = "off.intro-loader-tail/v1"
MAX_EVENTS = 64
MAX_CALLBACK_ORDINAL = 65535
MAX_PRIVATE_RECORD_BYTES = 8 * 1024 * 1024
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
STAGES = ("entry", "named_global", "renderer", "associations", "source_lease", "camera", "outer_scene", "between_scene", "finalization", "saved_resources", "completion", "failure")
RANK = {stage: index for index, stage in enumerate(STAGES)}
VALUES = {
    "tail": {"not_entered", "entered", "failed"},
    "named_global": {"not_present", "accepted", "rejected", "failed"},
    "renderer_payload": {"not_present", "consumed", "rejected", "failed"},
    "renderer_state": {"not_attempted", "selected", "restored", "failed"},
    "associations": {"not_present", "applied", "rejected", "failed"},
    "source_lease": {"not_attempted", "released", "failed"},
    "camera": {"not_attempted", "existing_zero", "fallback_registered", "rejected", "failed"},
    "outer_scene": {"not_attempted", "called", "failed"},
    "between_scene": {"not_attempted", "called", "failed"},
    "finalization": {"not_attempted", "called", "failed"},
    "spatial": {"not_attempted", "admitted", "failed"},
    "saved_0x4000": {"not_attempted", "applied", "failed"},
    "outcome": {"pending", "success", "failure"},
}
FIELDS = frozenset(("observation_order", "stage", "callback_ordinal", *VALUES))


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value


def _validate(event: dict[str, Any]) -> None:
    stage = event["stage"]
    if stage != "entry" and event["tail"] != "entered":
        raise ValueError("later loader-tail stages require an entered tail")
    if event["named_global"] in {"accepted", "rejected"} and RANK[stage] < RANK["named_global"]:
        raise ValueError("named-global result requires its stage")
    if event["renderer_payload"] in {"consumed", "rejected"} and RANK[stage] < RANK["renderer"]:
        raise ValueError("renderer payload result requires its stage")
    if event["renderer_state"] in {"selected", "restored"}:
        if event["renderer_payload"] != "consumed" or RANK[stage] < RANK["renderer"]:
            raise ValueError("renderer state requires a consumed renderer payload")
    if event["associations"] in {"applied", "rejected"} and RANK[stage] < RANK["associations"]:
        raise ValueError("association result requires its stage")
    if event["source_lease"] == "released" and RANK[stage] < RANK["source_lease"]:
        raise ValueError("source lease release requires its stage")
    if event["camera"] in {"existing_zero", "fallback_registered", "rejected"}:
        if event["source_lease"] != "released" or RANK[stage] < RANK["camera"]:
            raise ValueError("camera route requires released source lease")
    for field, prerequisite, phase in (("outer_scene", None, "outer_scene"), ("between_scene", "called", "between_scene"), ("finalization", "called", "finalization")):
        if event[field] == "called":
            if RANK[stage] < RANK[phase] or (prerequisite is not None and event["outer_scene" if field == "between_scene" else "between_scene"] != prerequisite):
                raise ValueError(f"{field} requires its ordered scene stage")
    if event["spatial"] == "admitted" and (event["finalization"] != "called" or RANK[stage] < RANK["saved_resources"]):
        raise ValueError("spatial admission requires scene finalization")
    if event["saved_0x4000"] == "applied" and (event["spatial"] != "admitted" or RANK[stage] < RANK["saved_resources"]):
        raise ValueError("saved-resource service requires spatial admission")
    if stage == "completion":
        required = (event["tail"] == "entered" and event["named_global"] in {"not_present", "accepted"}
                    and event["renderer_payload"] in {"not_present", "consumed"}
                    and (event["renderer_payload"] != "consumed" or event["renderer_state"] == "restored")
                    and event["associations"] in {"not_present", "applied"} and event["source_lease"] == "released"
                    and event["camera"] in {"existing_zero", "fallback_registered"} and event["outer_scene"] == "called"
                    and event["between_scene"] == "called" and event["finalization"] == "called"
                    and event["spatial"] == "admitted" and event["saved_0x4000"] == "applied" and event["outcome"] == "success")
        if not required:
            raise ValueError("completion requires every concrete loader-tail boundary")
    if stage == "failure":
        if event["outcome"] != "failure":
            raise ValueError("failure stage requires failure outcome")
    elif event["outcome"] != "pending" and stage != "completion":
        raise ValueError("only a terminal stage may report an outcome")


def sanitize_trace(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != {"format", "events"} or raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 64 events")
    clean: list[dict[str, Any]] = []
    prior_order = prior_stage = -1
    callback: int | None = None
    for record in events:
        if not isinstance(record, dict) or frozenset(record) != FIELDS:
            raise ValueError("trace record has an unsupported field")
        order = _natural(record["observation_order"], "observation_order", MAX_EVENTS)
        stage = record["stage"]
        if order <= prior_order or stage not in RANK or RANK[stage] < prior_stage:
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
        clean.append(current)
        prior_order, prior_stage = order, RANK[stage]
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
    try: descriptor = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    except OSError as error: raise ValueError(f"{label} parent must be a real directory") from error
    return descriptor, path.name


def _read_private_json_no_follow(path: pathlib.Path, label: str) -> Any:
    parent, name = _parent_fd(path, label)
    try:
        try: descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=parent)
        except OSError as error: raise ValueError(f"{label} must be a bounded regular private file") from error
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_PRIVATE_RECORD_BYTES: raise ValueError(f"{label} must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream: return json.load(stream)
        finally: os.close(descriptor)
    finally: os.close(parent)


def _write_new_private_json_no_follow(path: pathlib.Path, record: dict[str, Any]) -> None:
    parent, name = _parent_fd(path, "output")
    try:
        try: descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=parent)
        except OSError as error: raise ValueError("refusing to overwrite private output") from error
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream: json.dump(record, stream, separators=(",", ":")); stream.write("\n")
        finally: os.close(descriptor)
    finally: os.close(parent)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path); parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        source, output = _outside_repository(args.input, "input"), _outside_repository(args.output, "output")
        if source == output or output.exists(): raise ValueError("input and new private output must differ")
        result = sanitize_trace(_read_private_json_no_follow(source, "input")); _write_new_private_json_no_follow(output, result)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr); return 1
    print(f"wrote {len(result['events'])} source-free intro loader-tail records")
    return 0


if __name__ == "__main__": raise SystemExit(main())
