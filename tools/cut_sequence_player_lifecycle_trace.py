#!/usr/bin/env python3
"""Validate a private, source-free ZLIST_CutSequence player trace.

This is intentionally not a component-reader or command trace.  It captures
the outer player boundary: joining the prepared sequence to the first-cut
receiver, its member/reference sweep, and the later activation/completion
handoff.  A separately maintained private observer supplies categorical
relations only.  This program never reads game files, executables, dumps,
screenshots, logs, or assets.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.cut-sequence-player-lifecycle.raw/v1"
OUTPUT_FORMAT = "off.cut-sequence-player-lifecycle/v1"
MAX_EVENTS = 4096
MAX_CALLBACK_ORDINAL = 65535
MASK_LIMIT = (1 << 32) - 1

_PHASES = ("construction", "reader", "phase_one", "phase_two", "activation", "completion", "failure")
_PHASE_RANK = {phase: rank for rank, phase in enumerate(_PHASES)}
_RECEIPTS = frozenset(("absent", "complete"))
_PLAYER_STATES = frozenset(("cold", "phase_one_ready", "phase_two_ready", "active", "completed", "failed"))
_RECEIVER_STATES = frozenset(("closed", "open", "sealed"))
_MEMBER_SWEEPS = frozenset(("not_entered", "queried", "derived", "failed"))
_REFERENCE_SWEEPS = frozenset(("not_entered", "camera_only", "camera_and_sequence", "failed"))
_ACTIVATIONS = frozenset(("not_attempted", "attempted", "started", "failed"))
_COMPLETIONS = frozenset(("not_observed", "pending", "completed", "failed"))
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
    state_before, state_after = event["player_state_before"], event["player_state_after"]
    receiver = event["receiver_state"]
    if phase == "construction" and event["reader_graph_receipt"] != "absent":
        raise ValueError("construction cannot claim a complete reader receipt")
    if phase != "construction" and event["reader_graph_receipt"] == "complete" and not event["sequence_component_constructed"]:
        raise ValueError("a complete reader receipt requires a constructed sequence component")
    if state_before == "failed" and state_after != "failed":
        raise ValueError("a failed player cannot leave the failed state")
    if state_after == "failed" and event["outcome"] != "failure":
        raise ValueError("failed player state requires a failure outcome")
    if phase == "failure" and event["outcome"] != "failure":
        raise ValueError("failure phase requires a failure outcome")
    if phase == "phase_one" and state_after not in ("phase_one_ready", "failed"):
        raise ValueError("phase one must establish readiness or fail")
    if phase == "phase_two" and state_before != "phase_one_ready":
        raise ValueError("phase two requires phase-one readiness")
    if phase == "phase_two" and state_after not in ("phase_two_ready", "failed"):
        raise ValueError("phase two must establish readiness or fail")
    if phase == "activation" and state_before != "phase_two_ready":
        raise ValueError("activation requires phase-two readiness")
    if phase == "activation" and event["activation"] in ("attempted", "started") and receiver != "sealed":
        raise ValueError("activation requires a sealed receiver")
    if event["activation"] == "started" and state_after != "active":
        raise ValueError("a started activation requires the active player state")
    if phase == "completion" and state_before != "active":
        raise ValueError("completion requires an active player")
    if event["completion"] == "completed" and state_after != "completed":
        raise ValueError("completed result requires completed player state")
    if event["member_sweep"] == "derived" and phase != "phase_two":
        raise ValueError("member derivation belongs only to phase two")
    if event["reference_sweep"] == "camera_and_sequence" and phase != "phase_two":
        raise ValueError("player reference resolution belongs only to phase two")
    if receiver == "open" and state_after not in ("phase_one_ready", "failed"):
        raise ValueError("an open receiver belongs only to phase-one readiness")
    if receiver == "sealed" and phase not in ("phase_two", "activation", "completion", "failure"):
        raise ValueError("a sealed receiver requires phase two or a later boundary")


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the only permitted structural player-lifecycle trace."""
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
        "sequence_component_constructed", "sequence_owner_constructed", "reader_graph_receipt",
        "component_status_before", "component_status_after", "owner_status_before", "owner_status_after",
        "player_state_before", "player_state_after", "receiver_state", "member_sweep", "reference_sweep",
        "activation", "completion", "outcome", "external_service",
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
            "sequence_component_constructed": _boolean(event["sequence_component_constructed"], "sequence_component_constructed"),
            "sequence_owner_constructed": _boolean(event["sequence_owner_constructed"], "sequence_owner_constructed"),
            "reader_graph_receipt": _enum(event["reader_graph_receipt"], "reader_graph_receipt", _RECEIPTS),
            "component_status_before": _natural(event["component_status_before"], "component_status_before", MASK_LIMIT),
            "component_status_after": _natural(event["component_status_after"], "component_status_after", MASK_LIMIT),
            "owner_status_before": _natural(event["owner_status_before"], "owner_status_before", MASK_LIMIT),
            "owner_status_after": _natural(event["owner_status_after"], "owner_status_after", MASK_LIMIT),
            "player_state_before": _enum(event["player_state_before"], "player_state_before", _PLAYER_STATES),
            "player_state_after": _enum(event["player_state_after"], "player_state_after", _PLAYER_STATES),
            "receiver_state": _enum(event["receiver_state"], "receiver_state", _RECEIVER_STATES),
            "member_sweep": _enum(event["member_sweep"], "member_sweep", _MEMBER_SWEEPS),
            "reference_sweep": _enum(event["reference_sweep"], "reference_sweep", _REFERENCE_SWEEPS),
            "activation": _enum(event["activation"], "activation", _ACTIVATIONS),
            "completion": _enum(event["completion"], "completion", _COMPLETIONS),
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
    print(f"wrote {len(sanitized['events'])} source-free cut-sequence-player records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
