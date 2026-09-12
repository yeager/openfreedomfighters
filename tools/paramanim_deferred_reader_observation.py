#!/usr/bin/env python3
"""Sanitize a private, source-free ParamAnim reader-boundary observation.

The observer may report only the bounded categories required by
``INTRO_DEFERRED_READER_COVERAGE.md``.  It must not read or retain retail
payloads, identifiers, strings, paths, assets, bytes, addresses, offsets,
symbols, or screenshots.  Input and output observations belong outside this
repository.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.paramanim-deferred-reader-observation.raw/v1"
OUTPUT_FORMAT = "off.paramanim-deferred-reader-observation/v1"
MAX_EVENTS = 64

_STAGES = ("deferred_preparation", "owner_reader", "component_reader",
           "later_callback", "failure")
_STAGE_RANK = {stage: rank for rank, stage in enumerate(_STAGES)}
_INPUT_FORMS = frozenset(("accepted_bounded", "rejected_malformed",
                          "rejected_unsupported"))
_TERMINALS = frozenset(("required", "rejected_missing", "rejected_premature"))
_DELIMITERS = frozenset(("required", "rejected_missing", "rejected_duplicate",
                         "rejected_before_terminal"))
_TRAILING = frozenset(("accepted_none_only", "rejected_present"))
_WRITES = frozenset(("not_observed", "owner_reader", "component_reader"))
_READER_PREREQUISITES = frozenset((
    "not_observed", "preparation_before_owner_reader",
    "preparation_and_owner_reader_before_component_reader",
))
_PRESERVATION = frozenset(("not_observed", "preserved", "rejected"))
_OWNERSHIP = frozenset(("not_observed", "reader_local", "owner_local"))
_REENTRY = frozenset(("not_observed", "rejected_duplicate", "idempotent"))
_ROLLBACK = frozenset(("not_observed", "rolled_back", "no_write"))
_CONSUMERS = frozenset(("none", "later_callback"))
_OUTCOMES = frozenset(("success", "failure"))
_SIDE_EFFECTS = frozenset(("none",))
_FIELDS = frozenset((
    "observation_order", "stage", "owner_input_form", "component_input_form",
    "terminal_rule", "attachment_delimiter_rule", "trailing_bytes_policy",
    "destination_write", "reader_prerequisite", "raw_value_preservation", "ownership",
    "duplicate_reentry", "failure_rollback", "later_consumer", "outcome",
    "side_effect",
))


def _exact(record: dict[str, Any], expected: frozenset[str]) -> None:
    if frozenset(record) != expected:
        raise ValueError("observation record has an unsupported field")


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value


def _enum(value: Any, label: str, allowed: frozenset[str] | tuple[str, ...]) -> str:
    if value not in allowed:
        raise ValueError(f"{label} has an unsupported category")
    return value


def _validate(event: dict[str, Any]) -> None:
    rejected = (event["owner_input_form"] != "accepted_bounded" or
                event["component_input_form"] != "accepted_bounded")
    if not rejected:
        if (event["terminal_rule"] != "required" or
                event["attachment_delimiter_rule"] != "required" or
                event["trailing_bytes_policy"] != "accepted_none_only"):
            raise ValueError("accepted input requires the complete bounded grammar")
    if rejected:
        if (event["outcome"] != "failure" or event["destination_write"] != "not_observed" or
                event["failure_rollback"] != "no_write" or event["side_effect"] != "none"):
            raise ValueError("rejected input must fail before mutation or side effects")
    if event["destination_write"] != "not_observed":
        if event["stage"] != event["destination_write"]:
            raise ValueError("destination write must occur at its declared reader boundary")
        expected_prerequisite = (
            "preparation_before_owner_reader" if event["destination_write"] == "owner_reader"
            else "preparation_and_owner_reader_before_component_reader")
        if event["reader_prerequisite"] != expected_prerequisite:
            raise ValueError("a write requires its complete deferred-reader ordering evidence")
        if event["outcome"] != "success" or event["raw_value_preservation"] != "preserved":
            raise ValueError("a write requires successful raw-value preservation")
        if event["ownership"] == "not_observed" or event["duplicate_reentry"] == "not_observed":
            raise ValueError("a write requires ownership and re-entry evidence")
    elif (event["reader_prerequisite"] != "not_observed" or
          event["raw_value_preservation"] == "preserved" or event["ownership"] != "not_observed"):
        raise ValueError("state contract cannot be claimed without a write")
    if event["later_consumer"] == "later_callback":
        if event["stage"] != "later_callback" or event["outcome"] != "success":
            raise ValueError("a later consumer requires a successful later-callback observation")
    if event["stage"] == "failure" and event["outcome"] != "failure":
        raise ValueError("failure stage requires failure outcome")
    # A rollback is meaningful only after an accepted reader path has acquired
    # state.  Rejected grammar must instead report the stricter no-write
    # outcome above.  Keeping this distinction structural prevents a later
    # admission review from treating a pre-mutation rejection as destination
    # rollback evidence.
    if event["failure_rollback"] == "rolled_back":
        if (event["stage"] != "failure" or event["outcome"] != "failure" or
                event["owner_input_form"] != "accepted_bounded" or
                event["component_input_form"] != "accepted_bounded" or
                event["destination_write"] != "not_observed" or
                event["side_effect"] != "none"):
            raise ValueError("rollback requires an accepted post-write failure without side effects")
    elif (event["failure_rollback"] == "no_write" and
          event["outcome"] != "failure"):
        raise ValueError("no-write rollback requires a failed observation")


def sanitize_observation(raw: Any) -> dict[str, Any]:
    """Return the sole permitted source-free ParamAnim observation format."""
    if not isinstance(raw, dict):
        raise ValueError("observation must be a JSON object")
    _exact(raw, frozenset(("format", "events")))
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized observation format")
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("observation must contain between one and 64 events")
    clean_events: list[dict[str, Any]] = []
    prior_order = -1
    prior_stage = -1
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("observation event must be a JSON object")
        _exact(event, _FIELDS)
        order = _natural(event["observation_order"], "observation_order", MAX_EVENTS)
        if order <= prior_order:
            raise ValueError("observation_order must be strictly increasing")
        prior_order = order
        stage = _enum(event["stage"], "stage", _STAGES)
        if _STAGE_RANK[stage] < prior_stage:
            raise ValueError("stage order cannot move backward")
        prior_stage = _STAGE_RANK[stage]
        clean = {
            "observation_order": order, "stage": stage,
            "owner_input_form": _enum(event["owner_input_form"], "owner_input_form", _INPUT_FORMS),
            "component_input_form": _enum(event["component_input_form"], "component_input_form", _INPUT_FORMS),
            "terminal_rule": _enum(event["terminal_rule"], "terminal_rule", _TERMINALS),
            "attachment_delimiter_rule": _enum(event["attachment_delimiter_rule"], "attachment_delimiter_rule", _DELIMITERS),
            "trailing_bytes_policy": _enum(event["trailing_bytes_policy"], "trailing_bytes_policy", _TRAILING),
            "destination_write": _enum(event["destination_write"], "destination_write", _WRITES),
            "reader_prerequisite": _enum(event["reader_prerequisite"], "reader_prerequisite", _READER_PREREQUISITES),
            "raw_value_preservation": _enum(event["raw_value_preservation"], "raw_value_preservation", _PRESERVATION),
            "ownership": _enum(event["ownership"], "ownership", _OWNERSHIP),
            "duplicate_reentry": _enum(event["duplicate_reentry"], "duplicate_reentry", _REENTRY),
            "failure_rollback": _enum(event["failure_rollback"], "failure_rollback", _ROLLBACK),
            "later_consumer": _enum(event["later_consumer"], "later_consumer", _CONSUMERS),
            "outcome": _enum(event["outcome"], "outcome", _OUTCOMES),
            "side_effect": _enum(event["side_effect"], "side_effect", _SIDE_EFFECTS),
        }
        _validate(clean)
        clean_events.append(clean)
    return {"format": OUTPUT_FORMAT, "events": clean_events}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path, help="private structural JSON from the observer")
    parser.add_argument("output", type=pathlib.Path, help="new private sanitized JSON path")
    args = parser.parse_args()
    try:
        input_path = _outside_repository(args.input, "input")
        output_path = _outside_repository(args.output, "output")
        if input_path == output_path:
            raise ValueError("input and output paths must differ")
        if output_path.exists():
            raise ValueError("refusing to overwrite an existing private observation")
        sanitized = sanitize_observation(json.loads(input_path.read_text(encoding="utf-8")))
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(sanitized, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(sanitized['events'])} source-free ParamAnim observation records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
