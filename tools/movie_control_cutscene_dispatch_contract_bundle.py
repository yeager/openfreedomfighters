#!/usr/bin/env python3
"""Join source-free MovieControl dispatch success and failure observations.

The success input must be a repeated dispatch observation from
``movie_control_cutscene_dispatch_repeat_pair`` and the failure input must
already be sanitized by ``movie_control_cutscene_dispatch_trace``.  This tool
opens neither source material nor observer artifacts.  It revalidates both
formats, retains one matching structural route, and writes a new private
review bundle outside the repository.

The bundle is review evidence only.  It does not authorize runtime dispatch or
relax the fail-closed startup boundary.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any

import movie_control_cutscene_dispatch_repeat_pair as repeat_pair
import movie_control_cutscene_dispatch_trace as dispatch_trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
SUCCESS_FORMAT = repeat_pair.OUTPUT_FORMAT
FAILURE_FORMAT = dispatch_trace.OUTPUT_FORMAT
OUTPUT_FORMAT = "off.movie-control-cutscene-dispatch-contract-bundle/v1"

_PRECONDITION_FIELDS = (
    "movie_phase_one_completed", "movie_phase_two_completed",
    "component_status_before", "owner_status_before",
)


def _exact_keys(value: Any, expected: frozenset[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict) or frozenset(value) != expected:
        raise ValueError(f"{label} has an unsupported field")
    return value


def _validated_success_events(raw: Any) -> list[dict[str, Any]]:
    pair = _exact_keys(raw, frozenset(("format", "events")), "repeat-pair result")
    if pair["format"] != SUCCESS_FORMAT:
        raise ValueError("unrecognized repeat-pair format")
    return dispatch_trace.sanitize_trace({
        "format": dispatch_trace.INPUT_FORMAT, "events": pair["events"],
    })["events"]


def _validated_failure_events(raw: Any) -> list[dict[str, Any]]:
    trace = _exact_keys(raw, frozenset(("format", "events")), "failure trace")
    if trace["format"] != FAILURE_FORMAT:
        raise ValueError("unrecognized sanitized failure-trace format")
    return dispatch_trace.sanitize_trace({
        "format": dispatch_trace.INPUT_FORMAT, "events": trace["events"],
    })["events"]


def _is_source_bound(event: dict[str, Any]) -> bool:
    return (
        event["movie_component_is_constructed"] and
        event["movie_owner_is_constructed_owner"] and
        event["sequence_component_is_constructed"] and
        event["sequence_owner_is_constructed_owner"] and
        event["movie_phase_one_completed"] and
        event["handoff_sender_is_movie_owner"] and
        event["handoff_target_is_sequence_owner"]
    )


def _is_success_route(event: dict[str, Any]) -> bool:
    return (
        _is_source_bound(event) and event["event16_gate"] == "admitted" and
        event["handoff"] == "delivered" and
        event["delivery_mode"] == "synchronous" and
        event["player_activation"] == "started" and event["outcome"] == "success"
    )


def _is_matching_failure(event: dict[str, Any], success: dict[str, Any]) -> bool:
    return (
        _is_source_bound(event) and
        event["callback_ordinal"] == success["callback_ordinal"] and
        event["outcome"] == "failure" and
        event["handoff"] in ("failed", "attempted") and
        all(event[field] == success[field] for field in _PRECONDITION_FIELDS)
    )


def _is_successful_same_route(event: dict[str, Any], success: dict[str, Any]) -> bool:
    return (
        _is_source_bound(event) and event["event16_gate"] == "admitted" and
        event["callback_ordinal"] == success["callback_ordinal"] and
        event["outcome"] == "success"
    )


def sanitize_contract_bundle(success: Any, failure: Any) -> dict[str, Any]:
    """Return a review-only bundle for one successful and one failed route."""
    success_events = _validated_success_events(success)
    routes = [event for event in success_events if _is_success_route(event)]
    if len(routes) != 1:
        raise ValueError("repeat-pair result must contain exactly one successful source-bound route")
    candidate = routes[0]
    failure_events = _validated_failure_events(failure)
    matching = [event for event in failure_events if _is_matching_failure(event, candidate)]
    if len(matching) != 1:
        raise ValueError("failure trace must contain exactly one matching constructed failure route")
    if any(_is_successful_same_route(event, candidate) for event in failure_events):
        raise ValueError("failure trace also contains a successful matching route")
    return {"format": OUTPUT_FORMAT, "candidate": candidate, "failure": matching[0]}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("success", type=pathlib.Path, help="private dispatch repeat-pair JSON")
    parser.add_argument("failure", type=pathlib.Path, help="private sanitized dispatch failure JSON")
    parser.add_argument("output", type=pathlib.Path, help="new private review-only bundle JSON")
    args = parser.parse_args()
    try:
        success_path = _outside_repository(args.success, "success input")
        failure_path = _outside_repository(args.failure, "failure input")
        output_path = _outside_repository(args.output, "output")
        if len({success_path, failure_path, output_path}) != 3:
            raise ValueError("inputs and output paths must differ")
        if output_path.exists():
            raise ValueError("refusing to overwrite an existing private contract bundle")
        result = sanitize_contract_bundle(
            json.loads(success_path.read_text(encoding="utf-8")),
            json.loads(failure_path.read_text(encoding="utf-8")),
        )
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one source-free MovieControl dispatch contract bundle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
