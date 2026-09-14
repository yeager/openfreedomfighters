#!/usr/bin/env python3
"""Collect two private, fresh-isolated retail localization observations.

The supplied executable is an operator-maintained private observer, not part
of OpenFreedomFighters. This wrapper supplies no game location, PID, target,
debugger argument, or shell. It keeps only sanitized repeat-gated records.
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

import retail_localization_lookup_repeat_pair as repeat_pair
import retail_localization_lookup_trace as trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
FIRST_RAW_NAME, SECOND_RAW_NAME = "first.raw.json", "second.raw.json"
FIRST_SANITIZED_NAME, SECOND_SANITIZED_NAME = "first.sanitized.json", "second.sanitized.json"
REPEAT_PAIR_NAME = "repeat-pair.json"
_RAW_NAMES = (FIRST_RAW_NAME, SECOND_RAW_NAME)
_EXPECTED_RAW_FILES = frozenset(_RAW_NAMES)
MAX_RAW_RECORD_BYTES = trace.MAX_TRACE_BYTES
DEFAULT_TIMEOUT_SECONDS, MAX_TIMEOUT_SECONDS = 300, 1800


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
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
    if getattr(os, "O_NOFOLLOW", None) is None:
        raise ValueError("platform cannot safely read observer records")
    try:
        descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=directory)
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
    if getattr(os, "O_NOFOLLOW", None) is None:
        raise ValueError("platform cannot safely write observer records")
    try:
        descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
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
                raise ValueError("observer workspace must contain exactly two structural records")
            first = trace.sanitize_trace(_read_workspace_json(descriptor, FIRST_RAW_NAME))
            second = trace.sanitize_trace(_read_workspace_json(descriptor, SECOND_RAW_NAME))
            pair = repeat_pair.sanitize_repeat_pair(first, second)
        finally:
            for name in _RAW_NAMES:
                try:
                    os.unlink(name, dir_fd=descriptor)
                except FileNotFoundError:
                    pass
        _write_workspace_json(descriptor, FIRST_SANITIZED_NAME, first)
        _write_workspace_json(descriptor, SECOND_SANITIZED_NAME, second)
        _write_workspace_json(descriptor, REPEAT_PAIR_NAME, pair)
        return pair
    finally:
        os.close(descriptor)


def execute_observation(*, observer: pathlib.Path, workspace: pathlib.Path,
                        timeout_seconds: int = DEFAULT_TIMEOUT_SECONDS,
                        run: Callable[..., subprocess.CompletedProcess[Any]] = subprocess.run) -> dict[str, Any]:
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= MAX_TIMEOUT_SECONDS:
        raise ValueError("observer timeout must be between 1 and 1800 seconds")
    observer_path = _validate_observer_path(observer)
    workspace_path = _outside_repository(workspace, "workspace")
    _make_workspace(workspace_path)
    try:
        for raw_name in _RAW_NAMES:
            command: Sequence[str] = (str(observer_path), "--mode", "fresh-isolated",
                                      "--output", str(workspace_path / raw_name))
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
        pair = execute_observation(observer=args.observer, workspace=args.workspace,
                                   timeout_seconds=args.timeout_seconds)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"collected two fresh source-free retail localization observations ({len(pair['events'])} events)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
