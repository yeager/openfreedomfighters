#!/usr/bin/env python3
"""Bundle repeatable source-free scene lifecycle evidence into an inert receipt."""
from __future__ import annotations
import argparse, json, pathlib, sys
from typing import Any
import scene_manager_lifecycle_trace as trace
OUTPUT_FORMAT = "off.scene-manager-lifecycle-contract/v1"

def _clean(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != {"format", "events"} or raw["format"] != trace.OUTPUT_FORMAT: raise ValueError("evidence must be a sanitized scene lifecycle trace")
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": raw["events"]})
def _terminal(record: dict[str, Any], phase: str, outcome: str) -> None:
    terminals=[event for event in record["events"] if event["phase"] in {"completion", "failure"}]
    if len(terminals)!=1 or terminals[0]["phase"]!=phase or terminals[0]["outcome"]!=outcome: raise ValueError("evidence has an invalid lifecycle terminal")
def bundle_contract(candidate_raw: Any, repeat_raw: Any, failure_raw: Any) -> dict[str, Any]:
    candidate, repeat, failure = _clean(candidate_raw), _clean(repeat_raw), _clean(failure_raw)
    if candidate != repeat: raise ValueError("successful scene lifecycle observations must be identical")
    _terminal(candidate,"completion","success"); _terminal(failure,"failure","failure")
    if candidate["events"][-1]["callback_ordinal"] == failure["events"][-1]["callback_ordinal"]: raise ValueError("success and failure must use distinct observer-local callbacks")
    return {"format": OUTPUT_FORMAT,
            "candidate": {"manager_entered":True,"scene_staged":True,"global_lifecycle_completed":True,"component_phase_one_completed":True,"camera_route_ready":True,"scene_committed":True,"outcome":"success"},
            "repeat": {"manager_entered":True,"scene_staged":True,"global_lifecycle_completed":True,"component_phase_one_completed":True,"camera_route_ready":True,"scene_committed":True,"outcome":"success"},
            "failure": {"manager_entered":True,"scene_staged":False,"global_lifecycle_completed":False,"component_phase_one_completed":False,"camera_route_ready":False,"scene_committed":False,"outcome":"failure"}}
def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument("candidate",type=pathlib.Path); parser.add_argument("repeat",type=pathlib.Path); parser.add_argument("failure",type=pathlib.Path); parser.add_argument("output",type=pathlib.Path); args=parser.parse_args()
    try:
        if args.output.exists(): raise ValueError("refusing to overwrite output")
        receipt=bundle_contract(*(json.loads(path.read_text(encoding="utf-8")) for path in (args.candidate,args.repeat,args.failure)))
        args.output.write_text(json.dumps(receipt,separators=(",",":"))+"\n",encoding="utf-8")
    except (OSError,ValueError,json.JSONDecodeError) as error: print(f"error: {error}",file=sys.stderr); return 1
    print("wrote one inert source-free scene lifecycle receipt"); return 0
if __name__ == "__main__": raise SystemExit(main())
