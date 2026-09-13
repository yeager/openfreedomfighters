#!/usr/bin/env python3
"""Bundle repeatable source-free intro-camera lifecycle evidence."""
from __future__ import annotations
import argparse
import json
import pathlib
import sys
from typing import Any
import intro_camera_lifecycle_trace as trace

OUTPUT_FORMAT = "off.intro-camera-lifecycle-contract/v1"

def _clean(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != {"format", "events"} or raw["format"] != trace.OUTPUT_FORMAT:
        raise ValueError("evidence must be a sanitized intro-camera trace")
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": raw["events"]})

def bundle_contract(candidate_raw: Any, repeat_raw: Any, failure_raw: Any) -> dict[str, Any]:
    candidate, repeat, failure = _clean(candidate_raw), _clean(repeat_raw), _clean(failure_raw)
    expected = list(trace.STAGES[:-1])
    if [event["stage"] for event in candidate["events"]] != expected or candidate != repeat:
        raise ValueError("successful observations must be identical and cover every lifecycle relation once")
    if failure["events"][-1]["stage"] != "failure" or failure["events"][-1]["outcome"] != "failure":
        raise ValueError("failure evidence must terminate in failure")
    if candidate["events"][-1]["callback_ordinal"] == failure["events"][-1]["callback_ordinal"]:
        raise ValueError("success and failure must use distinct observer-local callbacks")
    return {"format": OUTPUT_FORMAT, "candidate": {"sequence_active": True, "controller_ready": True, "camera_owner_resolved": True, "transform_composed": True, "view_admitted": True, "frame_delivered": True, "outcome": "success"}, "repeat": {"sequence_active": True, "controller_ready": True, "camera_owner_resolved": True, "transform_composed": True, "view_admitted": True, "frame_delivered": True, "outcome": "success"}, "failure": {"outcome": "failure"}}

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("candidate", "repeat", "failure", "output"): parser.add_argument(name, type=pathlib.Path)
    args = parser.parse_args()
    try:
        paths = [trace._outside_repository(getattr(args, name), name) for name in ("candidate", "repeat", "failure", "output")]
        if len(set(paths)) != 4 or paths[-1].exists(): raise ValueError("private inputs and new output must be distinct")
        receipt = bundle_contract(*(trace._read_private_json_no_follow(path, label) for path, label in zip(paths[:-1], ("candidate", "repeat", "failure"))))
        trace._write_new_private_json_no_follow(paths[-1], receipt)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr); return 1
    print("wrote one inert source-free intro-camera lifecycle receipt"); return 0

if __name__ == "__main__": raise SystemExit(main())
