#!/usr/bin/env python3
"""Validate the fixed opaque protocol for a private coordinator observer.

This contains protocol positions only.  It never accepts a process selector,
game path, executable detail, locator, debugger expression, or raw material.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.startup-coordinator-observer-probe-plan.raw/v1"
OUTPUT_FORMAT = "off.startup-coordinator-observer-probe-plan/v1"
# Protocol labels, not target names or instrumentation instructions.
PROTOCOL_POINTS = (
    "coordinator_enter",
    "root_selection",
    "camera_view",
    "pass_delivery",
    "coordinator_leave",
)


def _exact(value: dict[str, Any], keys: frozenset[str]) -> None:
    if frozenset(value) != keys:
        raise ValueError("probe plan has an unsupported field")


def validate_probe_plan(raw: Any) -> dict[str, Any]:
    """Return the one permissible source-free plan, sorted by fixed slot."""
    if not isinstance(raw, dict):
        raise ValueError("probe plan must be a JSON object")
    _exact(raw, frozenset(("format", "probes")))
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized probe-plan format")
    probes = raw["probes"]
    if not isinstance(probes, list) or len(probes) != len(PROTOCOL_POINTS):
        raise ValueError("probe plan must contain exactly five probes")
    seen: set[int] = set()
    canonical: list[dict[str, Any]] = []
    for probe in probes:
        if not isinstance(probe, dict):
            raise ValueError("probe must be a JSON object")
        _exact(probe, frozenset(("slot", "point")))
        slot = probe["slot"]
        if type(slot) is not int or not 0 <= slot < len(PROTOCOL_POINTS):
            raise ValueError("probe slot is outside the fixed ordinal range")
        if slot in seen or probe["point"] != PROTOCOL_POINTS[slot]:
            raise ValueError("probe slot/point relation is not the fixed protocol")
        seen.add(slot)
        canonical.append({"slot": slot, "point": PROTOCOL_POINTS[slot]})
    if len(seen) != len(PROTOCOL_POINTS):
        raise ValueError("probe slots must be dense and unique")
    canonical.sort(key=lambda item: item["slot"])
    return {"format": OUTPUT_FORMAT, "probes": canonical}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        source, output = (_outside_repository(args.input, "input"),
                          _outside_repository(args.output, "output"))
        if source == output:
            raise ValueError("input and output paths must differ")
        if output.exists():
            raise ValueError("refusing to overwrite an existing private probe plan")
        output.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        output.write_text(json.dumps(validate_probe_plan(json.loads(source.read_text(encoding="utf-8"))),
                          indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one source-free opaque startup coordinator observer probe plan")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
