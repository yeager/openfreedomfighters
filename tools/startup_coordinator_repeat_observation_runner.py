#!/usr/bin/env python3
"""Collect a reviewed private startup-coordinator pass contract twice.

This is a narrow clean-room boundary for an operator-supplied observer.  It
never locates a game, process, debugger, dump, or target.  Each observer is
started once in literal ``fresh-isolated`` mode in a distinct newly-created
private workspace.  Raw observer files are consumed only through no-follow
descriptors, erased before return, and never copied to the contract output.
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

import startup_coordinator_observer_probe_plan as probe_plan
import startup_coordinator_pass_contract_bundle as bundle
import startup_coordinator_pass_selection_repeat_pair as repeat_pair
import startup_coordinator_pass_selection_trace as trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
SUCCESS_RAW_NAME = "successful-pass.raw.json"
FAILURE_RAW_NAME = "rejected-pass.raw.json"
_RAW_NAMES = (SUCCESS_RAW_NAME, FAILURE_RAW_NAME)
_EXPECTED_RAW_FILES = frozenset(_RAW_NAMES)
MAX_RAW_RECORD_BYTES = 8 * 1024 * 1024
DEFAULT_TIMEOUT_SECONDS = 300
MAX_TIMEOUT_SECONDS = 1800


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    """Resolve a private path without accepting traversal or symlink hops."""
    if str(path).startswith("//"):
        raise ValueError(f"{label} must not use a double-slash path")
    if ".." in path.parts:
        raise ValueError(f"{label} must not contain parent traversal")
    absolute = path if path.is_absolute() else pathlib.Path.cwd() / path
    current = pathlib.Path(absolute.anchor)
    for component in absolute.parts[1:]:
        current /= component
        if current.exists() and current.is_symlink():
            raise ValueError(f"{label} must not traverse a symlink")
    resolved = absolute.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _open_regular_json_no_follow(path: pathlib.Path, label: str) -> Any:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError(f"platform cannot safely read {label}")
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NONBLOCK | no_follow)
    except OSError as error:
        raise ValueError(f"{label} must be a bounded regular private file") from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_RAW_RECORD_BYTES:
            raise ValueError(f"{label} must be a bounded regular private file")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(descriptor)


def _canonical_plan(path: pathlib.Path) -> pathlib.Path:
    if not path.is_file() or path.is_symlink():
        raise ValueError("canonical plan must be a regular private file")
    raw = _open_regular_json_no_follow(path, "canonical plan")
    if not isinstance(raw, dict) or frozenset(raw) != {"format", "probes"}:
        raise ValueError("canonical plan has an unsupported field")
    if raw["format"] != probe_plan.OUTPUT_FORMAT:
        raise ValueError("canonical plan must use the reviewed format")
    probe_plan.validate_probe_plan({"format": probe_plan.INPUT_FORMAT, "probes": raw["probes"]})
    return path


def _validate_observer(observer: pathlib.Path) -> pathlib.Path:
    if not observer.is_file() or observer.is_symlink() or not os.access(observer, os.X_OK):
        raise ValueError("observer must be an executable regular file outside the repository")
    return observer


def _make_workspace(workspace: pathlib.Path) -> None:
    if workspace.exists() or not workspace.parent.is_dir() or workspace.parent.is_symlink():
        raise ValueError("private observation workspace must be new under an existing private directory")
    workspace.mkdir(mode=0o700)
    if workspace.is_symlink() or not workspace.is_dir():
        raise ValueError("private observation workspace must be a real directory")


def _open_workspace(workspace: pathlib.Path) -> int:
    no_follow, directory = getattr(os, "O_NOFOLLOW", None), getattr(os, "O_DIRECTORY", None)
    if no_follow is None or directory is None:
        raise ValueError("platform cannot safely open observer workspaces")
    try:
        descriptor = os.open(workspace, os.O_RDONLY | directory | no_follow)
    except OSError as error:
        raise ValueError("private observation workspace must be a real directory") from error
    if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise ValueError("private observation workspace must be a real directory")
    return descriptor


def _read_workspace_json(directory: int, name: str) -> Any:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError("platform cannot safely read observer records")
    try:
        descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | no_follow, dir_fd=directory)
    except OSError as error:
        raise ValueError("observer records must be bounded regular files") from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_RAW_RECORD_BYTES:
            raise ValueError("observer records must be bounded regular files")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(descriptor)


def _discard_raw_records(workspace: pathlib.Path) -> None:
    try:
        directory = _open_workspace(workspace)
    except ValueError:
        return
    try:
        for name in _RAW_NAMES:
            try:
                os.unlink(name, dir_fd=directory)
            except FileNotFoundError:
                pass
    finally:
        os.close(directory)


def _collect_one(workspace: pathlib.Path) -> tuple[dict[str, Any], dict[str, Any]]:
    directory = _open_workspace(workspace)
    try:
        try:
            if frozenset(os.listdir(directory)) != _EXPECTED_RAW_FILES:
                raise ValueError("observer workspace must contain exactly two structural records")
            success = trace.sanitize_trace(_read_workspace_json(directory, SUCCESS_RAW_NAME))
            failure = trace.sanitize_trace(_read_workspace_json(directory, FAILURE_RAW_NAME))
        finally:
            for name in _RAW_NAMES:
                try:
                    os.unlink(name, dir_fd=directory)
                except FileNotFoundError:
                    pass
        return success, failure
    finally:
        os.close(directory)


def _run_one(*, observer: pathlib.Path, plan: pathlib.Path, workspace: pathlib.Path,
             timeout_seconds: int, run: Callable[..., subprocess.CompletedProcess[Any]]) -> tuple[dict[str, Any], dict[str, Any]]:
    _make_workspace(workspace)
    command: Sequence[str] = (
        str(observer), "--mode", "fresh-isolated", "--probe-plan", str(plan),
        "--success-output", str(workspace / SUCCESS_RAW_NAME),
        "--failure-output", str(workspace / FAILURE_RAW_NAME),
    )
    try:
        run(command, check=True, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, shell=False, timeout=timeout_seconds)
        return _collect_one(workspace)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        _discard_raw_records(workspace)
        raise ValueError("private observer did not complete") from error
    except (OSError, ValueError, json.JSONDecodeError):
        _discard_raw_records(workspace)
        raise


def execute_observation(*, observer: pathlib.Path, canonical_plan: pathlib.Path,
                        first_workspace: pathlib.Path, second_workspace: pathlib.Path,
                        output_directory: pathlib.Path,
                        timeout_seconds: int = DEFAULT_TIMEOUT_SECONDS,
                        run: Callable[..., subprocess.CompletedProcess[Any]] = subprocess.run,
                        ) -> dict[str, Any]:
    """Start two isolated observers and write exactly one reviewed receipt."""
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= MAX_TIMEOUT_SECONDS:
        raise ValueError("observer timeout must be between 1 and 1800 seconds")
    observer_path = _validate_observer(_outside_repository(observer, "observer"))
    plan_path = _canonical_plan(_outside_repository(canonical_plan, "canonical plan"))
    first = _outside_repository(first_workspace, "first workspace")
    second = _outside_repository(second_workspace, "second workspace")
    destination = _outside_repository(output_directory, "output directory")
    if len({first, second, destination}) != 3:
        raise ValueError("workspaces and output directory must differ")
    if not destination.is_dir() or destination.is_symlink():
        raise ValueError("output directory must be an existing private directory")
    output = destination / bundle.OUTPUT_NAME
    if output.exists():
        raise ValueError("refusing to overwrite private output")
    first_success, first_failure = _run_one(observer=observer_path, plan=plan_path, workspace=first,
                                            timeout_seconds=timeout_seconds, run=run)
    try:
        second_success, second_failure = _run_one(observer=observer_path, plan=plan_path, workspace=second,
                                                   timeout_seconds=timeout_seconds, run=run)
    except (OSError, ValueError, json.JSONDecodeError):
        _discard_raw_records(first)
        raise
    # Both result classes must agree across independent observer processes.
    repeat_pair.sanitize_repeat_pair(first_success, second_success)
    repeat_pair.sanitize_repeat_pair(first_failure, second_failure)
    receipt = bundle.bundle_contract(first_success, second_success, first_failure)
    bundle._write_new_private_json_no_follow(output, receipt)
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--observer", type=pathlib.Path)
    parser.add_argument("--canonical-plan", type=pathlib.Path)
    parser.add_argument("--first-workspace", type=pathlib.Path)
    parser.add_argument("--second-workspace", type=pathlib.Path)
    parser.add_argument("--output-directory", type=pathlib.Path)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    args = parser.parse_args()
    if not args.execute:
        parser.error("refusing to run an observer without --execute")
    required = (args.observer, args.canonical_plan, args.first_workspace, args.second_workspace,
                args.output_directory)
    if any(value is None for value in required):
        parser.error("--observer, --canonical-plan, both workspaces, and --output-directory are required")
    try:
        execute_observation(observer=args.observer, canonical_plan=args.canonical_plan,
                            first_workspace=args.first_workspace, second_workspace=args.second_workspace,
                            output_directory=args.output_directory, timeout_seconds=args.timeout_seconds)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one inert source-free reviewed startup coordinator pass receipt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
