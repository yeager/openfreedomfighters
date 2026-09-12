#!/usr/bin/env python3
"""Validate one private, source-free first-mission observation trace.

The observer records externally visible categories only.  It must not export
retail identifiers, paths, addresses, executable material, media, text, or
serialized scene data.  This utility never opens an executable or game data.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.first-mission-observation.raw/v1"
OUTPUT_FORMAT = "off.first-mission-observation/v1"
MAX_EVENTS = 4096
_FINGERPRINT = re.compile(r"[0-9a-f]{64}\Z")

_PLATFORMS = frozenset(("windows", "linux", "macos"))
_ARCHITECTURES = frozenset(("x86", "x86_64", "arm64"))
_INPUT_DEVICES = frozenset(("keyboard_mouse", "controller"))
_RUN_KINDS = frozenset(("baseline", "input_experiment"))
_PROBES = frozenset((
    "launch", "idle", "movement", "look", "movement_look", "fire", "aim",
    "interact", "squad", "pause", "menu", "obstacle", "interaction_candidate",
))
_STATES = {
    "handoff": frozenset(("cinematic_visible", "loading_visible", "gameplay_viewport_visible")),
    "control": frozenset(("no_response", "movement_only", "camera_only", "movement_and_camera", "blocked_by_overlay")),
    "camera": frozenset(("fixed", "follows_translation", "rotates_with_look", "independently_rotatable", "unknown")),
    "player": frozenset(("absent", "spawned", "controllable", "disabled", "unknown")),
    "collision": frozenset(("no_contact", "blocked", "sliding", "stepped", "falling", "unknown")),
    "interaction": frozenset(("unavailable", "prompt_only", "entered_range", "accepted", "rejected", "unknown")),
    "mission": frozenset(("unchanged", "objective_advanced", "failed", "completed", "loading", "unknown")),
    "hud": frozenset(("absent", "stable", "changed", "hidden", "unknown")),
}


def _exact(record: dict[str, Any], expected: frozenset[str]) -> None:
    if frozenset(record) != expected:
        raise ValueError("trace record has an unsupported field")


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or value < 0 or value > maximum:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value


def _enum(value: Any, label: str, allowed: frozenset[str]) -> str:
    if value not in allowed:
        raise ValueError(f"{label} has an unsupported value")
    return value


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the fixed, source-free trace schema and reject all other data."""
    if not isinstance(raw, dict):
        raise ValueError("trace must be a JSON object")
    _exact(raw, frozenset((
        "format", "method_version", "verified_data_manifest_fingerprint", "platform",
        "architecture", "input_device", "run_kind", "events",
    )))
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    method_version = _natural(raw["method_version"], "method_version", 65535)
    if method_version == 0:
        raise ValueError("method_version must be nonzero")
    fingerprint = raw["verified_data_manifest_fingerprint"]
    if not isinstance(fingerprint, str) or not _FINGERPRINT.fullmatch(fingerprint):
        raise ValueError("verified_data_manifest_fingerprint must be an opaque SHA-256 value")
    platform = _enum(raw["platform"], "platform", _PLATFORMS)
    architecture = _enum(raw["architecture"], "architecture", _ARCHITECTURES)
    input_device = _enum(raw["input_device"], "input_device", _INPUT_DEVICES)
    run_kind = _enum(raw["run_kind"], "run_kind", _RUN_KINDS)
    events = raw["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 4096 events")

    permitted = frozenset(("observation_order", "probe", "boundary", "state", "visible_change"))
    clean_events: list[dict[str, Any]] = []
    prior_order = -1
    for index, event in enumerate(events):
        if not isinstance(event, dict):
            raise ValueError("trace event must be a JSON object")
        _exact(event, permitted)
        order = _natural(event["observation_order"], "observation_order", MAX_EVENTS)
        if order <= prior_order:
            raise ValueError("observation_order must be strictly increasing")
        prior_order = order
        probe = _enum(event["probe"], "probe", _PROBES)
        boundary = _enum(event["boundary"], "boundary", frozenset(_STATES))
        state = _enum(event["state"], "state", _STATES[boundary])
        visible_change = event["visible_change"]
        if type(visible_change) is not bool:
            raise ValueError("visible_change must be boolean")
        if index == 0 and (probe != "launch" or boundary != "handoff"):
            raise ValueError("the first event must record the launch handoff boundary")
        if index > 0 and probe == "launch":
            raise ValueError("launch may appear only in the first event")
        clean_events.append({
            "observation_order": order,
            "probe": probe,
            "boundary": boundary,
            "state": state,
            "visible_change": visible_change,
        })

    return {
        "format": OUTPUT_FORMAT,
        "method_version": method_version,
        "verified_data_manifest_fingerprint": fingerprint,
        "platform": platform,
        "architecture": architecture,
        "input_device": input_device,
        "run_kind": run_kind,
        "events": clean_events,
    }


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
    print(f"wrote {len(sanitized['events'])} source-free first-mission observation records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
