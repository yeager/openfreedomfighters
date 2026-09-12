#!/usr/bin/env python3
"""Validate a private, source-free CutSequenceList lifecycle trace.

The original process must be observed by a separately maintained private
instrument.  This program never reads game files, executables, debugger logs,
memory dumps, or assets.  It accepts only the enumerated structural relations
below and writes a new JSON document outside this repository.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.cut-sequence-list-phase-one.raw/v1"
OUTPUT_FORMAT = "off.cut-sequence-list-phase-one/v1"
MAX_EVENTS = 4096
MAX_CALLBACK_ORDINAL = 65535
MASK_LIMIT = (1 << 32) - 1

_PHASES = ("construction", "reader", "phase_one", "command_window", "completion", "failure")
_PHASE_RANK = {phase: rank for rank, phase in enumerate(_PHASES)}
_READER_RECEIPTS = frozenset(("absent", "complete"))
_COLLECTION_STATES = frozenset(("absent", "created", "retained"))
_COMMAND_WINDOWS = frozenset(("not_entered", "closed", "open", "accepted", "rejected"))
_OUTCOMES = frozenset(("success", "failure"))
_EXTERNAL_SERVICES = frozenset(("not_entered", "entered"))


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


def _enum(value: Any, label: str, allowed: frozenset[str] | tuple[str, ...]) -> str:
    if value not in allowed:
        raise ValueError(f"{label} has an unsupported value")
    return value


def _validate_relations(event: dict[str, Any]) -> None:
    phase = event["phase"]
    receipt = event["reader_graph_receipt"]
    collection_before = event["collection_before"]
    collection_after = event["collection_after"]
    window = event["command_window"]
    if phase == "construction" and receipt != "absent":
        raise ValueError("construction cannot claim a complete reader receipt")
    if phase != "construction" and receipt == "complete" and not event["component_is_constructed"]:
        raise ValueError("a complete reader receipt requires a constructed component")
    if collection_before == "retained" and collection_after != "retained":
        raise ValueError("a retained collection cannot be cleared by this trace")
    if window in ("accepted", "rejected") and phase != "command_window":
        raise ValueError("command admission result requires the command-window phase")
    if window == "open" and phase not in ("phase_one", "command_window"):
        raise ValueError("an open command window requires phase one or command-window observation")
    if phase == "failure" and event["outcome"] != "failure":
        raise ValueError("failure phase requires a failure result")


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the only permitted structural CutSequenceList trace.

    Collection and command-window values are categorical relations only.  They
    reveal neither retained command content nor object identities.  Callback
    ordinals are local to one observer run and are not executable identifiers.
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
        "observation_order", "phase", "callback_ordinal",
        "component_is_constructed", "owner_is_constructed_owner", "reader_graph_receipt",
        "component_status_before", "component_status_after",
        "owner_status_before", "owner_status_after",
        "collection_before", "collection_after", "command_window",
        "outcome", "external_service",
    ))
    sanitized: list[dict[str, Any]] = []
    prior_order = -1
    prior_phase_rank = -1
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("trace event must be a JSON object")
        _require_exact_keys(event, permitted)
        order = _natural(event["observation_order"], "observation_order", MAX_EVENTS)
        if order <= prior_order:
            raise ValueError("observation_order must be strictly increasing")
        prior_order = order
        phase = _enum(event["phase"], "phase", _PHASES)
        phase_rank = _PHASE_RANK[phase]
        if phase_rank < prior_phase_rank:
            raise ValueError("phase order cannot move backward")
        prior_phase_rank = phase_rank
        clean = {
            "observation_order": order,
            "phase": phase,
            "callback_ordinal": _natural(event["callback_ordinal"], "callback_ordinal", MAX_CALLBACK_ORDINAL),
            "component_is_constructed": _boolean(event["component_is_constructed"], "component_is_constructed"),
            "owner_is_constructed_owner": _boolean(event["owner_is_constructed_owner"], "owner_is_constructed_owner"),
            "reader_graph_receipt": _enum(event["reader_graph_receipt"], "reader_graph_receipt", _READER_RECEIPTS),
            "component_status_before": _natural(event["component_status_before"], "component_status_before", MASK_LIMIT),
            "component_status_after": _natural(event["component_status_after"], "component_status_after", MASK_LIMIT),
            "owner_status_before": _natural(event["owner_status_before"], "owner_status_before", MASK_LIMIT),
            "owner_status_after": _natural(event["owner_status_after"], "owner_status_after", MASK_LIMIT),
            "collection_before": _enum(event["collection_before"], "collection_before", _COLLECTION_STATES),
            "collection_after": _enum(event["collection_after"], "collection_after", _COLLECTION_STATES),
            "command_window": _enum(event["command_window"], "command_window", _COMMAND_WINDOWS),
            "outcome": _enum(event["outcome"], "outcome", _OUTCOMES),
            "external_service": _enum(event["external_service"], "external_service", _EXTERNAL_SERVICES),
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
    print(f"wrote {len(sanitized['events'])} source-free cut-sequence-list records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
