#!/usr/bin/env python3
"""Run one explicit fresh-isolated private startup-coordinator observation.

The external observer is supplied by the operator.  This wrapper does not
discover or attach to a target. It supplies only a fixed canonical plan and a
new private workspace, retains only sanitized structural records, and never
relays observer output.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import subprocess
import sys
from typing import Any, Callable, Sequence

import startup_coordinator_pass_selection_trace as trace
import startup_coordinator_observer_probe_plan as probe_plan


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
SUCCESS_RAW_NAME = "successful-pass.raw.json"
FAILURE_RAW_NAME = "rejected-pass.raw.json"
SUCCESS_SANITIZED_NAME = "successful-pass.sanitized.json"
FAILURE_SANITIZED_NAME = "rejected-pass.sanitized.json"
_EXPECTED_FILES = frozenset((SUCCESS_RAW_NAME, FAILURE_RAW_NAME))
MAX_RAW_RECORD_BYTES = 8 * 1024 * 1024
DEFAULT_TIMEOUT_SECONDS = 300
MAX_TIMEOUT_SECONDS = 1800


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _canonical_probe_plan(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != frozenset(("format", "probes")):
        raise ValueError("probe plan has an unsupported field")
    if raw["format"] != probe_plan.OUTPUT_FORMAT:
        raise ValueError("probe plan must be canonical")
    return probe_plan.validate_probe_plan({"format": probe_plan.INPUT_FORMAT,
                                            "probes": raw["probes"]})


def _read_regular_json_path_no_follow(path: pathlib.Path, label: str) -> Any:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError(f"platform cannot safely read {label}")
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | no_follow)
    except OSError as error:
        raise ValueError(f"{label} must be a regular private file") from error
    try:
        metadata = os.fstat(fd)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_RAW_RECORD_BYTES:
            raise ValueError(f"{label} must be a bounded regular private file")
        with os.fdopen(fd, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(fd)


def _make_workspace(workspace: pathlib.Path) -> None:
    if workspace.exists():
        raise ValueError("private observation workspace must be new")
    workspace.mkdir(mode=0o700, parents=True)
    if workspace.is_symlink() or not workspace.is_dir():
        raise ValueError("private observation workspace must be a real directory")


def _validate_observer_path(observer: pathlib.Path) -> pathlib.Path:
    resolved = _outside_repository(observer, "observer")
    if not resolved.is_file() or resolved.is_symlink() or not os.access(resolved, os.X_OK):
        raise ValueError("observer must be an executable regular file outside the repository")
    return resolved


def _open_workspace_no_follow(workspace: pathlib.Path) -> int:
    flags = getattr(os, "O_DIRECTORY", None)
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if flags is None or no_follow is None:
        raise ValueError("platform cannot safely open observer workspaces")
    try:
        fd = os.open(workspace, os.O_RDONLY | flags | no_follow)
    except OSError as error:
        raise ValueError("private observation workspace must be a real directory") from error
    if not stat.S_ISDIR(os.fstat(fd).st_mode):
        os.close(fd)
        raise ValueError("private observation workspace must be a real directory")
    return fd


def _read_workspace_json(fd: int, name: str) -> Any:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError("platform cannot safely read observer records")
    try:
        record_fd = os.open(name, os.O_RDONLY | os.O_NONBLOCK | no_follow, dir_fd=fd)
    except OSError as error:
        raise ValueError("observer records must be regular files") from error
    try:
        metadata = os.fstat(record_fd)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_RAW_RECORD_BYTES:
            raise ValueError("observer records must be bounded regular files")
        with os.fdopen(record_fd, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(record_fd)


def _write_workspace_json(fd: int, name: str, record: dict[str, Any]) -> None:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError("platform cannot safely write observer records")
    try:
        output_fd = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | no_follow,
                            0o600, dir_fd=fd)
    except OSError as error:
        raise ValueError("refusing to overwrite observer output") from error
    try:
        with os.fdopen(output_fd, "w", encoding="utf-8", closefd=False) as stream:
            json.dump(record, stream, indent=2)
            stream.write("\n")
    finally:
        os.close(output_fd)


def _discard_raw_records(workspace: pathlib.Path) -> None:
    try:
        fd = _open_workspace_no_follow(workspace)
    except ValueError:
        return
    try:
        for name in _EXPECTED_FILES:
            try:
                os.unlink(name, dir_fd=fd)
            except FileNotFoundError:
                pass
    finally:
        os.close(fd)


def _validate_relation(success: dict[str, Any], failure: dict[str, Any]) -> None:
    success_terminal = [event for event in success["events"] if event["phase"] == "completion"]
    failure_terminal = [event for event in failure["events"] if event["phase"] == "failure"]
    if len(success_terminal) != 1 or success_terminal[0]["outcome"] != "success":
        raise ValueError("success record must contain exactly one completed coordinator pass")
    if len(failure_terminal) != 1 or failure_terminal[0]["outcome"] != "failure":
        raise ValueError("failure record must contain exactly one rejected coordinator pass")
    if success_terminal[0]["callback_ordinal"] == failure_terminal[0]["callback_ordinal"]:
        raise ValueError("success and failure records must have distinct observer-local callbacks")


def _collect(workspace: pathlib.Path) -> tuple[dict[str, Any], dict[str, Any]]:
    fd = _open_workspace_no_follow(workspace)
    try:
        try:
            if frozenset(os.listdir(fd)) != _EXPECTED_FILES:
                raise ValueError("observer workspace must contain exactly two structural records")
            success = trace.sanitize_trace(_read_workspace_json(fd, SUCCESS_RAW_NAME))
            failure = trace.sanitize_trace(_read_workspace_json(fd, FAILURE_RAW_NAME))
            _validate_relation(success, failure)
        finally:
            for name in _EXPECTED_FILES:
                try:
                    os.unlink(name, dir_fd=fd)
                except FileNotFoundError:
                    pass
        _write_workspace_json(fd, SUCCESS_SANITIZED_NAME, success)
        _write_workspace_json(fd, FAILURE_SANITIZED_NAME, failure)
        return success, failure
    finally:
        os.close(fd)


def execute_observation(*, observer: pathlib.Path, canonical_plan: pathlib.Path,
                        workspace: pathlib.Path, timeout_seconds: int = DEFAULT_TIMEOUT_SECONDS,
                        run: Callable[..., subprocess.CompletedProcess[Any]] = subprocess.run,
                        ) -> tuple[dict[str, Any], dict[str, Any]]:
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= MAX_TIMEOUT_SECONDS:
        raise ValueError("observer timeout must be between 1 and 1800 seconds")
    observer_path = _validate_observer_path(observer)
    plan_path, workspace_path = (_outside_repository(canonical_plan, "canonical plan"),
                                 _outside_repository(workspace, "workspace"))
    if not plan_path.is_file():
        raise ValueError("canonical plan must be a regular private file")
    _canonical_probe_plan(_read_regular_json_path_no_follow(plan_path, "canonical plan"))
    _make_workspace(workspace_path)
    command: Sequence[str] = (str(observer_path), "--mode", "fresh-isolated", "--probe-plan",
                              str(plan_path), "--success-output",
                              str(workspace_path / SUCCESS_RAW_NAME), "--failure-output",
                              str(workspace_path / FAILURE_RAW_NAME))
    try:
        run(command, check=True, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, shell=False, timeout=timeout_seconds)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        _discard_raw_records(workspace_path)
        raise ValueError("private observer did not complete") from error
    try:
        return _collect(workspace_path)
    except (OSError, ValueError, json.JSONDecodeError):
        _discard_raw_records(workspace_path)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--observer", type=pathlib.Path)
    parser.add_argument("--canonical-plan", type=pathlib.Path)
    parser.add_argument("--workspace", type=pathlib.Path)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    args = parser.parse_args()
    if not args.execute:
        parser.error("refusing to run an observer without --execute")
    if args.observer is None or args.canonical_plan is None or args.workspace is None:
        parser.error("--observer, --canonical-plan, and --workspace are required with --execute")
    try:
        success, failure = execute_observation(observer=args.observer, canonical_plan=args.canonical_plan,
                                               workspace=args.workspace, timeout_seconds=args.timeout_seconds)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("collected one isolated source-free startup coordinator observation "
          f"({len(success['events'])} success, {len(failure['events'])} failure records)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
