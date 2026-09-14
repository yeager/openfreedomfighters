#!/usr/bin/env python3
"""Validate an opaque, source-free MovieControl observation probe plan.

This is a schema gate for a separately maintained private observer.  It does
not locate a process, inspect a game installation, attach a debugger, or
instrument an executable.  The plan intentionally carries no target details:
it can only select the fixed, opaque protocol points below.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

import private_structural_json

REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.movie-control-observer-probe-plan.raw/v1"
OUTPUT_FORMAT = "off.movie-control-observer-probe-plan/v1"
MAX_PLAN_BYTES = 64 * 1024

# These labels are observer-protocol positions, not retail identities, target
# selectors, function names, or instructions to a debugger.
PROTOCOL_POINTS = (
    "global_phase_one_enter",
    "candidate_enter",
    "candidate_leave",
    "global_phase_one_leave",
    # The collection runner also requires a source-free event-16-to-player
    # route record. These positions are not executable targets or instructions
    # for how an external observer reaches them.
    "event16_gate",
    "handoff_boundary",
    "player_activation",
    "route_terminal",
)


def _require_exact_keys(record: dict[str, Any], expected: frozenset[str]) -> None:
    if frozenset(record) != expected:
        raise ValueError("probe plan has an unsupported field")


def validate_probe_plan(raw: Any) -> dict[str, Any]:
    """Return the sole permitted source-free probe-plan representation.

    The validator deliberately accepts no free-form values.  In particular,
    it cannot encode a locator, address, offset, symbol, path, executable
    name, raw material, or a target-discovery rule.
    """
    if not isinstance(raw, dict):
        raise ValueError("probe plan must be a JSON object")
    _require_exact_keys(raw, frozenset(("format", "probes")))
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized probe-plan format")
    probes = raw["probes"]
    if not isinstance(probes, list) or len(probes) != len(PROTOCOL_POINTS):
        raise ValueError("probe plan must contain every fixed protocol probe")
    seen_slots: set[int] = set()
    canonical: list[dict[str, Any]] = []
    for probe in probes:
        if not isinstance(probe, dict):
            raise ValueError("probe must be a JSON object")
        _require_exact_keys(probe, frozenset(("slot", "point")))
        slot = probe["slot"]
        if type(slot) is not int or slot < 0 or slot >= len(PROTOCOL_POINTS):
            raise ValueError("probe slot is outside the fixed ordinal range")
        if slot in seen_slots or probe["point"] != PROTOCOL_POINTS[slot]:
            raise ValueError("probe slot/point relation is not the fixed protocol")
        seen_slots.add(slot)
        canonical.append({"slot": slot, "point": PROTOCOL_POINTS[slot]})
    if len(seen_slots) != len(PROTOCOL_POINTS):
        raise ValueError("probe slots must be dense and unique")
    canonical.sort(key=lambda probe: probe["slot"])
    return {"format": OUTPUT_FORMAT, "probes": canonical}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _read_private_json_no_follow(path: pathlib.Path, label: str) -> Any:
    """Read the operator's final file entry without following a symlink."""
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
        if metadata.st_size > MAX_PLAN_BYTES:
            raise ValueError(f"{label} exceeds the probe-plan size limit")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream, object_pairs_hook=private_structural_json.strict_json_object)
    finally:
        os.close(descriptor)


def _write_new_private_json_no_follow(path: pathlib.Path, record: dict[str, Any]) -> None:
    """Create one private canonical plan without a replaceable output entry."""
    no_follow = getattr(os, "O_NOFOLLOW", None)
    if no_follow is None:
        raise ValueError("platform cannot safely write probe plan")
    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | no_follow,
                             0o600)
    except OSError as error:
        raise ValueError("refusing to overwrite an existing private probe plan") from error
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
            json.dump(record, stream, indent=2)
            stream.write("\n")
    finally:
        os.close(descriptor)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path,
                        help="private opaque probe-plan JSON")
    parser.add_argument("output", type=pathlib.Path,
                        help="new private canonical probe-plan JSON")
    args = parser.parse_args()
    try:
        input_path = _outside_repository(args.input, "input")
        output_path = _outside_repository(args.output, "output")
        if input_path == output_path:
            raise ValueError("input and output paths must differ")
        plan = validate_probe_plan(_read_private_json_no_follow(input_path, "input"))
        output_path.parent.mkdir(parents=True, exist_ok=True)
        _write_new_private_json_no_follow(output_path, plan)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one source-free opaque MovieControl observer probe plan")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
