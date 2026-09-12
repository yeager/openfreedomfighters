#!/usr/bin/env python3
"""Validate a private, source-free MovieControl-to-player handoff trace.

This records the missing dispatcher boundary between an admitted MovieControl
ordinary event and the first-cut player.  A separately maintained private
observer supplies categorical relations only.  This utility never opens game
files, executables, dumps, logs, screenshots, or assets, and both trace files
must remain outside the repository.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.movie-control-cutscene-dispatch.raw/v1"
OUTPUT_FORMAT = "off.movie-control-cutscene-dispatch/v1"
MAX_EVENTS = 4096
MAX_CALLBACK_ORDINAL = 65535
MASK_LIMIT = (1 << 32) - 1

_PHASES = ("event16", "handoff", "player_activation", "completion", "failure")
_PHASE_RANK = {phase: rank for rank, phase in enumerate(_PHASES)}
_EVENT16_GATES = frozenset(("not_entered", "waiting", "admitted", "failed"))
_HANDOFFS = frozenset(("not_attempted", "attempted", "delivered", "failed"))
_DELIVERY_MODES = frozenset(("not_observed", "synchronous"))
_PLAYER_ACTIVATIONS = frozenset(("not_entered", "not_started", "started", "failed"))
_OUTCOMES = frozenset(("success", "failure"))
_EXTERNAL_SERVICES = frozenset(("not_entered", "entered"))


def _exact(record: dict[str, Any], expected: frozenset[str]) -> None:
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


def _enum(value: Any, label: str, allowed: frozenset[str] | tuple[str, ...]) -> str:
    if value not in allowed:
        raise ValueError(f"{label} has an unsupported value")
    return value


def _validate(event: dict[str, Any]) -> None:
    phase = event["phase"]
    gate = event["event16_gate"]
    handoff = event["handoff"]
    activation = event["player_activation"]
    movie_ready = event["movie_phase_one_completed"]
    delivery_mode = event["delivery_mode"]
    source_bound = (event["movie_component_is_constructed"] and
                    event["movie_owner_is_constructed_owner"] and
                    event["sequence_component_is_constructed"] and
                    event["sequence_owner_is_constructed_owner"])
    target_bound = (event["handoff_sender_is_movie_owner"] and
                    event["handoff_target_is_sequence_owner"])

    if gate == "admitted":
        if phase not in ("event16", "handoff", "player_activation", "completion", "failure"):
            raise ValueError("admitted event16 requires its dispatch or a later phase")
        if not movie_ready or event["outcome"] != "success":
            raise ValueError("admitted event16 requires phase-one completion and success")
    if gate == "waiting" and phase != "event16":
        raise ValueError("waiting event16 belongs only to the event16 phase")
    if handoff in ("attempted", "delivered"):
        if phase not in ("handoff", "player_activation", "completion", "failure"):
            raise ValueError("cutscene handoff requires its delivery or a later phase")
        if gate != "admitted" or not source_bound or not target_bound:
            raise ValueError("cutscene handoff requires admitted source-bound MovieControl and player relations")
    if handoff == "delivered" and event["outcome"] != "success":
        raise ValueError("delivered cutscene handoff requires success")
    if delivery_mode == "synchronous":
        if handoff != "delivered" or not source_bound or not target_bound:
            raise ValueError("synchronous delivery requires a delivered source-bound handoff")
    if handoff != "delivered" and delivery_mode != "not_observed":
        raise ValueError("delivery mode requires a delivered handoff")
    if activation == "started":
        if phase not in ("player_activation", "completion", "failure"):
            raise ValueError("started player requires its activation or a later phase")
        if handoff != "delivered" or not source_bound or event["outcome"] != "success":
            raise ValueError("started player requires a delivered source-bound handoff")
    if phase == "failure" and event["outcome"] != "failure":
        raise ValueError("failure phase requires failure outcome")
    if event["outcome"] == "failure" and not any(value == "failed" for value in (gate, handoff, activation)):
        raise ValueError("failure outcome requires a failed structural boundary")


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the only permitted structural MovieControl-to-player trace."""
    if not isinstance(raw, dict):
        raise ValueError("trace must be a JSON object")
    _exact(raw, frozenset(("format", "events")))
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 4096 events")

    permitted = frozenset((
        "observation_order", "phase", "callback_ordinal",
        "movie_component_is_constructed", "movie_owner_is_constructed_owner",
        "sequence_component_is_constructed", "sequence_owner_is_constructed_owner",
        "movie_phase_one_completed", "movie_phase_two_completed",
        "component_status_before", "component_status_after",
        "owner_status_before", "owner_status_after", "event16_gate",
        "handoff_sender_is_movie_owner", "handoff_target_is_sequence_owner", "handoff",
        "delivery_mode",
        "player_activation", "outcome", "external_service",
    ))
    clean_events: list[dict[str, Any]] = []
    prior_order = -1
    prior_phase = -1
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("trace event must be a JSON object")
        _exact(event, permitted)
        order = _natural(event["observation_order"], "observation_order", MAX_EVENTS)
        if order <= prior_order:
            raise ValueError("observation_order must be strictly increasing")
        prior_order = order
        phase = _enum(event["phase"], "phase", _PHASES)
        rank = _PHASE_RANK[phase]
        if rank < prior_phase:
            raise ValueError("phase order cannot move backward")
        prior_phase = rank
        clean = {
            "observation_order": order,
            "phase": phase,
            "callback_ordinal": _natural(event["callback_ordinal"], "callback_ordinal", MAX_CALLBACK_ORDINAL),
            "movie_component_is_constructed": _boolean(event["movie_component_is_constructed"], "movie_component_is_constructed"),
            "movie_owner_is_constructed_owner": _boolean(event["movie_owner_is_constructed_owner"], "movie_owner_is_constructed_owner"),
            "sequence_component_is_constructed": _boolean(event["sequence_component_is_constructed"], "sequence_component_is_constructed"),
            "sequence_owner_is_constructed_owner": _boolean(event["sequence_owner_is_constructed_owner"], "sequence_owner_is_constructed_owner"),
            "movie_phase_one_completed": _boolean(event["movie_phase_one_completed"], "movie_phase_one_completed"),
            "movie_phase_two_completed": _boolean(event["movie_phase_two_completed"], "movie_phase_two_completed"),
            "component_status_before": _natural(event["component_status_before"], "component_status_before", MASK_LIMIT),
            "component_status_after": _natural(event["component_status_after"], "component_status_after", MASK_LIMIT),
            "owner_status_before": _natural(event["owner_status_before"], "owner_status_before", MASK_LIMIT),
            "owner_status_after": _natural(event["owner_status_after"], "owner_status_after", MASK_LIMIT),
            "event16_gate": _enum(event["event16_gate"], "event16_gate", _EVENT16_GATES),
            "handoff_sender_is_movie_owner": _boolean(event["handoff_sender_is_movie_owner"], "handoff_sender_is_movie_owner"),
            "handoff_target_is_sequence_owner": _boolean(event["handoff_target_is_sequence_owner"], "handoff_target_is_sequence_owner"),
            "handoff": _enum(event["handoff"], "handoff", _HANDOFFS),
            "delivery_mode": _enum(event["delivery_mode"], "delivery_mode", _DELIVERY_MODES),
            "player_activation": _enum(event["player_activation"], "player_activation", _PLAYER_ACTIVATIONS),
            "outcome": _enum(event["outcome"], "outcome", _OUTCOMES),
            "external_service": _enum(event["external_service"], "external_service", _EXTERNAL_SERVICES),
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
    parser.add_argument("input", type=pathlib.Path, help="private structural JSON emitted by the observer")
    parser.add_argument("output", type=pathlib.Path, help="new private sanitized JSON path")
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
    print(f"wrote {len(sanitized['events'])} source-free MovieControl-to-player records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
