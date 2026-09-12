#!/usr/bin/env python3
"""Run one explicitly requested isolated private first-mission observation.

The observer is an operator-supplied private executable.  This wrapper has no
target discovery, process attachment, game path, executable path, or debugger
arguments.  It starts the observer only in the literal ``fresh-isolated``
mode, accepts exactly the three narrow structural records needed by the
existing repeat gate, removes their raw forms, and retains only sanitized
records plus the aggregate review bundle.
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

import first_mission_observation_repeat_bundle as repeat_bundle
import first_mission_observation_trace as trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
FIRST_BASELINE_RAW_NAME = "baseline-a.raw.json"
SECOND_BASELINE_RAW_NAME = "baseline-b.raw.json"
EXPERIMENT_RAW_NAME = "one-probe.raw.json"
FIRST_BASELINE_SANITIZED_NAME = "baseline-a.sanitized.json"
SECOND_BASELINE_SANITIZED_NAME = "baseline-b.sanitized.json"
EXPERIMENT_SANITIZED_NAME = "one-probe.sanitized.json"
BUNDLE_NAME = "review-bundle.json"
_RAW_NAMES = (FIRST_BASELINE_RAW_NAME, SECOND_BASELINE_RAW_NAME, EXPERIMENT_RAW_NAME)
_SANITIZED_NAMES = (
    FIRST_BASELINE_SANITIZED_NAME,
    SECOND_BASELINE_SANITIZED_NAME,
    EXPERIMENT_SANITIZED_NAME,
)
_EXPECTED_RAW_FILES = frozenset(_RAW_NAMES)
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


def _validate_observer_path(observer: pathlib.Path) -> pathlib.Path:
    resolved = _outside_repository(observer, "observer")
    if not resolved.is_file() or resolved.is_symlink() or not os.access(resolved, os.X_OK):
        raise ValueError("observer must be an executable regular file outside the repository")
    return resolved


def _make_workspace(workspace: pathlib.Path) -> None:
    if workspace.exists():
        raise ValueError("private observation workspace must be new")
    workspace.mkdir(mode=0o700, parents=True)
    if workspace.is_symlink() or not workspace.is_dir():
        raise ValueError("private observation workspace must be a real directory")


def _open_workspace_no_follow(workspace: pathlib.Path) -> int:
    directory = getattr(os, "O_DIRECTORY", None)
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if directory is None or no_follow is None:
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
        raise ValueError("observer records must be regular files") from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_RAW_RECORD_BYTES:
            raise ValueError("observer records must be bounded regular files")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(descriptor)


def _write_workspace_json(directory: int, name: str, record: dict[str, Any]) -> None:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError("platform cannot safely write observer records")
    try:
        descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | no_follow,
                             0o600, dir_fd=directory)
    except OSError as error:
        raise ValueError("refusing to overwrite observer output") from error
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
            json.dump(record, stream, indent=2)
            stream.write("\n")
    finally:
        os.close(descriptor)


def _discard_raw_records(workspace: pathlib.Path) -> None:
    try:
        descriptor = _open_workspace_no_follow(workspace)
    except ValueError:
        return
    try:
        for name in _RAW_NAMES:
            try:
                os.unlink(name, dir_fd=descriptor)
            except FileNotFoundError:
                pass
    finally:
        os.close(descriptor)


def _collect(workspace: pathlib.Path) -> dict[str, Any]:
    descriptor = _open_workspace_no_follow(workspace)
    try:
        try:
            if frozenset(os.listdir(descriptor)) != _EXPECTED_RAW_FILES:
                raise ValueError("observer workspace must contain exactly three structural records")
            first, second, experiment = (
                trace.sanitize_trace(_read_workspace_json(descriptor, name)) for name in _RAW_NAMES)
            aggregate = repeat_bundle.sanitize_repeat_bundle(first, second, experiment)
        finally:
            for name in _RAW_NAMES:
                try:
                    os.unlink(name, dir_fd=descriptor)
                except FileNotFoundError:
                    pass
        for name, record in zip(_SANITIZED_NAMES, (first, second, experiment), strict=True):
            _write_workspace_json(descriptor, name, record)
        _write_workspace_json(descriptor, BUNDLE_NAME, aggregate)
        return aggregate
    finally:
        os.close(descriptor)


def execute_observation(*, observer: pathlib.Path, workspace: pathlib.Path,
                        timeout_seconds: int = DEFAULT_TIMEOUT_SECONDS,
                        run: Callable[..., subprocess.CompletedProcess[Any]] = subprocess.run,
                        ) -> dict[str, Any]:
    """Start only an external fresh-process observer and retain aggregate evidence."""
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= MAX_TIMEOUT_SECONDS:
        raise ValueError("observer timeout must be between 1 and 1800 seconds")
    observer_path = _validate_observer_path(observer)
    workspace_path = _outside_repository(workspace, "workspace")
    _make_workspace(workspace_path)
    command: Sequence[str] = (
        str(observer_path), "--mode", "fresh-isolated",
        "--first-baseline-output", str(workspace_path / FIRST_BASELINE_RAW_NAME),
        "--second-baseline-output", str(workspace_path / SECOND_BASELINE_RAW_NAME),
        "--experiment-output", str(workspace_path / EXPERIMENT_RAW_NAME),
    )
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
    parser.add_argument("--workspace", type=pathlib.Path)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    args = parser.parse_args()
    if not args.execute:
        parser.error("refusing to run an observer without --execute")
    if args.observer is None or args.workspace is None:
        parser.error("--observer and --workspace are required with --execute")
    try:
        aggregate = execute_observation(observer=args.observer, workspace=args.workspace,
                                        timeout_seconds=args.timeout_seconds)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("collected one isolated source-free first-mission observation "
          f"({aggregate['baseline_run_count']} baselines, {aggregate['experiment_run_count']} experiment)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
