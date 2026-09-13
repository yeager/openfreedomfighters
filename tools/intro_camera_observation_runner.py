#!/usr/bin/env python3
"""Run a supplied fresh-isolated private intro-camera observer.

No target, game path, process selector, debugger command, or observer output is
accepted by this wrapper. It keeps only the bundled categorical receipt.
"""
from __future__ import annotations
import argparse
import json
import os
import pathlib
import stat
import subprocess
import sys
from typing import Any, Callable
import intro_camera_lifecycle_contract_bundle as bundle
import intro_camera_lifecycle_trace as trace

REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
RAW_NAMES = ("candidate.raw.json", "repeat.raw.json", "failure.raw.json")
RECEIPT_NAME = "intro-camera-lifecycle.receipt.json"
MAX_BYTES = 8 * 1024 * 1024
DEFAULT_TIMEOUT_SECONDS = 300
MAX_TIMEOUT_SECONDS = 1800

def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    # Reuse the trace's component-wise lstat walk: rejecting only the final
    # entry would let a private workspace escape through a symlinked parent.
    return trace._outside_repository(path, label)

def _observer(path: pathlib.Path) -> pathlib.Path:
    resolved = _outside_repository(path, "observer")
    if not resolved.is_file() or resolved.is_symlink() or not os.access(resolved, os.X_OK): raise ValueError("observer must be an executable regular file outside the repository")
    return resolved

def _workspace(path: pathlib.Path) -> pathlib.Path:
    resolved = _outside_repository(path, "workspace")
    if resolved.exists(): raise ValueError("private observation workspace must be new")
    resolved.mkdir(mode=0o700, parents=True)
    return resolved

def _read(path: pathlib.Path) -> Any:
    try: fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
    except OSError as error: raise ValueError("observer records must be bounded regular files") from error
    try:
        metadata = os.fstat(fd)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_BYTES: raise ValueError("observer records must be bounded regular files")
        with os.fdopen(fd, "r", encoding="utf-8", closefd=False) as stream: return json.load(stream)
    finally: os.close(fd)

def _write_new(path: pathlib.Path, value: dict[str, Any]) -> None:
    try: fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    except OSError as error: raise ValueError("refusing to overwrite observer receipt") from error
    try:
        with os.fdopen(fd, "w", encoding="utf-8", closefd=False) as stream: json.dump(value, stream, separators=(",", ":")); stream.write("\n")
    finally: os.close(fd)

def _discard_raw(workspace: pathlib.Path) -> None:
    for name in RAW_NAMES:
        try: (workspace / name).unlink()
        except FileNotFoundError: pass

def execute_observation(*, observer: pathlib.Path, workspace: pathlib.Path, timeout_seconds: int = DEFAULT_TIMEOUT_SECONDS, run: Callable[..., subprocess.CompletedProcess[Any]] = subprocess.run) -> dict[str, Any]:
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= MAX_TIMEOUT_SECONDS: raise ValueError("observer timeout must be between 1 and 1800 seconds")
    executable, private = _observer(observer), _workspace(workspace)
    records = tuple(private / name for name in RAW_NAMES)
    command = (str(executable), "--mode", "fresh-isolated", "--candidate-output", str(records[0]), "--repeat-output", str(records[1]), "--failure-output", str(records[2]))
    try:
        run(command, check=True, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, shell=False, timeout=timeout_seconds)
        if frozenset(child.name for child in private.iterdir()) != frozenset(RAW_NAMES): raise ValueError("observer workspace must contain exactly three structural records")
        receipt = bundle.bundle_contract(*(trace.sanitize_trace(_read(record)) for record in records))
    except (OSError, ValueError, json.JSONDecodeError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        _discard_raw(private); raise ValueError("private observer did not produce a valid lifecycle receipt") from error
    _discard_raw(private); _write_new(private / RECEIPT_NAME, receipt)
    return receipt

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("--execute", action="store_true"); parser.add_argument("--observer", type=pathlib.Path); parser.add_argument("--workspace", type=pathlib.Path); parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS); args = parser.parse_args()
    if not args.execute: parser.error("refusing to run an observer without --execute")
    if args.observer is None or args.workspace is None: parser.error("--observer and --workspace are required with --execute")
    try: execute_observation(observer=args.observer, workspace=args.workspace, timeout_seconds=args.timeout_seconds)
    except ValueError as error: print(f"error: {error}", file=sys.stderr); return 1
    print("collected one fresh-isolated source-free intro-camera lifecycle receipt"); return 0

if __name__ == "__main__": raise SystemExit(main())
