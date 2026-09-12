#!/usr/bin/env python3
"""Run a long owned-install audit without exporting its data or diagnostics.

This is an operator-side helper, not a CI command.  It starts only the
OpenFreedomFighters ``--verify-only`` mode, redirects its stdout and stderr to
``/dev/null``, and retains a tiny status record in a newly-created private
workspace.  The record deliberately contains no source path, executable path,
hash, archive name, game text, or diagnostic string.  ``--status`` can be run
later (including through a new SSH connection) without needing the original
terminal output.

The game-data directory is never a write target.  On Linux the child receives
an application-owned XDG cache directory below the private workspace, so a
successful deep-audit certificate cannot be written beside the data either.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import subprocess
import sys
import time
from typing import Any, Mapping, Sequence


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
STATUS_NAME = "status.json"
FORMAT = "off-private-install-verification/v1"
DEFAULT_TIMEOUT_SECONDS = 7200
MAX_TIMEOUT_SECONDS = 21600
_STATES = frozenset((
    "running", "verified", "verification-failed", "runtime-failed",
    "timed-out", "start-failed",
))


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    # ``resolve`` follows a final symlink. Reject it before resolving so the
    # supplied workspace/data entry cannot silently cross a filesystem
    # boundary that the subsequent no-follow opens no longer see.
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _private_directory(path: pathlib.Path) -> bool:
    try:
        info = path.stat()
    except OSError:
        return False
    return (stat.S_ISDIR(info.st_mode) and not path.is_symlink() and
            info.st_uid == os.getuid() and (stat.S_IMODE(info.st_mode) & 0o077) == 0)


def _open_workspace(workspace: pathlib.Path) -> int:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    directory = getattr(os, "O_DIRECTORY", None)
    if no_follow is None or directory is None:
        raise ValueError("platform cannot safely open private workspaces")
    try:
        descriptor = os.open(workspace, os.O_RDONLY | directory | no_follow)
    except OSError as error:
        raise ValueError("workspace must be a private real directory") from error
    info = os.fstat(descriptor)
    if (not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid() or
            (stat.S_IMODE(info.st_mode) & 0o077) != 0):
        os.close(descriptor)
        raise ValueError("workspace must be owned and inaccessible to other users")
    return descriptor


def _make_workspace(workspace: pathlib.Path) -> pathlib.Path:
    workspace = _outside_repository(workspace, "workspace")
    if workspace.exists() or workspace.is_symlink():
        raise ValueError("workspace must be a new private directory")
    workspace.mkdir(mode=0o700, parents=True)
    if not _private_directory(workspace):
        raise ValueError("could not create a private workspace")
    return workspace


def _validate_binary(binary: pathlib.Path) -> pathlib.Path:
    # A project build is allowed here; this validation concerns execution, not
    # game data.  It must nevertheless bind to a real regular executable.
    if binary.is_symlink():
        raise ValueError("binary must not be a symlink")
    resolved = binary.resolve()
    try:
        info = resolved.stat()
    except OSError as error:
        raise ValueError("binary must be a readable executable regular file") from error
    if not stat.S_ISREG(info.st_mode) or not os.access(resolved, os.X_OK):
        raise ValueError("binary must be a readable executable regular file")
    return resolved


def _validate_data_root(data_root: pathlib.Path) -> pathlib.Path:
    resolved = _outside_repository(data_root, "game-data directory")
    if not resolved.is_dir():
        raise ValueError("game-data directory must be an existing directory")
    return resolved


def _status_record(state: str, elapsed_seconds: int | None = None) -> dict[str, Any]:
    if state not in _STATES:
        raise ValueError("unsupported private verification state")
    record: dict[str, Any] = {"format": FORMAT, "state": state}
    if elapsed_seconds is not None:
        record["elapsed_seconds"] = elapsed_seconds
    return record


def _validate_status(value: Any) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) not in ({"format", "state"},
                                                           {"format", "state", "elapsed_seconds"}):
        raise ValueError("private verification status has an unsupported field")
    if value.get("format") != FORMAT or value.get("state") not in _STATES:
        raise ValueError("private verification status is invalid")
    if "elapsed_seconds" in value and (type(value["elapsed_seconds"]) is not int or
                                        not 0 <= value["elapsed_seconds"] <= MAX_TIMEOUT_SECONDS):
        raise ValueError("private verification status has an invalid duration")
    return value


def _write_status(workspace_fd: int, record: Mapping[str, Any]) -> None:
    """Atomically replace the fixed status entry without following it."""
    payload = (json.dumps(_validate_status(dict(record)), sort_keys=True) + "\n").encode("utf-8")
    temporary = f".{STATUS_NAME}.{os.getpid()}.part"
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(temporary, flags, 0o600, dir_fd=workspace_fd)
        try:
            os.write(descriptor, payload)
            os.fsync(descriptor)
        finally:
            os.close(descriptor)
        os.replace(temporary, STATUS_NAME, src_dir_fd=workspace_fd, dst_dir_fd=workspace_fd)
    except Exception:
        try:
            os.unlink(temporary, dir_fd=workspace_fd)
        except OSError:
            pass
        raise


def _read_status(workspace: pathlib.Path) -> dict[str, Any]:
    descriptor = _open_workspace(_outside_repository(workspace, "workspace"))
    try:
        flags = os.O_RDONLY | os.O_NONBLOCK | getattr(os, "O_NOFOLLOW", 0)
        try:
            status_fd = os.open(STATUS_NAME, flags, dir_fd=descriptor)
        except OSError as error:
            raise ValueError("private verification status is unavailable") from error
        try:
            info = os.fstat(status_fd)
            if not stat.S_ISREG(info.st_mode) or info.st_size > 512:
                raise ValueError("private verification status is invalid")
            with os.fdopen(status_fd, "r", encoding="utf-8", closefd=False) as stream:
                return _validate_status(json.load(stream))
        finally:
            os.close(status_fd)
    finally:
        os.close(descriptor)


def _child_environment(workspace: pathlib.Path) -> dict[str, str]:
    environment = dict(os.environ)
    cache = workspace / "cache"
    cache.mkdir(mode=0o700)
    if not _private_directory(cache):
        raise ValueError("could not create the private audit cache")
    environment["XDG_CACHE_HOME"] = str(cache)
    return environment


def _run(binary: pathlib.Path, data_root: pathlib.Path, workspace: pathlib.Path,
         timeout_seconds: int) -> dict[str, Any]:
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= MAX_TIMEOUT_SECONDS:
        raise ValueError(f"timeout must be between 1 and {MAX_TIMEOUT_SECONDS} seconds")
    workspace_fd = _open_workspace(workspace)
    started = time.monotonic()
    try:
        _write_status(workspace_fd, _status_record("running"))
        try:
            result = subprocess.run(
                (str(binary), "--data", str(data_root), "--verify-only"),
                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                check=False, shell=False, timeout=timeout_seconds,
                env=_child_environment(workspace), cwd=workspace)
            state = ("verified" if result.returncode == 0 else
                     "verification-failed" if result.returncode == 3 else
                     "runtime-failed")
        except subprocess.TimeoutExpired:
            state = "timed-out"
        record = _status_record(state, min(int(time.monotonic() - started), MAX_TIMEOUT_SECONDS))
        _write_status(workspace_fd, record)
        return record
    finally:
        os.close(workspace_fd)


def launch(*, binary: pathlib.Path, data_root: pathlib.Path, workspace: pathlib.Path,
           timeout_seconds: int, popen: Any = subprocess.Popen) -> None:
    binary = _validate_binary(binary)
    data_root = _validate_data_root(data_root)
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= MAX_TIMEOUT_SECONDS:
        raise ValueError(f"timeout must be between 1 and {MAX_TIMEOUT_SECONDS} seconds")
    workspace = _make_workspace(workspace)
    descriptor = _open_workspace(workspace)
    try:
        _write_status(descriptor, _status_record("running"))
    finally:
        os.close(descriptor)
    command: Sequence[str] = (
        sys.executable, str(pathlib.Path(__file__).resolve()), "--execute",
        "--binary", str(binary), "--data", str(data_root), "--workspace", str(workspace),
        "--timeout-seconds", str(timeout_seconds),
    )
    try:
        popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
              stderr=subprocess.DEVNULL, shell=False, start_new_session=True)
    except OSError:
        descriptor = _open_workspace(workspace)
        try:
            _write_status(descriptor, _status_record("start-failed"))
        finally:
            os.close(descriptor)
        raise ValueError("could not start private verification")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true", help="start a detached private audit")
    mode.add_argument("--execute", action="store_true", help=argparse.SUPPRESS)
    mode.add_argument("--status", action="store_true", help="read only the sanitized terminal state")
    parser.add_argument("--binary", type=pathlib.Path)
    parser.add_argument("--data", type=pathlib.Path)
    parser.add_argument("--workspace", type=pathlib.Path, required=True)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    args = parser.parse_args()
    try:
        if args.status:
            if args.binary is not None or args.data is not None:
                parser.error("--status accepts only --workspace")
            record = _read_status(args.workspace)
        else:
            if args.binary is None or args.data is None:
                parser.error("--binary and --data are required")
            if args.launch:
                launch(binary=args.binary, data_root=args.data, workspace=args.workspace,
                       timeout_seconds=args.timeout_seconds)
                print("private installation verification started")
                return 0
            record = _run(_validate_binary(args.binary), _validate_data_root(args.data),
                          _outside_repository(args.workspace, "workspace"), args.timeout_seconds)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    # This is intentionally the only record ever sent to a terminal.
    print(f"private installation verification: {record['state']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
