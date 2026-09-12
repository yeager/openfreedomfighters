#!/usr/bin/env python3
"""Sanitize a private, source-free startup coordinator pass-selection trace.

The startup picture boundary deliberately consumes an already-selected root,
camera/view and renderer pass.  This tool is the private observation gate for
that producer boundary.  It accepts categorical relations only: no executable
or game data is opened, and no address, identity, path, text, byte, timing or
image field can enter the emitted record.
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
INPUT_FORMAT = "off.startup-coordinator-pass-selection.raw/v1"
OUTPUT_FORMAT = "off.startup-coordinator-pass-selection/v1"
MAX_EVENTS = 1024
MAX_CALLBACK_ORDINAL = 65535
MAX_PRIVATE_RECORD_BYTES = 8 * 1024 * 1024

_PHASES = ("coordinator_entry", "root_selection", "camera_view", "pass_context", "delivery", "completion", "failure")
_PHASE_RANK = {phase: rank for rank, phase in enumerate(_PHASES)}
_COORDINATOR_STATES = frozenset(("not_entered", "entered", "failed"))
_ROOT_SELECTIONS = frozenset(("not_attempted", "candidate", "selected", "rejected", "failed"))
_CAMERA_VIEWS = frozenset(("not_attempted", "disabled", "enabled", "rejected", "failed"))
_PASS_CONTEXTS = frozenset(("not_attempted", "resolved", "rejected", "failed"))
_DELIVERIES = frozenset(("not_attempted", "delivered", "failed"))
_OUTCOMES = frozenset(("success", "failure"))
_EXTERNAL_SERVICES = frozenset(("not_entered", "entered"))


def _exact(record: dict[str, Any], expected: frozenset[str]) -> None:
    if frozenset(record) != expected:
        raise ValueError("trace record has an unsupported field")


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or value < 0 or value > maximum:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value


def _enum(value: Any, label: str, allowed: frozenset[str] | tuple[str, ...]) -> str:
    if value not in allowed:
        raise ValueError(f"{label} has an unsupported value")
    return value


def _validate(event: dict[str, Any]) -> None:
    phase = event["phase"]
    coordinator = event["coordinator"]
    root = event["root_selection"]
    camera = event["camera_view"]
    context = event["pass_context"]
    delivery = event["delivery"]
    outcome = event["outcome"]

    if coordinator == "entered" and phase == "coordinator_entry":
        if root != "not_attempted" or camera != "not_attempted" or context != "not_attempted" or delivery != "not_attempted":
            raise ValueError("coordinator entry cannot claim a later pass boundary")
    if coordinator == "not_entered" and phase != "coordinator_entry":
        raise ValueError("later phases require coordinator entry")
    if root in ("candidate", "selected", "rejected"):
        if coordinator != "entered" or phase not in ("root_selection", "camera_view", "pass_context", "delivery", "completion", "failure"):
            raise ValueError("root selection requires entered coordinator")
    if camera in ("disabled", "enabled", "rejected"):
        if root != "selected" or phase not in ("camera_view", "pass_context", "delivery", "completion", "failure"):
            raise ValueError("camera/view result requires selected root")
    if context in ("resolved", "rejected"):
        if root != "selected" or camera != "enabled" or phase not in ("pass_context", "delivery", "completion", "failure"):
            raise ValueError("pass context requires selected root and enabled camera/view")
    if delivery == "delivered":
        if root != "selected" or camera != "enabled" or context != "resolved":
            raise ValueError("delivery requires a complete selected pass")
        if phase not in ("delivery", "completion") or outcome != "success":
            raise ValueError("delivery requires its phase and success")
    if delivery == "failed":
        if phase not in ("delivery", "failure") or outcome != "failure":
            raise ValueError("failed delivery requires failure")
    if phase == "completion":
        if delivery != "delivered" or outcome != "success":
            raise ValueError("completion requires delivered selected pass")
    if phase == "failure":
        if outcome != "failure":
            raise ValueError("failure phase requires failure outcome")
        if not (coordinator == "failed" or root in ("rejected", "failed") or
                camera in ("rejected", "failed") or context in ("rejected", "failed") or
                delivery == "failed"):
            raise ValueError("failure outcome requires a failed structural boundary")
    if outcome == "failure" and phase != "failure" and delivery != "failed":
        raise ValueError("failure outcome requires failure phase or failed delivery")


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the sole allowed startup coordinator structural trace."""
    if not isinstance(raw, dict):
        raise ValueError("trace must be a JSON object")
    _exact(raw, frozenset(("format", "events")))
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 1024 events")

    permitted = frozenset(("observation_order", "phase", "callback_ordinal", "coordinator",
                           "root_selection", "camera_view", "pass_context", "delivery",
                           "outcome", "external_service"))
    clean_events: list[dict[str, Any]] = []
    previous_order = -1
    previous_phase = -1
    callback_ordinal: int | None = None
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("trace event must be a JSON object")
        _exact(event, permitted)
        order = _natural(event["observation_order"], "observation_order", MAX_EVENTS)
        if order <= previous_order:
            raise ValueError("observation_order must be strictly increasing")
        previous_order = order
        phase = _enum(event["phase"], "phase", _PHASES)
        if _PHASE_RANK[phase] < previous_phase:
            raise ValueError("phase order cannot move backward")
        previous_phase = _PHASE_RANK[phase]
        callback = _natural(event["callback_ordinal"], "callback_ordinal", MAX_CALLBACK_ORDINAL)
        if callback_ordinal is None:
            callback_ordinal = callback
        elif callback != callback_ordinal:
            raise ValueError("one coordinator trace must use one observer-local callback ordinal")
        clean = {
            "observation_order": order,
            "phase": phase,
            "callback_ordinal": callback,
            "coordinator": _enum(event["coordinator"], "coordinator", _COORDINATOR_STATES),
            "root_selection": _enum(event["root_selection"], "root_selection", _ROOT_SELECTIONS),
            "camera_view": _enum(event["camera_view"], "camera_view", _CAMERA_VIEWS),
            "pass_context": _enum(event["pass_context"], "pass_context", _PASS_CONTEXTS),
            "delivery": _enum(event["delivery"], "delivery", _DELIVERIES),
            "outcome": _enum(event["outcome"], "outcome", _OUTCOMES),
            "external_service": _enum(event["external_service"], "external_service", _EXTERNAL_SERVICES),
        }
        _validate(clean)
        clean_events.append(clean)
    return {"format": OUTPUT_FORMAT, "events": clean_events}


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
    """Read one bounded regular private input without following its final entry."""
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
    """Create an owner-only private output without replacing an existing entry."""
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
    parser.add_argument("input", type=pathlib.Path, help="private structural observer JSON")
    parser.add_argument("output", type=pathlib.Path, help="new private sanitized JSON path")
    args = parser.parse_args()
    try:
        input_path = _outside_repository(args.input, "input")
        output_path = _outside_repository(args.output, "output")
        if input_path == output_path:
            raise ValueError("input and output paths must differ")
        if output_path.exists():
            raise ValueError("refusing to overwrite an existing private trace")
        sanitized = sanitize_trace(_read_private_json_no_follow(input_path, "input"))
        _write_new_private_json_no_follow(output_path, sanitized)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(sanitized['events'])} source-free startup coordinator records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
