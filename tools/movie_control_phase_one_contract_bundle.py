#!/usr/bin/env python3
"""Join reviewed source-free MovieControl phase-one success and failure evidence.

The success input must be the output of ``movie_control_phase_one_repeat_pair``;
the failure input must already have been sanitized by
``movie_control_phase_one_trace``.  This tool neither opens nor identifies an
executable, archive, asset, dump, log, screenshot, or game installation.  It
revalidates both small structural formats, requires a failure of the same
observer-local callback under the same pre-callback state, and writes one new
private result outside the repository.

The resulting bundle is evidence for human review only.  It is not a native
implementation contract and never authorizes normal startup to run phase one.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any

import movie_control_phase_one_repeat_pair as repeat_pair
import movie_control_phase_one_trace as phase_one_trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
SUCCESS_FORMAT = repeat_pair.OUTPUT_FORMAT
FAILURE_FORMAT = phase_one_trace.OUTPUT_FORMAT
OUTPUT_FORMAT = "off.movie-control-phase-one-contract-bundle/v1"

_CANDIDATE_FIELD_ORDER = (
    "dispatch_order", "callback_ordinal", "component_is_constructed",
    "owner_is_constructed_owner", "global_lifecycle_entered",
    "global_lifecycle_completed", "global_lifecycle_outcome",
    "phase_one_completed", "outcome", "component_status_before",
    "component_status_after", "owner_status_before", "owner_status_after",
    "event_member_before", "event_member_after", "external_service",
    "ordinary_member_before", "ordinary_member_after",
)
_CANDIDATE_FIELDS = frozenset(_CANDIDATE_FIELD_ORDER)

_PRECONDITION_FIELDS = (
    "component_status_before", "owner_status_before", "event_member_before",
    "ordinary_member_before",
)


def _exact_keys(value: Any, expected: frozenset[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict) or frozenset(value) != expected:
        raise ValueError(f"{label} has an unsupported field")
    return value


def _validated_success_candidate(raw: Any) -> dict[str, Any]:
    """Revalidate the repeat-pair result without trusting its producer."""
    bundle = _exact_keys(raw, frozenset(("format", "candidate")), "repeat-pair result")
    if bundle["format"] != SUCCESS_FORMAT:
        raise ValueError("unrecognized repeat-pair format")
    candidate = _exact_keys(bundle["candidate"], _CANDIDATE_FIELDS, "repeat-pair candidate")
    # The phase-one sanitizer remains the schema authority for all retained
    # fields.  The pair output omits only the constant phase marker.
    event = phase_one_trace.sanitize_trace({
        "format": phase_one_trace.INPUT_FORMAT,
        "events": [{**candidate, "phase": 1}],
    })["events"][0]
    if not repeat_pair._is_completed_candidate(event):
        raise ValueError("repeat-pair candidate is not a completed success")
    return candidate


def _validated_failure_events(raw: Any) -> list[dict[str, Any]]:
    """Accept only an already-sanitized phase-one trace."""
    trace = _exact_keys(raw, frozenset(("format", "events")), "failure trace")
    if trace["format"] != FAILURE_FORMAT:
        raise ValueError("unrecognized sanitized failure-trace format")
    return phase_one_trace.sanitize_trace({
        "format": phase_one_trace.INPUT_FORMAT,
        "events": trace["events"],
    })["events"]


def _is_matching_failure(event: dict[str, Any], candidate: dict[str, Any]) -> bool:
    return (
        event["component_is_constructed"]
        and event["owner_is_constructed_owner"]
        and event["dispatch_order"] == candidate["dispatch_order"]
        and event["callback_ordinal"] == candidate["callback_ordinal"]
        and event["global_lifecycle_entered"]
        and not event["global_lifecycle_completed"]
        and event["global_lifecycle_outcome"] == "failure"
        and not event["phase_one_completed"]
        and event["outcome"] == "failure"
        and all(event[field] == candidate[field] for field in _PRECONDITION_FIELDS)
    )


def _failure_relation(event: dict[str, Any]) -> dict[str, Any]:
    """Keep only the existing source-free structural event fields."""
    return {field: event[field] for field in _CANDIDATE_FIELD_ORDER}


def sanitize_contract_bundle(success: Any, failure: Any) -> dict[str, Any]:
    """Return a review-only bundle for one repeated success and one failure path."""
    candidate = _validated_success_candidate(success)
    events = _validated_failure_events(failure)
    matching = [event for event in events if _is_matching_failure(event, candidate)]
    if len(matching) != 1:
        raise ValueError("failure trace must contain exactly one matching constructed failure")
    # A success for the same constructed callback in this deliberately failing
    # observation makes the observed branch ambiguous rather than failure-only.
    if any(
        event["component_is_constructed"] and event["owner_is_constructed_owner"]
        and event["dispatch_order"] == candidate["dispatch_order"]
        and event["callback_ordinal"] == candidate["callback_ordinal"]
        and event["outcome"] == "success"
        for event in events
    ):
        raise ValueError("failure trace also contains a successful matching callback")
    return {
        "format": OUTPUT_FORMAT,
        "candidate": candidate,
        "failure": _failure_relation(matching[0]),
    }


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("success", type=pathlib.Path,
                        help="private MovieControl phase-one repeat-pair JSON")
    parser.add_argument("failure", type=pathlib.Path,
                        help="private sanitized MovieControl phase-one failure JSON")
    parser.add_argument("output", type=pathlib.Path,
                        help="new private review-only contract-bundle JSON")
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
    print("wrote one source-free MovieControl phase-one contract bundle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
