#!/usr/bin/env python3
"""Verify two identical source-free MovieControl handoff observations.

Inputs must already have passed ``movie_control_cutscene_dispatch_trace.py``.
The utility does not open an executable, install, archive, dump, log,
screenshot, or raw observer record.  It revalidates the narrow structural
schema and retains one exact trace only when two fresh-process observations
agree.  Inputs and the newly written result always remain outside the
repository.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any

import movie_control_cutscene_dispatch_trace as dispatch_trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = dispatch_trace.OUTPUT_FORMAT
OUTPUT_FORMAT = "off.movie-control-cutscene-dispatch-repeat-pair/v1"


def _validated_sanitized_trace(raw: Any) -> dict[str, Any]:
    """Revalidate a sanitized trace without accepting raw observer input."""
    if not isinstance(raw, dict) or frozenset(raw) != frozenset(("format", "events")):
        raise ValueError("sanitized trace must contain only format and events")
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized sanitized trace format")
    return dispatch_trace.sanitize_trace({
        "format": dispatch_trace.INPUT_FORMAT,
        "events": raw["events"],
    })


def _has_terminal_boundary(events: list[dict[str, Any]]) -> bool:
    """Require one observed route outcome without inventing its behavior."""
    return any(
        event["player_activation"] == "started" or
        event["event16_gate"] == "failed" or
        event["handoff"] == "failed" or
        event["player_activation"] == "failed"
        for event in events
    )


def sanitize_repeat_pair(first: Any, second: Any) -> dict[str, Any]:
    """Return one source-free trace only when both observations match exactly."""
    first_trace = _validated_sanitized_trace(first)
    second_trace = _validated_sanitized_trace(second)
    if first_trace["events"] != second_trace["events"]:
        raise ValueError("repeat observations disagree on the structural handoff trace")
    if not _has_terminal_boundary(first_trace["events"]):
        raise ValueError("repeat observation lacks a terminal handoff boundary")
    return {"format": OUTPUT_FORMAT, "events": first_trace["events"]}


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
    print(f"wrote {len(result['events'])} source-free MovieControl handoff repeat records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
