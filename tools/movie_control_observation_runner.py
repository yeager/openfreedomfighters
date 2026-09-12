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


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
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


def _read_json(path: pathlib.Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def _read_regular_json_no_follow(path: pathlib.Path) -> Any:
    """Read one observer record without following a replacement symlink.

    The observer runs out-of-process.  Checking ``Path.is_symlink()`` before a
    normal ``read_text`` leaves a check/use gap in which a record could be
    replaced.  A descriptor opened with ``O_NOFOLLOW`` is bound to the exact
    directory entry that is validated and decoded below.
    """
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError("platform cannot safely read observer records")
    try:
        descriptor = os.open(path, os.O_RDONLY | no_follow)
    except OSError as error:
        raise ValueError("observer records must be regular files") from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise ValueError("observer records must be regular files")
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


def _collect(workspace: pathlib.Path) -> tuple[dict[str, Any], dict[str, Any]]:
    entries = frozenset(entry.name for entry in workspace.iterdir())
    phase_raw = workspace / PHASE_ONE_RAW_NAME
    dispatch_raw = workspace / DISPATCH_RAW_NAME
    try:
        if entries != _EXPECTED_FILES:
            raise ValueError("observer workspace must contain exactly the two structural records")
        # These strict schemas admit no free-form string, location, raw byte, or
        # identity fields.  We only persist the sanitized structural forms.
        phase_clean = phase_one_trace.sanitize_trace(_read_regular_json_no_follow(phase_raw))
        dispatch_clean = dispatch_trace.sanitize_trace(_read_regular_json_no_follow(dispatch_raw))
    finally:
        # A malformed or rejected raw record must not become a retained export
        # channel either. Leave only a private error state for the operator.
        for raw_path in (phase_raw, dispatch_raw):
            raw_path.unlink(missing_ok=True)
    (workspace / PHASE_ONE_SANITIZED_NAME).write_text(
        json.dumps(phase_clean, indent=2) + "\n", encoding="utf-8")
    (workspace / DISPATCH_SANITIZED_NAME).write_text(
        json.dumps(dispatch_clean, indent=2) + "\n", encoding="utf-8")
    return phase_clean, dispatch_clean


def execute_observation(
    *, observer: pathlib.Path, canonical_plan: pathlib.Path, workspace: pathlib.Path,
    run: Callable[..., subprocess.CompletedProcess[Any]] = subprocess.run,
) -> tuple[dict[str, Any], dict[str, Any]]:
    """Start an external fresh-process observer and retain only safe records.

    The opaque observer protocol uses no PID or attach mode.  ``fresh-isolated``
    is a mandatory, literal mode supplied to the private observer; enforcement
    inside that observer remains an operator responsibility and is not claimed
    as public evidence by this wrapper.
    """
    observer_path = _validate_observer_path(observer)
    plan_path = _outside_repository(canonical_plan, "canonical plan")
    workspace_path = _outside_repository(workspace, "workspace")
    if not plan_path.is_file() or plan_path.is_symlink():
        raise ValueError("canonical plan must be a regular private file")
    _canonical_probe_plan(_read_json(plan_path))
    _make_workspace(workspace_path)
    command: Sequence[str] = (
        str(observer_path), "--mode", "fresh-isolated", "--probe-plan", str(plan_path),
        "--phase-one-output", str(workspace_path / PHASE_ONE_RAW_NAME),
        "--dispatch-output", str(workspace_path / DISPATCH_RAW_NAME),
    )
    try:
        run(command, check=True, stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, shell=False)
    except subprocess.CalledProcessError as error:
        raise ValueError("private observer did not complete") from error
    return _collect(workspace_path)


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
    args = parser.parse_args()
    if not args.execute:
        parser.error("refusing to run an observer without --execute")
    if args.observer is None or args.canonical_plan is None or args.workspace is None:
        parser.error("--observer, --canonical-plan, and --workspace are required with --execute")
    try:
        phase_clean, dispatch_clean = execute_observation(
            observer=args.observer, canonical_plan=args.canonical_plan, workspace=args.workspace)
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
