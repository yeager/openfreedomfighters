#!/usr/bin/env python3
"""Build one inert private coordinator receipt from reviewed trace evidence.

This is a source-free boundary adapter.  It accepts already-sanitized records
only, requires two identical completed observations plus one structurally
separate rejected observation, and emits the fixed receipt consumed by the
native inert contract loader.  It never opens game data and cannot enable a
startup, scene, camera, renderer, or intro path.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

import startup_coordinator_pass_selection_repeat_pair as repeat_pair
import startup_coordinator_pass_selection_trace as trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
OUTPUT_FORMAT = "off.startup-coordinator-pass-contract/v1"
OUTPUT_NAME = "reviewed-startup-coordinator-pass.json"
MAX_PRIVATE_RECORD_BYTES = 8 * 1024 * 1024


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _private_parent(path: pathlib.Path, label: str) -> None:
    if path.parent.is_symlink() or not path.parent.is_dir():
        raise ValueError(f"{label} parent must be an existing private directory")


def _read_private_json_no_follow(path: pathlib.Path, label: str) -> Any:
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
            return json.load(stream)
    finally:
        os.close(descriptor)


def _write_new_private_json_no_follow(path: pathlib.Path, record: dict[str, Any]) -> None:
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
            json.dump(record, stream, separators=(",", ":"))
            stream.write("\n")
    finally:
        os.close(descriptor)


def _sanitized(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != frozenset(("format", "events")):
        raise ValueError("evidence must be a sanitized coordinator trace")
    if raw["format"] != trace.OUTPUT_FORMAT:
        raise ValueError("evidence must be a sanitized coordinator trace")
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": raw["events"]})


def _one_terminal(events: list[dict[str, Any]], phase: str, outcome: str) -> None:
    terminals = [event for event in events if event["phase"] in ("completion", "failure")]
    if len(terminals) != 1 or terminals[0]["phase"] != phase or terminals[0]["outcome"] != outcome:
        raise ValueError("evidence must contain exactly its expected terminal coordinator boundary")


def _receipt_record(*, admitted: bool) -> dict[str, Any]:
    return {
        "pass_order": 0,
        "coordinator_constructed": True,
        "manager_constructed": True,
        "pass_entered": True,
        "work_list_ready": True,
        "selected_work": "startup_picture",
        "work_admission": "admitted" if admitted else "rejected",
        "pass_completed": admitted,
        "outcome": "success" if admitted else "failure",
        "external_service": "entered",
    }


def bundle_contract(candidate_raw: Any, repeat_raw: Any, failure_raw: Any) -> dict[str, Any]:
    """Return the fixed native receipt only for reviewed source-free evidence."""
    candidate = _sanitized(candidate_raw)
    repeat = _sanitized(repeat_raw)
    failure = _sanitized(failure_raw)
    successful_pair = repeat_pair.sanitize_repeat_pair(candidate, repeat)
    _one_terminal(successful_pair["events"], "completion", "success")
    _one_terminal(failure["events"], "failure", "failure")
    success_callback = successful_pair["events"][-1]["callback_ordinal"]
    failure_callback = failure["events"][-1]["callback_ordinal"]
    if success_callback == failure_callback:
        raise ValueError("successful and rejected evidence must use distinct observer-local callbacks")
    return {
        "format": OUTPUT_FORMAT,
        "candidate": _receipt_record(admitted=True),
        "repeat": _receipt_record(admitted=True),
        "failure": _receipt_record(admitted=False),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=pathlib.Path, help="first private sanitized completed trace")
    parser.add_argument("repeat", type=pathlib.Path, help="second private sanitized completed trace")
    parser.add_argument("failure", type=pathlib.Path, help="private sanitized rejected trace")
    parser.add_argument("output_directory", type=pathlib.Path, help="existing private receipt directory")
    args = parser.parse_args()
    try:
        candidate_path = _outside_repository(args.candidate, "candidate")
        repeat_path = _outside_repository(args.repeat, "repeat")
        failure_path = _outside_repository(args.failure, "failure")
        output_directory = _outside_repository(args.output_directory, "output directory")
        if not output_directory.is_dir() or output_directory.is_symlink():
            raise ValueError("output directory must be an existing private directory")
        output_path = output_directory / OUTPUT_NAME
        if len({candidate_path, repeat_path, failure_path, output_path}) != 4:
            raise ValueError("evidence inputs and output must differ")
        if output_path.exists():
            raise ValueError("refusing to overwrite private output")
        receipt = bundle_contract(_read_private_json_no_follow(candidate_path, "candidate"),
                                  _read_private_json_no_follow(repeat_path, "repeat"),
                                  _read_private_json_no_follow(failure_path, "failure"))
        _write_new_private_json_no_follow(output_path, receipt)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one inert source-free startup coordinator pass receipt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
