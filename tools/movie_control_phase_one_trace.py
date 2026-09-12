#!/usr/bin/env python3
"""Validate and redact a private, structural MovieControl phase-one trace.

The original process must be observed by a separately maintained private
instrument.  This utility never opens an executable, archive, memory dump, or
asset.  It accepts only the small, enumerated record format below and writes a
new JSON document outside the repository.  In particular, it deliberately has
no fields for addresses, offsets, symbols, strings, paths, bytes, or images.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.movie-control-phase-one.raw/v2"
OUTPUT_FORMAT = "off.movie-control-phase-one/v2"
MAX_EVENTS = 4096
MAX_CALLBACK_ORDINAL = 65535
MASK_LIMIT = (1 << 32) - 1

_OUTCOMES = frozenset(("success", "failure"))
_EXTERNAL_SERVICES = frozenset(("not_entered", "entered"))
_GLOBAL_LIFECYCLE_OUTCOMES = frozenset(("not_observed", "success", "failure"))


def _require_exact_keys(record: dict[str, Any], expected: frozenset[str]) -> None:
    if frozenset(record) != expected:
        raise ValueError("trace record has an unsupported field")


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or value < 0 or value > maximum:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value


def _boolean(value: Any, label: str) -> bool:
    if type(value) is not bool:
        raise ValueError(f"{label} must be boolean")
    return value


def _enum(value: Any, label: str, allowed: frozenset[str]) -> str:
    if value not in allowed:
        raise ValueError(f"{label} has an unsupported value")
    return value


def _validate_relations(event: dict[str, Any]) -> None:
    """Reject lifecycle claims that cannot describe one global pass."""
    entered = event["global_lifecycle_entered"]
    completed = event["global_lifecycle_completed"]
    lifecycle_outcome = event["global_lifecycle_outcome"]
    if not entered:
        if completed or lifecycle_outcome != "not_observed":
            raise ValueError("an unentered global lifecycle has no completion outcome")
    elif lifecycle_outcome == "not_observed":
        if completed:
            raise ValueError("a completed global lifecycle requires an outcome")
    elif lifecycle_outcome == "success":
        if not completed:
            raise ValueError("a successful global lifecycle must be complete")
    elif completed:
        raise ValueError("a failed global lifecycle cannot be complete")
    if event["phase_one_completed"] and not entered:
        raise ValueError("phase-one completion requires global lifecycle entry")


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the only permitted public/private trace representation.

    The two constructed-identity relations are booleans, rather than raw object
    identifiers.  Callback ordinal is an observer-local sequence number, never
    an instruction address or symbolic name.
    """
    if not isinstance(raw, dict):
        raise ValueError("trace must be a JSON object")
    _require_exact_keys(raw, frozenset(("format", "events")))
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 4096 events")

    permitted = frozenset((
        "dispatch_order", "phase", "callback_ordinal",
        "component_is_constructed", "owner_is_constructed_owner",
        "component_status_before", "component_status_after",
        "owner_status_before", "owner_status_after",
        "event_member_before", "event_member_after", "outcome",
        "external_service",
        "global_lifecycle_entered", "global_lifecycle_completed",
        "global_lifecycle_outcome", "ordinary_member_before",
        "ordinary_member_after", "phase_one_completed",
    ))
    sanitized: list[dict[str, Any]] = []
    prior_order = -1
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("trace event must be a JSON object")
        _require_exact_keys(event, permitted)
        order = _natural(event["dispatch_order"], "dispatch_order", MAX_EVENTS)
        if order <= prior_order:
            raise ValueError("dispatch_order must be strictly increasing")
        prior_order = order
        phase = _natural(event["phase"], "phase", 255)
        if phase != 1:
            raise ValueError("MovieControl phase-one trace may record phase 1 only")
        callback = _natural(event["callback_ordinal"], "callback_ordinal", MAX_CALLBACK_ORDINAL)
        outcome = event["outcome"]
        external = event["external_service"]
        if outcome not in _OUTCOMES or external not in _EXTERNAL_SERVICES:
            raise ValueError("trace event has an unsupported enumerated value")
        clean = {
            "dispatch_order": order,
            "phase": phase,
            "callback_ordinal": callback,
            "component_is_constructed": _boolean(event["component_is_constructed"], "component_is_constructed"),
            "owner_is_constructed_owner": _boolean(event["owner_is_constructed_owner"], "owner_is_constructed_owner"),
            "component_status_before": _natural(event["component_status_before"], "component_status_before", MASK_LIMIT),
            "component_status_after": _natural(event["component_status_after"], "component_status_after", MASK_LIMIT),
            "owner_status_before": _natural(event["owner_status_before"], "owner_status_before", MASK_LIMIT),
            "owner_status_after": _natural(event["owner_status_after"], "owner_status_after", MASK_LIMIT),
            "event_member_before": _boolean(event["event_member_before"], "event_member_before"),
            "event_member_after": _boolean(event["event_member_after"], "event_member_after"),
            "outcome": outcome,
            "external_service": external,
            "global_lifecycle_entered": _boolean(event["global_lifecycle_entered"], "global_lifecycle_entered"),
            "global_lifecycle_completed": _boolean(event["global_lifecycle_completed"], "global_lifecycle_completed"),
            "global_lifecycle_outcome": _enum(event["global_lifecycle_outcome"], "global_lifecycle_outcome", _GLOBAL_LIFECYCLE_OUTCOMES),
            "ordinary_member_before": _boolean(event["ordinary_member_before"], "ordinary_member_before"),
            "ordinary_member_after": _boolean(event["ordinary_member_after"], "ordinary_member_after"),
            "phase_one_completed": _boolean(event["phase_one_completed"], "phase_one_completed"),
        }
        _validate_relations(clean)
        sanitized.append(clean)
    return {"format": OUTPUT_FORMAT, "events": sanitized}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path,
                        help="private structural JSON emitted by the observer")
    parser.add_argument("output", type=pathlib.Path,
                        help="new private sanitized JSON path")
    args = parser.parse_args()
    try:
        input_path = _outside_repository(args.input, "input")
        output_path = _outside_repository(args.output, "output")
        if input_path == output_path:
            raise ValueError("input and output paths must differ")
        if output_path.exists():
            raise ValueError("refusing to overwrite an existing private trace")
        raw = json.loads(input_path.read_text(encoding="utf-8"))
        sanitized = sanitize_trace(raw)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(sanitized, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(sanitized['events'])} source-free phase-one records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
