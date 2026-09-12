#!/usr/bin/env python3
"""Privately verify repeatable first-mission evidence without retaining traces.

The inputs must already have been sanitized by
``first_mission_observation_trace.py``.  This review-only gate requires two
identical clean baselines and one fresh one-probe input experiment under the
same declared metadata.  It returns only a compact source-free aggregate: no
raw trace rows, retail content, paths, process data, timings, screenshots, or
executable material are retained in its output.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

import first_mission_observation_trace as observation_trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = observation_trace.OUTPUT_FORMAT
OUTPUT_FORMAT = "off.first-mission-observation-repeat-bundle/v1"
MAX_PRIVATE_RECORD_BYTES = 8 * 1024 * 1024

_METADATA_FIELDS = (
    "method_version", "verified_data_manifest_fingerprint", "platform",
    "architecture", "input_device",
)
_TERMINAL_MISSION_STATES = frozenset(("loading", "failed", "completed"))


def _exact(value: Any, fields: frozenset[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict) or frozenset(value) != fields:
        raise ValueError(f"{label} has an unsupported field")
    return value


def _validated_sanitized_trace(raw: Any, label: str) -> dict[str, Any]:
    """Revalidate one sanitized record without accepting raw observer data."""
    trace = _exact(raw, frozenset((
        "format", "method_version", "verified_data_manifest_fingerprint", "platform",
        "architecture", "input_device", "run_kind", "events",
    )), label)
    if trace["format"] != INPUT_FORMAT:
        raise ValueError(f"unrecognized {label} format")
    return observation_trace.sanitize_trace({
        "format": observation_trace.INPUT_FORMAT,
        **{field: trace[field] for field in _METADATA_FIELDS},
        "run_kind": trace["run_kind"],
        "events": trace["events"],
    })


def _metadata(trace: dict[str, Any]) -> dict[str, Any]:
    return {field: trace[field] for field in _METADATA_FIELDS}


def _matching_metadata(*traces: dict[str, Any]) -> dict[str, Any]:
    first = _metadata(traces[0])
    if any(_metadata(trace) != first for trace in traces[1:]):
        raise ValueError("baseline and experiment observations must have identical metadata")
    return first


def _one_probe_experiment(trace: dict[str, Any]) -> tuple[str, dict[str, Any], dict[str, Any]]:
    if trace["run_kind"] != "input_experiment":
        raise ValueError("experiment must declare input_experiment")
    events = trace["events"]
    probes = {event["probe"] for event in events[1:]}
    if len(probes) != 1:
        raise ValueError("experiment must contain exactly one non-launch probe")
    probe = probes.pop()
    if probe in ("launch", "idle"):
        raise ValueError("experiment probe must be one controlled input or world probe")
    visible = [event for event in events[1:] if event["visible_change"]]
    if len(visible) != 1:
        raise ValueError("experiment must contain exactly one visible action outcome")
    outcome = visible[0]
    terminals = [event for event in events if (
        event["boundary"] == "mission" and event["state"] in _TERMINAL_MISSION_STATES and
        event["observation_order"] > outcome["observation_order"]
    )]
    if len(terminals) != 1:
        raise ValueError("experiment must contain exactly one reset or terminal mission outcome after the action")
    return probe, outcome, terminals[0]


def sanitize_repeat_bundle(first_baseline: Any, second_baseline: Any, experiment: Any) -> dict[str, Any]:
    """Return aggregate-only evidence for a repeated baseline and one probe."""
    first = _validated_sanitized_trace(first_baseline, "first baseline")
    second = _validated_sanitized_trace(second_baseline, "second baseline")
    trial = _validated_sanitized_trace(experiment, "experiment")
    if first["run_kind"] != "baseline" or second["run_kind"] != "baseline":
        raise ValueError("both repeat observations must declare baseline")
    if first != second:
        raise ValueError("baseline observations must agree exactly")
    metadata = _matching_metadata(first, second, trial)
    if trial["events"][0] != first["events"][0]:
        raise ValueError("experiment must retain the repeated baseline launch handoff")
    probe, outcome, terminal = _one_probe_experiment(trial)
    return {
        "format": OUTPUT_FORMAT,
        **metadata,
        "baseline_run_count": 2,
        "baseline_event_count": len(first["events"]),
        "baseline_visible_change_count": sum(event["visible_change"] for event in first["events"]),
        "experiment_run_count": 1,
        "experiment_probe": probe,
        "experiment_event_count": len(trial["events"]),
        "visible_action_outcome": {
            "boundary": outcome["boundary"], "state": outcome["state"],
        },
        "reset_or_terminal_outcome": {
            "boundary": terminal["boundary"], "state": terminal["state"],
        },
    }


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
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
                json.dump(record, stream, indent=2)
                stream.write("\n")
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first_baseline", type=pathlib.Path)
    parser.add_argument("second_baseline", type=pathlib.Path)
    parser.add_argument("experiment", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        paths = [_outside_repository(getattr(args, name), name.replace("_", " ")) for name in (
            "first_baseline", "second_baseline", "experiment", "output")]
        if len(set(paths)) != len(paths) or paths[-1].exists():
            raise ValueError("private inputs and new output must be distinct")
        result = sanitize_repeat_bundle(*(
            _read_private_json_no_follow(path, label) for path, label in zip(
                paths[:-1], ("first baseline", "second baseline", "experiment"), strict=True)))
        _write_new_private_json_no_follow(paths[-1], result)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one source-free first-mission repeat evidence bundle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
