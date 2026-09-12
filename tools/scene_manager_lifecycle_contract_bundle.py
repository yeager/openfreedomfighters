#!/usr/bin/env python3
"""Bundle repeatable source-free scene lifecycle evidence into an inert receipt."""
from __future__ import annotations
import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any
import scene_manager_lifecycle_trace as trace
OUTPUT_FORMAT = "off.scene-manager-lifecycle-contract/v1"
MAX_PRIVATE_RECORD_BYTES = 8 * 1024 * 1024
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent

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


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    absolute = pathlib.Path(os.path.abspath(path))
    current = pathlib.Path(absolute.anchor)
    for part in absolute.parts[1:]:
        current /= part
        try:
            metadata = os.lstat(current)
        except FileNotFoundError:
            break
        if stat.S_ISLNK(metadata.st_mode):
            raise ValueError(f"{label} must not contain a symlink")
    resolved = absolute.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _open_parent_no_follow(path: pathlib.Path, label: str) -> tuple[int, str]:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    directory = getattr(os, "O_DIRECTORY", None)
    if no_follow is None or directory is None:
        raise ValueError(f"platform cannot safely open {label}")
    try:
        descriptor = os.open(path.parent, os.O_RDONLY | directory | no_follow)
    except OSError as error:
        raise ValueError(f"{label} parent must be an existing real directory") from error
    if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise ValueError(f"{label} parent must be an existing real directory")
    return descriptor, path.name


def _read_private_json_no_follow(path: pathlib.Path, label: str) -> Any:
    directory, name = _open_parent_no_follow(path, label)
    try:
        try:
            descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW,
                                 dir_fd=directory)
        except OSError as error:
            raise ValueError(f"{label} must be a bounded regular private file") from error
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_PRIVATE_RECORD_BYTES:
                raise ValueError(f"{label} must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
                return json.load(stream)
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def _write_new_private_json_no_follow(path: pathlib.Path, record: dict[str, Any]) -> None:
    directory, name = _open_parent_no_follow(path, "output")
    try:
        try:
            descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                 0o600, dir_fd=directory)
        except OSError as error:
            raise ValueError("refusing to overwrite private output") from error
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
                json.dump(record, stream, separators=(",", ":"))
                stream.write("\n")
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument("candidate",type=pathlib.Path); parser.add_argument("repeat",type=pathlib.Path); parser.add_argument("failure",type=pathlib.Path); parser.add_argument("output",type=pathlib.Path); args=parser.parse_args()
    try:
        paths = [_outside_repository(getattr(args, name), name) for name in ("candidate", "repeat", "failure", "output")]
        if len(set(paths)) != len(paths):
            raise ValueError("private inputs and new output must be distinct")
        if paths[-1].exists():
            raise ValueError("refusing to overwrite private output")
        receipt = bundle_contract(*(
            _read_private_json_no_follow(path, label)
            for path, label in zip(paths[:-1], ("candidate", "repeat", "failure"))))
        _write_new_private_json_no_follow(paths[-1], receipt)
    except (OSError,ValueError,json.JSONDecodeError) as error: print(f"error: {error}",file=sys.stderr); return 1
    print("wrote one inert source-free scene lifecycle receipt"); return 0
if __name__ == "__main__": raise SystemExit(main())
