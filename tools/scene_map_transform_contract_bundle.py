#!/usr/bin/env python3
"""Bundle repeatable vector-free map-transform observations into an inert receipt."""
from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any

import scene_map_transform_trace as trace

OUTPUT_FORMAT = "off.scene-map-transform-contract/v1"


def _clean(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != {"format", "events"} or raw["format"] != trace.OUTPUT_FORMAT:
        raise ValueError("evidence must be a sanitized map-transform trace")
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": raw["events"]})


def _success(record: dict[str, Any]) -> None:
    stages = [event["stage"] for event in record["events"]]
    if stages != list(trace.STAGES[:-1]):
        raise ValueError("success evidence must cover every map-transform relation exactly once")


def _failure(record: dict[str, Any]) -> None:
    if record["events"][-1]["stage"] != "failure" or record["events"][-1]["outcome"] != "failure":
        raise ValueError("failure evidence must terminate in failure")


def bundle_contract(candidate_raw: Any, repeat_raw: Any, failure_raw: Any) -> dict[str, Any]:
    candidate, repeat, failure = _clean(candidate_raw), _clean(repeat_raw), _clean(failure_raw)
    if candidate != repeat:
        raise ValueError("successful map-transform observations must be identical")
    _success(candidate)
    _failure(failure)
    if candidate["events"][-1]["callback_ordinal"] == failure["events"][-1]["callback_ordinal"]:
        raise ValueError("success and failure must use distinct observer-local callbacks")
    return {"format": OUTPUT_FORMAT,
            "candidate": {"scene_active": True, "parent_relation_checked": True, "local_relation_checked": True,
                          "world_relation_resolved": True, "render_boundary_consumed": True, "outcome": "success"},
            "repeat": {"scene_active": True, "parent_relation_checked": True, "local_relation_checked": True,
                       "world_relation_resolved": True, "render_boundary_consumed": True, "outcome": "success"},
            "failure": {"scene_active": True, "parent_relation_checked": False, "local_relation_checked": False,
                        "world_relation_resolved": False, "render_boundary_consumed": False, "outcome": "failure"}}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=pathlib.Path)
    parser.add_argument("repeat", type=pathlib.Path)
    parser.add_argument("failure", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        paths = [trace._outside_repository(getattr(args, name), name) for name in ("candidate", "repeat", "failure", "output")]
        if len(set(paths)) != len(paths) or paths[-1].exists():
            raise ValueError("private inputs and new output must be distinct")
        receipt = bundle_contract(*(trace._read_private_json_no_follow(path, label) for path, label in zip(paths[:-1], ("candidate", "repeat", "failure"))))
        trace._write_new_private_json_no_follow(paths[-1], receipt)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one inert vector-free map-transform receipt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
