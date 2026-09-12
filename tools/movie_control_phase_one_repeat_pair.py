#!/usr/bin/env python3
"""Validate a repeated, source-free MovieControl phase-one observation pair.

The two inputs must already have passed ``movie_control_phase_one_trace.py``.
This utility does not open or inspect an executable, asset, archive, dump,
log, screenshot, or raw observer record.  It keeps all inputs and its newly
written result outside the repository, and retains only the one identical
structural candidate established by both fresh-process observations.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any

import movie_control_phase_one_trace as phase_one_trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = phase_one_trace.OUTPUT_FORMAT
OUTPUT_FORMAT = "off.movie-control-phase-one-repeat-pair/v1"

_EFFECT_FIELDS = (
    "component_status_before", "component_status_after",
    "owner_status_before", "owner_status_after",
    "event_member_before", "event_member_after", "external_service",
    "ordinary_member_before", "ordinary_member_after",
)


def _validated_sanitized_trace(raw: Any) -> dict[str, Any]:
    """Revalidate one already-sanitized trace without widening its schema."""
    if not isinstance(raw, dict) or frozenset(raw) != frozenset(("format", "events")):
        raise ValueError("sanitized trace must contain only format and events")
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized sanitized trace format")
    # The phase-one sanitizer is the sole schema authority.  Replaying its
    # validation with the raw-format marker is safe because both formats have
    # the same deliberately structural event body.
    return phase_one_trace.sanitize_trace({
        "format": phase_one_trace.INPUT_FORMAT,
        "events": raw["events"],
    })


def _is_completed_candidate(event: dict[str, Any]) -> bool:
    return (
        event["component_is_constructed"]
        and event["owner_is_constructed_owner"]
        and event["global_lifecycle_entered"]
        and event["global_lifecycle_completed"]
        and event["global_lifecycle_outcome"] == "success"
        and event["phase_one_completed"]
        and event["outcome"] == "success"
    )


def _single_completed_candidate(trace: dict[str, Any]) -> dict[str, Any]:
    candidates = [event for event in trace["events"] if _is_completed_candidate(event)]
    if len(candidates) != 1:
        raise ValueError("each observation must contain exactly one completed constructed candidate")
    return candidates[0]


def _candidate_relation(event: dict[str, Any]) -> dict[str, Any]:
    """Return the complete source-free candidate/effect relation."""
    return {
        "dispatch_order": event["dispatch_order"],
        "callback_ordinal": event["callback_ordinal"],
        "component_is_constructed": event["component_is_constructed"],
        "owner_is_constructed_owner": event["owner_is_constructed_owner"],
        "global_lifecycle_entered": event["global_lifecycle_entered"],
        "global_lifecycle_completed": event["global_lifecycle_completed"],
        "global_lifecycle_outcome": event["global_lifecycle_outcome"],
        "phase_one_completed": event["phase_one_completed"],
        "outcome": event["outcome"],
        **{field: event[field] for field in _EFFECT_FIELDS},
    }


def sanitize_repeat_pair(first: Any, second: Any) -> dict[str, Any]:
    """Return one verified candidate only when both sanitized runs agree exactly."""
    first_candidate = _candidate_relation(_single_completed_candidate(_validated_sanitized_trace(first)))
    second_candidate = _candidate_relation(_single_completed_candidate(_validated_sanitized_trace(second)))
    if first_candidate != second_candidate:
        raise ValueError("repeat observations disagree on the candidate/effect relation")
    return {"format": OUTPUT_FORMAT, "candidate": first_candidate}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first", type=pathlib.Path,
                        help="first private sanitized JSON observation")
    parser.add_argument("second", type=pathlib.Path,
                        help="second private sanitized JSON observation")
    parser.add_argument("output", type=pathlib.Path,
                        help="new private repeat-pair JSON path")
    args = parser.parse_args()
    try:
        first_path = _outside_repository(args.first, "first input")
        second_path = _outside_repository(args.second, "second input")
        output_path = _outside_repository(args.output, "output")
        if len({first_path, second_path, output_path}) != 3:
            raise ValueError("inputs and output paths must differ")
        if output_path.exists():
            raise ValueError("refusing to overwrite an existing private trace")
        result = sanitize_repeat_pair(
            json.loads(first_path.read_text(encoding="utf-8")),
            json.loads(second_path.read_text(encoding="utf-8")),
        )
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one source-free MovieControl phase-one repeat-pair candidate")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
