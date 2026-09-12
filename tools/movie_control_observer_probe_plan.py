#!/usr/bin/env python3
"""Validate an opaque, source-free MovieControl observation probe plan.

This is a schema gate for a separately maintained private observer.  It does
not locate a process, inspect a game installation, attach a debugger, or
instrument an executable.  The plan intentionally carries no target details:
it can only select the four fixed, opaque protocol points below.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.movie-control-observer-probe-plan.raw/v1"
OUTPUT_FORMAT = "off.movie-control-observer-probe-plan/v1"

# These labels are observer-protocol positions, not retail identities, target
# selectors, function names, or instructions to a debugger.
PROTOCOL_POINTS = (
    "global_phase_one_enter",
    "candidate_enter",
    "candidate_leave",
    "global_phase_one_leave",
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
        raise ValueError("probe plan must contain exactly four probes")
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
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


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
        if output_path.exists():
            raise ValueError("refusing to overwrite an existing private probe plan")
        plan = validate_probe_plan(json.loads(input_path.read_text(encoding="utf-8")))
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(plan, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one source-free opaque MovieControl observer probe plan")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
