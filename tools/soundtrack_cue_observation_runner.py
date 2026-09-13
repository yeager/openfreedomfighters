#!/usr/bin/env python3
"""Run an explicit fresh-isolated private soundtrack cue observer.

No target discovery, attachment, game path, audio filename, decoder parameter,
or observer output is accepted.  The operator supplies a private executable;
this wrapper retains only sanitized structural evidence and removes raw files.
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

import soundtrack_cue_evidence_bundle as bundle
import soundtrack_cue_observation_trace as trace

REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
RAW_NAMES = ("success-a.raw.json", "success-b.raw.json", "rejected.raw.json")
SANITIZED_NAMES = ("success-a.sanitized.json", "success-b.sanitized.json", "rejected.sanitized.json")
BUNDLE_NAME = "review-bundle.json"
MAX_RAW_BYTES = 256 * 1024
DEFAULT_TIMEOUT_SECONDS = 300
MAX_TIMEOUT_SECONDS = 1800


def _outside(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _workspace(path: pathlib.Path) -> int:
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    except OSError as error:
        raise ValueError("workspace must be a real private directory") from error
    if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise ValueError("workspace must be a real private directory")
    return descriptor


def _read(directory: int, name: str) -> Any:
    try:
        descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=directory)
    except OSError as error:
        raise ValueError("observer record must be a regular private file") from error
    try:
        meta = os.fstat(descriptor)
        if not stat.S_ISREG(meta.st_mode) or meta.st_size > MAX_RAW_BYTES:
            raise ValueError("observer record must be a bounded regular private file")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(descriptor)


def _write(directory: int, name: str, record: dict[str, Any]) -> None:
    try:
        descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600, dir_fd=directory)
    except OSError as error:
        raise ValueError("refusing to overwrite private observation output") from error
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
            json.dump(record, stream, indent=2)
            stream.write("\n")
    finally:
        os.close(descriptor)


def _discard_raw(path: pathlib.Path) -> None:
    try:
        directory = _workspace(path)
    except ValueError:
        return
    try:
        for name in RAW_NAMES:
            try:
                os.unlink(name, dir_fd=directory)
            except FileNotFoundError:
                pass
    finally:
        os.close(directory)


def _collect(workspace: pathlib.Path) -> dict[str, Any]:
    directory = _workspace(workspace)
    try:
        try:
            if frozenset(os.listdir(directory)) != frozenset(RAW_NAMES):
                raise ValueError("observer workspace must contain exactly three structural records")
            records = [trace.sanitize_trace(_read(directory, name)) for name in RAW_NAMES]
            result = bundle.sanitize_bundle(*records)
        finally:
            for name in RAW_NAMES:
                try:
                    os.unlink(name, dir_fd=directory)
                except FileNotFoundError:
                    pass
        for name, record in zip(SANITIZED_NAMES, records, strict=True):
            _write(directory, name, record)
        _write(directory, BUNDLE_NAME, result)
        return result
    finally:
        os.close(directory)


def execute_observation(*, observer: pathlib.Path, workspace: pathlib.Path,
                        timeout_seconds: int = DEFAULT_TIMEOUT_SECONDS,
                        run: Callable[..., subprocess.CompletedProcess[Any]] = subprocess.run) -> dict[str, Any]:
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= MAX_TIMEOUT_SECONDS:
        raise ValueError("observer timeout must be between 1 and 1800 seconds")
    observer_path, workspace_path = _outside(observer, "observer"), _outside(workspace, "workspace")
    if (not observer_path.is_file() or observer_path.is_symlink() or
            not os.access(observer_path, os.X_OK)):
        raise ValueError("observer must be an executable regular file outside the repository")
    if workspace_path.exists():
        raise ValueError("workspace must be new")
    workspace_path.mkdir(mode=0o700, parents=True)
    command: Sequence[str] = (str(observer_path), "--mode", "fresh-isolated",
                              "--success-a-output", str(workspace_path / RAW_NAMES[0]),
                              "--success-b-output", str(workspace_path / RAW_NAMES[1]),
                              "--rejected-output", str(workspace_path / RAW_NAMES[2]))
    try:
        run(command, check=True, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, shell=False, timeout=timeout_seconds)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        _discard_raw(workspace_path)
        raise ValueError("private observer did not complete") from error
    try:
        return _collect(workspace_path)
    except (OSError, ValueError, json.JSONDecodeError):
        _discard_raw(workspace_path)
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
        result = execute_observation(observer=args.observer, workspace=args.workspace,
                                     timeout_seconds=args.timeout_seconds)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"collected {len(result['bindings'])} isolated source-free soundtrack cue bindings")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
