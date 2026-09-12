#!/usr/bin/env python3
"""Sanitize one private, source-free scene-manager lifecycle observation.

Only bounded categorical lifecycle relations are retained.  This tool never
opens game data, executables, dumps, screenshots, logs, or process state.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any

INPUT_FORMAT = "off.scene-manager-lifecycle.raw/v1"
OUTPUT_FORMAT = "off.scene-manager-lifecycle/v1"
MAX_EVENTS = 256
MAX_CALLBACK_ORDINAL = 65535
PHASES = ("entry", "previous_scene", "stage", "global_lifecycle", "component_phase_one", "camera_route", "commit", "completion", "failure")
RANK = {value: index for index, value in enumerate(PHASES)}
VALUES = {
    "manager": {"not_entered", "entered", "failed"},
    "previous_scene": {"not_observed", "retained", "retired", "failed"},
    "staging": {"not_attempted", "entered", "ready", "rejected", "failed"},
    "global_lifecycle": {"not_attempted", "entered", "completed", "failed"},
    "component_phase_one": {"not_attempted", "entered", "completed", "failed"},
    "camera_route": {"not_attempted", "entered", "ready", "failed"},
    "commit": {"not_attempted", "entered", "committed", "failed"},
    "outcome": {"success", "failure"},
    "external_service": {"not_entered", "entered"},
}
FIELDS = frozenset(("observation_order", "phase", "callback_ordinal", *VALUES))

def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value

def _enum(value: Any, label: str) -> str:
    if value not in VALUES[label]:
        raise ValueError(f"{label} has an unsupported value")
    return value

def _validate(event: dict[str, Any]) -> None:
    phase = event["phase"]
    if phase not in RANK:
        raise ValueError("phase has an unsupported value")
    manager, staging = event["manager"], event["staging"]
    global_lifecycle, component = event["global_lifecycle"], event["component_phase_one"]
    camera, commit, outcome = event["camera_route"], event["commit"], event["outcome"]
    if phase != "entry" and manager != "entered":
        raise ValueError("later phases require an entered scene manager")
    if staging in {"entered", "ready", "rejected"} and phase not in RANK or (staging in {"entered", "ready", "rejected"} and RANK[phase] < RANK["stage"]):
        raise ValueError("staging requires its phase")
    if global_lifecycle in {"entered", "completed"} and (staging != "ready" or RANK[phase] < RANK["global_lifecycle"]):
        raise ValueError("global lifecycle requires a staged scene")
    if component in {"entered", "completed"} and (global_lifecycle != "completed" or RANK[phase] < RANK["component_phase_one"]):
        raise ValueError("component phase one requires completed global lifecycle")
    if camera in {"entered", "ready"} and (component != "completed" or RANK[phase] < RANK["camera_route"]):
        raise ValueError("camera route requires completed component phase one")
    if commit in {"entered", "committed"} and (camera != "ready" or RANK[phase] < RANK["commit"]):
        raise ValueError("commit requires a ready camera route")
    if phase == "completion" and not (commit == "committed" and outcome == "success"):
        raise ValueError("completion requires a committed scene")
    if phase == "failure" and outcome != "failure":
        raise ValueError("failure phase requires failure outcome")
    if outcome == "failure" and phase != "failure":
        raise ValueError("failure outcome requires failure phase")

def sanitize_trace(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != {"format", "events"} or raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 256 events")
    clean: list[dict[str, Any]] = []
    prior_order = prior_phase = -1
    callback: int | None = None
    for raw_event in events:
        if not isinstance(raw_event, dict) or frozenset(raw_event) != FIELDS:
            raise ValueError("trace record has an unsupported field")
        order = _natural(raw_event["observation_order"], "observation_order", MAX_EVENTS)
        if order <= prior_order: raise ValueError("observation_order must be strictly increasing")
        phase = raw_event["phase"]
        if phase not in RANK or RANK[phase] < prior_phase: raise ValueError("phase order cannot move backward")
        current = {"observation_order": order, "phase": phase,
                   "callback_ordinal": _natural(raw_event["callback_ordinal"], "callback_ordinal", MAX_CALLBACK_ORDINAL)}
        current.update({key: _enum(raw_event[key], key) for key in VALUES})
        if callback is None: callback = current["callback_ordinal"]
        elif callback != current["callback_ordinal"]: raise ValueError("one trace must use one observer-local callback ordinal")
        _validate(current); clean.append(current); prior_order, prior_phase = order, RANK[phase]
    return {"format": OUTPUT_FORMAT, "events": clean}

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("input", type=pathlib.Path); parser.add_argument("output", type=pathlib.Path); args = parser.parse_args()
    try:
        if args.output.exists() or args.input.resolve() == args.output.resolve(): raise ValueError("refusing to overwrite output")
        result = sanitize_trace(json.loads(args.input.read_text(encoding="utf-8")))
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr); return 1
    print(f"wrote {len(result['events'])} source-free scene lifecycle records"); return 0
if __name__ == "__main__": raise SystemExit(main())
