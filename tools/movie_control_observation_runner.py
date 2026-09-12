#!/usr/bin/env python3
"""Run one explicitly requested, isolated private MovieControl observation.

This wrapper is intentionally not an observer or an instrumentation system.  A
separately maintained *private* observer is the only executable it can start.
The wrapper provides it no game path, process identifier, debugger selector,
or target locator.  It creates one empty private workspace and accepts exactly
two structural JSON records there, then validates and sanitizes them with the
existing narrow schemas.

No output from the private observer is relayed to the terminal.  This avoids
accidentally exporting retail text, addresses, logs, or other executable
material through a build log.  The operator must explicitly provide
``--execute``: there is no implicit observation path.
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

import movie_control_cutscene_dispatch_trace as dispatch_trace
import movie_control_observer_probe_plan as probe_plan
import movie_control_phase_one_trace as phase_one_trace


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
PHASE_ONE_RAW_NAME = "phase-one.raw.json"
DISPATCH_RAW_NAME = "cutscene-dispatch.raw.json"
PHASE_ONE_SANITIZED_NAME = "phase-one.sanitized.json"
DISPATCH_SANITIZED_NAME = "cutscene-dispatch.sanitized.json"
_EXPECTED_FILES = frozenset((PHASE_ONE_RAW_NAME, DISPATCH_RAW_NAME))
# Both schemas are small, bounded structural records.  This is deliberately
# generous for their maximum 4096-event form, while preventing an observer
# workspace from being used as an unbounded JSON parsing or disk-read channel.
MAX_RAW_RECORD_BYTES = 8 * 1024 * 1024
DEFAULT_TIMEOUT_SECONDS = 300
MAX_TIMEOUT_SECONDS = 1800


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    # ``Path.resolve`` deliberately follows a final symlink.  Checking for a
    # symlink after resolving would therefore validate the target rather than
    # the operator-supplied entry.  Do this check first so a plan or observer
    # cannot silently cross a private-file boundary through a final symlink.
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _canonical_probe_plan(raw: Any) -> dict[str, Any]:
    """Revalidate an already canonical opaque plan without accepting details."""
    if not isinstance(raw, dict) or frozenset(raw) != frozenset(("format", "probes")):
        raise ValueError("probe plan has an unsupported field")
    if raw["format"] != probe_plan.OUTPUT_FORMAT:
        raise ValueError("probe plan must be canonical")
    # The public plan validator is deliberately raw->canonical.  Convert only
    # its fixed marker back to raw and revalidate the fixed four positions.
    return probe_plan.validate_probe_plan({
        "format": probe_plan.INPUT_FORMAT,
        "probes": raw["probes"],
    })


def _read_regular_json_path_no_follow(path: pathlib.Path, label: str) -> Any:
    """Read one private protocol file from its supplied entry, not its target.

    The canonical plan is data supplied by an operator and is intentionally
    tiny and opaque.  It still must not be read through a final symlink or
    swapped between validation and decoding.  ``O_NOFOLLOW`` binds the read to
    that final entry; ``fstat`` rejects devices, directories, and FIFOs before
    JSON parsing.
    """
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError(f"platform cannot safely read {label}")
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NONBLOCK | no_follow)
    except OSError as error:
        raise ValueError(f"{label} must be a regular private file") from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise ValueError(f"{label} must be a regular private file")
        if metadata.st_size > MAX_RAW_RECORD_BYTES:
            raise ValueError(f"{label} exceeds the structural size limit")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(descriptor)


def _read_regular_json_no_follow(directory: int, name: str) -> Any:
    """Read one observer record without following a replacement symlink.

    The observer runs out-of-process.  Checking ``Path.is_symlink()`` before a
    normal ``read_text`` leaves a check/use gap in which a record could be
    replaced.  A descriptor opened relative to the already-opened workspace,
    with ``O_NOFOLLOW``, is bound to the exact directory entry that is
    validated and decoded below.  This also avoids following a replacement of
    the workspace pathname after collection has begun.
    """
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError("platform cannot safely read observer records")
    try:
        descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | no_follow,
                             dir_fd=directory)
    except OSError as error:
        raise ValueError("observer records must be regular files") from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise ValueError("observer records must be regular files")
        if metadata.st_size > MAX_RAW_RECORD_BYTES:
            raise ValueError("observer records exceed the structural size limit")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(descriptor)


def _make_workspace(workspace: pathlib.Path) -> None:
    if workspace.exists():
        raise ValueError("private observation workspace must be new")
    workspace.mkdir(mode=0o700, parents=True)
    if not workspace.is_dir() or workspace.is_symlink():
        raise ValueError("private observation workspace must be a real directory")


def _validate_observer_path(observer: pathlib.Path) -> pathlib.Path:
    resolved = _outside_repository(observer, "observer")
    if not resolved.is_file() or resolved.is_symlink() or not os.access(resolved, os.X_OK):
        raise ValueError("observer must be an executable regular file outside the repository")
    return resolved


def _open_workspace_no_follow(workspace: pathlib.Path) -> int:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    directory = getattr(os, "O_DIRECTORY", None)
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


def _write_sanitized_json(directory: int, name: str, record: dict[str, Any]) -> None:
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
    """Best-effort removal of protocol raw records after an observer failure.

    The workspace is created by this wrapper.  It is still reopened through a
    no-follow directory descriptor here: an external observer must not be able
    to redirect cleanup by replacing the workspace pathname.  Unknown entries
    are left alone for the private operator to diagnose, but the two only
    names that may contain observer output are never retained by this tool.
    """
    try:
        descriptor = _open_workspace_no_follow(workspace)
    except ValueError:
        return
    try:
        for raw_name in (PHASE_ONE_RAW_NAME, DISPATCH_RAW_NAME):
            try:
                os.unlink(raw_name, dir_fd=descriptor)
            except FileNotFoundError:
                pass
    finally:
        os.close(descriptor)


def _phase_one_success_candidates(phase_trace: dict[str, Any]) -> list[dict[str, Any]]:
    """Return every completed constructed phase-one candidate.

    Do not reduce this to a set of callback ordinals.  An observer that emits
    the same apparently successful candidate twice has recorded an ambiguous
    lifecycle boundary even when both records use the same run-local ordinal.
    """
    return [
        event
        for event in phase_trace["events"]
        if event["component_is_constructed"]
        and event["owner_is_constructed_owner"]
        and event["global_lifecycle_entered"]
        and event["global_lifecycle_completed"]
        and event["global_lifecycle_outcome"] == "success"
        and event["phase_one_completed"]
        and event["outcome"] == "success"
    ]


def _validate_trace_relation(
    phase_trace: dict[str, Any], dispatch_trace_record: dict[str, Any],
) -> None:
    """Require the collected dispatch route to be tied to phase-one evidence.

    The two schemas deliberately share just an observer-local callback
    ordinal.  It is enough to bind the source-free records while avoiding any
    process/object identity, address, or executable detail.  A collection
    without an admitted constructed MovieControl route is not actionable
    observation evidence and must not leave a misleading pair of files.
    """
    successful_candidates = _phase_one_success_candidates(phase_trace)
    admitted_callbacks = {
        event["callback_ordinal"]
        for event in dispatch_trace_record["events"]
        if event["event16_gate"] == "admitted"
        and event["movie_component_is_constructed"]
        and event["movie_owner_is_constructed_owner"]
        and event["movie_phase_one_completed"]
    }
    if len(successful_candidates) != 1:
        raise ValueError("phase-one record must contain exactly one completed constructed callback")
    successful_callbacks = {successful_candidates[0]["callback_ordinal"]}
    if admitted_callbacks != successful_callbacks:
        raise ValueError("dispatch record is not tied to the completed phase-one callback")


def _collect(workspace: pathlib.Path) -> tuple[dict[str, Any], dict[str, Any]]:
    workspace_descriptor = _open_workspace_no_follow(workspace)
    try:
        entries = frozenset(os.listdir(workspace_descriptor))
        try:
            if entries != _EXPECTED_FILES:
                raise ValueError("observer workspace must contain exactly the two structural records")
            # These strict schemas admit no free-form string, location, raw byte, or
            # identity fields.  We only persist the sanitized structural forms.
            phase_clean = phase_one_trace.sanitize_trace(
                _read_regular_json_no_follow(workspace_descriptor, PHASE_ONE_RAW_NAME))
            dispatch_clean = dispatch_trace.sanitize_trace(
                _read_regular_json_no_follow(workspace_descriptor, DISPATCH_RAW_NAME))
            _validate_trace_relation(phase_clean, dispatch_clean)
        finally:
            # A malformed or rejected raw record must not become a retained export
            # channel either. Leave only a private error state for the operator.
            for raw_name in (PHASE_ONE_RAW_NAME, DISPATCH_RAW_NAME):
                try:
                    os.unlink(raw_name, dir_fd=workspace_descriptor)
                except FileNotFoundError:
                    pass
        _write_sanitized_json(workspace_descriptor, PHASE_ONE_SANITIZED_NAME, phase_clean)
        _write_sanitized_json(workspace_descriptor, DISPATCH_SANITIZED_NAME, dispatch_clean)
        return phase_clean, dispatch_clean
    finally:
        os.close(workspace_descriptor)


def execute_observation(
    *, observer: pathlib.Path, canonical_plan: pathlib.Path, workspace: pathlib.Path,
    timeout_seconds: int = DEFAULT_TIMEOUT_SECONDS,
    run: Callable[..., subprocess.CompletedProcess[Any]] = subprocess.run,
) -> tuple[dict[str, Any], dict[str, Any]]:
    """Start an external fresh-process observer and retain only safe records.

    The opaque observer protocol uses no PID or attach mode.  ``fresh-isolated``
    is a mandatory, literal mode supplied to the private observer; enforcement
    inside that observer remains an operator responsibility and is not claimed
    as public evidence by this wrapper.
    """
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= MAX_TIMEOUT_SECONDS:
        raise ValueError("observer timeout must be between 1 and 1800 seconds")
    observer_path = _validate_observer_path(observer)
    plan_path = _outside_repository(canonical_plan, "canonical plan")
    workspace_path = _outside_repository(workspace, "workspace")
    if not plan_path.is_file():
        raise ValueError("canonical plan must be a regular private file")
    _canonical_probe_plan(_read_regular_json_path_no_follow(plan_path, "canonical plan"))
    _make_workspace(workspace_path)
    command: Sequence[str] = (
        str(observer_path), "--mode", "fresh-isolated", "--probe-plan", str(plan_path),
        "--phase-one-output", str(workspace_path / PHASE_ONE_RAW_NAME),
        "--dispatch-output", str(workspace_path / DISPATCH_RAW_NAME),
    )
    try:
        run(command, check=True, stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, shell=False,
            timeout=timeout_seconds)
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
    parser.add_argument("--execute", action="store_true",
                        help="required acknowledgement before an observer may start")
    parser.add_argument("--observer", type=pathlib.Path,
                        help="private fresh-process observer executable")
    parser.add_argument("--canonical-plan", type=pathlib.Path,
                        help="private canonical opaque probe plan")
    parser.add_argument("--workspace", type=pathlib.Path,
                        help="new private directory for this one observation")
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS,
                        help="private observer deadline (1-1800; default: 300)")
    args = parser.parse_args()
    if not args.execute:
        parser.error("refusing to run an observer without --execute")
    if args.observer is None or args.canonical_plan is None or args.workspace is None:
        parser.error("--observer, --canonical-plan, and --workspace are required with --execute")
    try:
        phase_clean, dispatch_clean = execute_observation(
            observer=args.observer, canonical_plan=args.canonical_plan,
            workspace=args.workspace, timeout_seconds=args.timeout_seconds)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    # Counts are the only terminal output; never expose private paths or raw
    # observation contents from a developer shell or CI log.
    print("collected one isolated source-free MovieControl observation "
          f"({len(phase_clean['events'])} phase-one, {len(dispatch_clean['events'])} dispatch records)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
