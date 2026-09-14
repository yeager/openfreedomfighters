#!/usr/bin/env python3
"""Join source-free MovieControl and first-cut lifecycle evidence privately.

This is a review gate, not a runtime switch.  It consumes only already
sanitized structural records produced by the narrow MovieControl and player
schemas.  It never accepts raw observer records and has no fields for retail
content, process identities, addresses, paths, text, bytes, or timings.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

import cut_sequence_player_lifecycle_trace as player_trace
import movie_control_cutscene_dispatch_contract_bundle as dispatch_bundle
import movie_control_phase_one_contract_bundle as phase_bundle
import private_structural_json


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
OUTPUT_FORMAT = "off.movie-control-cutscene-lifecycle-contract-bundle/v1"
MAX_PRIVATE_RECORD_BYTES = 8 * 1024 * 1024


def _exact(value: Any, fields: frozenset[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict) or frozenset(value) != fields:
        raise ValueError(f"{label} has an unsupported field")
    return value


def _phase_contract(raw: Any) -> dict[str, Any]:
    value = _exact(raw, frozenset(("format", "candidate", "failure")), "phase-one contract")
    if value["format"] != phase_bundle.OUTPUT_FORMAT:
        raise ValueError("unrecognized phase-one contract format")
    candidate = phase_bundle._validated_success_candidate({
        "format": phase_bundle.SUCCESS_FORMAT, "candidate": value["candidate"],
    })
    failure_event = {**value["failure"], "phase": 1}
    failure = phase_bundle._failure_relation(phase_bundle._validated_failure_events({
        "format": phase_bundle.FAILURE_FORMAT, "events": [failure_event],
    })[0])
    if not phase_bundle._is_matching_failure({**failure, "phase": 1}, candidate):
        raise ValueError("phase-one contract failure does not match its candidate")
    return {"candidate": candidate, "failure": failure}


def _dispatch_contract(raw: Any) -> dict[str, Any]:
    value = _exact(raw, frozenset(("format", "candidate", "failure")), "dispatch contract")
    if value["format"] != dispatch_bundle.OUTPUT_FORMAT:
        raise ValueError("unrecognized dispatch contract format")
    candidate = dispatch_bundle._validated_success_events({
        "format": dispatch_bundle.SUCCESS_FORMAT, "events": [value["candidate"]],
    })[0]
    failure = dispatch_bundle._validated_failure_events({
        "format": dispatch_bundle.FAILURE_FORMAT, "events": [value["failure"]],
    })[0]
    if not dispatch_bundle._is_success_route(candidate) or not dispatch_bundle._is_matching_failure(failure, candidate):
        raise ValueError("dispatch contract lacks one matching successful and failed route")
    return {"candidate": candidate, "failure": failure}


def _sanitized_player_trace(raw: Any, label: str) -> dict[str, Any]:
    value = _exact(raw, frozenset(("format", "events")), label)
    if value["format"] != player_trace.OUTPUT_FORMAT:
        raise ValueError(f"unrecognized {label} format")
    return player_trace.sanitize_trace({"format": player_trace.INPUT_FORMAT, "events": value["events"]})


def _successful_player_route(events: list[dict[str, Any]], callback: int) -> list[dict[str, Any]]:
    required = (
        ("phase_one", "cold", "phase_one_ready", "open", "not_attempted", "not_observed"),
        ("phase_two", "phase_one_ready", "phase_two_ready", "sealed", "not_attempted", "not_observed"),
        ("activation", "phase_two_ready", "active", "sealed", "started", "pending"),
        ("completion", "active", "completed", "sealed", "not_attempted", "completed"),
    )
    found: list[dict[str, Any]] = []
    for phase, before, after, receiver, activation, completion in required:
        candidates = [event for event in events if event["phase"] == phase and event["callback_ordinal"] == callback]
        if len(candidates) != 1:
            raise ValueError("player success trace must contain one complete route per lifecycle phase")
        event = candidates[0]
        if (not event["sequence_component_constructed"] or not event["sequence_owner_constructed"] or
                event["reader_graph_receipt"] != "complete" or event["player_state_before"] != before or
                event["player_state_after"] != after or event["receiver_state"] != receiver or
                event["activation"] != activation or event["completion"] != completion or
                event["outcome"] != "success"):
            raise ValueError("player success route has an incomplete lifecycle relation")
        found.append(event)
    phase_two = found[1]
    if phase_two["member_sweep"] != "derived" or phase_two["reference_sweep"] != "camera_and_sequence":
        raise ValueError("player success route lacks the completed phase-two sweep")
    return found


def _matching_player_failure(events: list[dict[str, Any]], callback: int, activation: dict[str, Any]) -> dict[str, Any]:
    matches = [event for event in events if event["callback_ordinal"] == callback and event["phase"] == "failure"]
    if len(matches) != 1:
        raise ValueError("player failure trace must contain exactly one matching failure boundary")
    failure = matches[0]
    if (not failure["sequence_component_constructed"] or not failure["sequence_owner_constructed"] or
            failure["outcome"] != "failure" or failure["completion"] == "completed" or
            failure["player_state_before"] != activation["player_state_before"] or
            failure["component_status_before"] != activation["component_status_before"] or
            failure["owner_status_before"] != activation["owner_status_before"]):
        raise ValueError("player failure route does not share the activation preconditions")
    if any(event["callback_ordinal"] == callback and event["completion"] == "completed" for event in events):
        raise ValueError("player failure trace also completes the matching route")
    return failure


def sanitize_contract_bundle(phase: Any, dispatch: Any, first_player_success: Any,
                             second_player_success: Any, player_failure: Any) -> dict[str, Any]:
    """Prove one repeatable end-to-end lifecycle route and one failure path."""
    phase_clean = _phase_contract(phase)
    dispatch_clean = _dispatch_contract(dispatch)
    phase_candidate, dispatch_candidate = phase_clean["candidate"], dispatch_clean["candidate"]
    if phase_candidate["callback_ordinal"] != dispatch_candidate["callback_ordinal"]:
        raise ValueError("phase-one and dispatch contracts are not tied to one callback")
    first = _sanitized_player_trace(first_player_success, "first player success trace")
    second = _sanitized_player_trace(second_player_success, "second player success trace")
    if first["events"] != second["events"]:
        raise ValueError("player success observations disagree on the lifecycle trace")
    callback = dispatch_candidate["callback_ordinal"]
    route = _successful_player_route(first["events"], callback)
    failure = _matching_player_failure(
        _sanitized_player_trace(player_failure, "player failure trace")["events"], callback, route[2])
    return {
        "format": OUTPUT_FORMAT,
        "phase_one": phase_clean,
        "dispatch": dispatch_clean,
        "player_route": route,
        "player_failure": failure,
    }


def validate_contract_receipt(raw: Any) -> dict[str, Any]:
    """Revalidate one final lifecycle receipt before it reaches local admission.

    The final bundle intentionally omits the intermediate format tags.  Put
    them back only long enough to reuse the narrow schema authorities above,
    then require a byte-for-structure canonical result.  This is an offline
    review/import boundary, never a runtime switch.
    """
    value = _exact(raw, frozenset(("format", "phase_one", "dispatch",
                                  "player_route", "player_failure")),
                   "lifecycle receipt")
    if value["format"] != OUTPUT_FORMAT:
        raise ValueError("unrecognized lifecycle receipt format")
    if not isinstance(value["player_route"], list):
        raise ValueError("lifecycle receipt player route must be an array")
    canonical = sanitize_contract_bundle(
        {"format": phase_bundle.OUTPUT_FORMAT, **value["phase_one"]}
        if isinstance(value["phase_one"], dict) else value["phase_one"],
        {"format": dispatch_bundle.OUTPUT_FORMAT, **value["dispatch"]}
        if isinstance(value["dispatch"], dict) else value["dispatch"],
        {"format": player_trace.OUTPUT_FORMAT, "events": value["player_route"]},
        {"format": player_trace.OUTPUT_FORMAT, "events": value["player_route"]},
        {"format": player_trace.OUTPUT_FORMAT, "events": [value["player_failure"]]},
    )
    if canonical != value:
        raise ValueError("lifecycle receipt is not canonical")
    return canonical


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _private_parent(path: pathlib.Path, label: str) -> None:
    parent = path.parent
    if parent.is_symlink() or not parent.is_dir():
        raise ValueError(f"{label} parent must be an existing private directory")


def _read_private_json_no_follow(path: pathlib.Path, label: str) -> Any:
    """Read one bounded regular private input without following its final entry."""
    _private_parent(path, label)
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError(f"platform cannot safely read {label}")
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NONBLOCK | no_follow)
    except OSError as error:
        raise ValueError(f"{label} must be a regular private file") from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_PRIVATE_RECORD_BYTES:
            raise ValueError(f"{label} must be a bounded regular private file")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream, object_pairs_hook=private_structural_json.strict_json_object)
    finally:
        os.close(descriptor)


def _write_new_private_json_no_follow(path: pathlib.Path, record: dict[str, Any]) -> None:
    """Create one owner-only result without following or replacing an entry."""
    _private_parent(path, "output")
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError("platform cannot safely write output")
    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | no_follow, 0o600)
    except OSError as error:
        raise ValueError("refusing to overwrite private output") from error
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
            json.dump(record, stream, indent=2)
            stream.write("\n")
    finally:
        os.close(descriptor)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", type=pathlib.Path)
    parser.add_argument("dispatch", type=pathlib.Path)
    parser.add_argument("first_player_success", type=pathlib.Path)
    parser.add_argument("second_player_success", type=pathlib.Path)
    parser.add_argument("player_failure", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        paths = [_outside_repository(getattr(args, name), name.replace("_", " ")) for name in (
            "phase", "dispatch", "first_player_success", "second_player_success", "player_failure", "output")]
        if len(set(paths)) != len(paths) or paths[-1].exists():
            raise ValueError("private inputs and new output must be distinct")
        result = sanitize_contract_bundle(*(
            _read_private_json_no_follow(path, label)
            for path, label in zip(paths[:-1], ("phase", "dispatch", "first player success",
                                                 "second player success", "player failure"), strict=True)))
        _write_new_private_json_no_follow(paths[-1], result)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one source-free MovieControl-to-player lifecycle contract bundle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
