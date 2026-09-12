#!/usr/bin/env python3
"""Validate a private, source-free startup-menu scene-request trace.

The currently recovered BootMenu route proves selection, active-window state,
and an internal menu transition.  It does not prove a scene-manager request.
This utility records only the structural evidence needed to close that gap.
It never opens game files, executables, dumps, screenshots, logs, or assets.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.startup-menu-scene-request.raw/v2"
OUTPUT_FORMAT = "off.startup-menu-scene-request/v2"
MAX_EVENTS = 4096
MAX_CALLBACK_ORDINAL = 65535
MASK_LIMIT = (1 << 32) - 1

_PHASES = ("selection", "active_window", "receiver", "scene_manager", "package_admission", "completion", "failure")
_PHASE_RANK = {phase: rank for rank, phase in enumerate(_PHASES)}
_SELECTIONS = frozenset(("not_attempted", "rejected", "resolved", "delivered", "failed"))
_ACTIVE_WINDOWS = frozenset(("not_observed", "unchanged", "replaced", "failed"))
_ACTIVE_WINDOW_SELECTION_DELIVERIES = frozenset(("not_observed", "delivered", "failed"))
_ROUTES = frozenset(("not_entered", "receiver_only", "runtime_transition", "scene_request", "failed"))
_RECEIVER_MANAGER_EDGES = frozenset(("not_observed", "not_entered", "entered", "failed"))
_MANAGER_REQUESTS = frozenset(("not_entered", "clear_only", "request_only", "clear_then_request", "failed"))
_TARGETS = frozenset(("not_observed", "retained", "validated", "rejected"))
_PACKAGE_ADMISSIONS = frozenset(("not_entered", "candidate", "admitted", "rejected", "failed"))
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
    selection = event["selection"]
    active_window = event["active_window"]
    active_window_selection_delivery = event["active_window_selection_delivery"]
    route = event["receiver_route"]
    receiver_manager_edge = event["receiver_manager_edge"]
    manager = event["manager_request"]
    target = event["request_target"]
    package = event["package_admission"]

    if selection == "delivered" and phase not in ("receiver", "scene_manager", "package_admission", "completion", "failure"):
        raise ValueError("a delivered selection requires a receiver or later phase")
    if active_window == "replaced" and phase not in ("active_window", "receiver", "scene_manager", "package_admission", "completion", "failure"):
        raise ValueError("active-window replacement requires its own or a later phase")
    if active_window_selection_delivery == "delivered":
        if selection != "delivered" or active_window != "replaced":
            raise ValueError("active-window selection delivery requires delivered selection and replacement")
        if phase not in ("active_window", "receiver", "scene_manager", "package_admission", "completion", "failure"):
            raise ValueError("active-window selection delivery requires the active-window or a later phase")
    if active_window_selection_delivery == "failed":
        if selection != "delivered" or active_window != "failed":
            raise ValueError("failed active-window selection delivery requires a delivered selection and failed window update")
        if phase not in ("active_window", "receiver", "scene_manager", "package_admission", "completion", "failure"):
            raise ValueError("failed active-window selection delivery requires the active-window or a later phase")
    if active_window == "replaced" and active_window_selection_delivery != "delivered":
        raise ValueError("active-window replacement must identify selection delivery")
    if active_window == "failed" and active_window_selection_delivery != "failed":
        raise ValueError("failed active-window update must identify selection delivery failure")
    if route == "scene_request":
        if phase not in ("scene_manager", "package_admission", "completion", "failure"):
            raise ValueError("a scene request requires the manager or a later phase")
        if manager not in ("request_only", "clear_then_request", "failed"):
            raise ValueError("a scene request requires a manager request result")
        if target not in ("retained", "validated", "rejected"):
            raise ValueError("a scene request requires a retained target result")
    if receiver_manager_edge == "entered":
        if phase not in ("scene_manager", "package_admission", "completion", "failure"):
            raise ValueError("receiver-to-manager entry requires the manager or a later phase")
        if active_window_selection_delivery != "delivered":
            raise ValueError("receiver-to-manager entry requires delivered active-window selection")
        if route != "scene_request" or manager not in ("request_only", "clear_then_request"):
            raise ValueError("receiver-to-manager entry requires a scene-manager request result")
    if receiver_manager_edge == "failed":
        if phase not in ("scene_manager", "failure"):
            raise ValueError("failed receiver-to-manager edge requires the manager or failure phase")
        if active_window_selection_delivery != "delivered" or route != "scene_request" or manager != "failed":
            raise ValueError("failed receiver-to-manager edge requires delivered selection and failed manager request")
    if manager in ("request_only", "clear_then_request") and route != "scene_request":
        raise ValueError("manager request evidence requires a scene-request receiver route")
    if manager in ("request_only", "clear_then_request") and receiver_manager_edge != "entered":
        raise ValueError("manager request evidence requires a receiver-to-manager edge")
    if manager == "failed" and receiver_manager_edge != "failed":
        raise ValueError("failed manager request evidence requires a failed receiver-to-manager edge")
    if package != "not_entered":
        if phase not in ("package_admission", "completion", "failure"):
            raise ValueError("package admission belongs only after scene-manager handoff")
        if route != "scene_request" or manager not in ("request_only", "clear_then_request"):
            raise ValueError("package admission requires a verified manager request")
        if target != "validated":
            raise ValueError("package admission requires a validated retained target")
    if package == "admitted" and event["outcome"] != "success":
        raise ValueError("admitted package requires success")
    if phase == "failure" and event["outcome"] != "failure":
        raise ValueError("failure phase requires failure outcome")


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the only permitted structural startup-menu request trace."""
    if not isinstance(raw, dict):
        raise ValueError("trace must be a JSON object")
    _exact(raw, frozenset(("format", "events")))
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 4096 events")

    permitted = frozenset((
        "observation_order", "phase", "callback_ordinal", "menu_component_constructed",
        "reader_graph_receipt", "component_status_before", "component_status_after",
        "owner_status_before", "owner_status_after", "selection", "active_window",
        "active_window_selection_delivery", "receiver_route", "receiver_manager_edge",
        "manager_request", "request_target", "package_admission",
        "outcome", "external_service",
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
            "menu_component_constructed": _boolean(event["menu_component_constructed"], "menu_component_constructed"),
            "reader_graph_receipt": _enum(event["reader_graph_receipt"], "reader_graph_receipt", frozenset(("absent", "complete"))),
            "component_status_before": _natural(event["component_status_before"], "component_status_before", MASK_LIMIT),
            "component_status_after": _natural(event["component_status_after"], "component_status_after", MASK_LIMIT),
            "owner_status_before": _natural(event["owner_status_before"], "owner_status_before", MASK_LIMIT),
            "owner_status_after": _natural(event["owner_status_after"], "owner_status_after", MASK_LIMIT),
            "selection": _enum(event["selection"], "selection", _SELECTIONS),
            "active_window": _enum(event["active_window"], "active_window", _ACTIVE_WINDOWS),
            "active_window_selection_delivery": _enum(
                event["active_window_selection_delivery"],
                "active_window_selection_delivery",
                _ACTIVE_WINDOW_SELECTION_DELIVERIES,
            ),
            "receiver_route": _enum(event["receiver_route"], "receiver_route", _ROUTES),
            "receiver_manager_edge": _enum(
                event["receiver_manager_edge"],
                "receiver_manager_edge",
                _RECEIVER_MANAGER_EDGES,
            ),
            "manager_request": _enum(event["manager_request"], "manager_request", _MANAGER_REQUESTS),
            "request_target": _enum(event["request_target"], "request_target", _TARGETS),
            "package_admission": _enum(event["package_admission"], "package_admission", _PACKAGE_ADMISSIONS),
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
    print(f"wrote {len(sanitized['events'])} source-free startup-menu request records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
