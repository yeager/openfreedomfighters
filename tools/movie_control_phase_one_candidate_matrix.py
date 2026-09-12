#!/usr/bin/env python3
"""Validate a private, aggregate MovieControl phase-one candidate matrix.

The input is produced outside the repository from an owned installation: a
private static dispatcher reduction and an isolated runtime observation.  It
contains only aggregate cardinalities and booleans.  In particular, this tool
does not accept callback identities, addresses, offsets, paths, source types,
names, bytes, strings, or executable material.

The result is preparation for collecting the existing source-free phase-one
traces.  It never identifies a callback and does not authorize native startup.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.movie-control-phase-one-candidate-matrix.raw/v1"
OUTPUT_FORMAT = "off.movie-control-phase-one-candidate-matrix/v1"
MAX_CANDIDATES = 65535

_FIELDS = frozenset((
    "format", "target_relation_verified", "static_mapping_complete",
    "runtime_observation_complete", "dispatcher_candidate_count",
    "candidates_observed", "uniquely_observed",
))


def _boolean(value: Any, label: str) -> bool:
    if type(value) is not bool:
        raise ValueError(f"{label} must be boolean")
    return value


def _count(value: Any, label: str) -> int:
    if type(value) is not int or value < 0 or value > MAX_CANDIDATES:
        raise ValueError(f"{label} must be a bounded non-negative integer")
    return value


def sanitize_matrix(raw: Any) -> dict[str, Any]:
    """Return the only permitted aggregate candidate-matrix record."""
    if not isinstance(raw, dict) or frozenset(raw) != _FIELDS:
        raise ValueError("candidate matrix has an unsupported field")
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized candidate-matrix format")
    target_relation = _boolean(raw["target_relation_verified"], "target_relation_verified")
    static_mapping = _boolean(raw["static_mapping_complete"], "static_mapping_complete")
    runtime_observation = _boolean(raw["runtime_observation_complete"], "runtime_observation_complete")
    candidates = _count(raw["dispatcher_candidate_count"], "dispatcher_candidate_count")
    observed = _count(raw["candidates_observed"], "candidates_observed")
    unique = _boolean(raw["uniquely_observed"], "uniquely_observed")
    if not target_relation and (static_mapping or runtime_observation or candidates or observed or unique):
        raise ValueError("unverified target relation cannot claim candidate analysis")
    if target_relation and candidates == 0:
        raise ValueError("verified target relation requires dispatcher candidates")
    if observed > candidates:
        raise ValueError("observed candidates exceed static candidates")
    if not static_mapping and (runtime_observation or observed or unique):
        raise ValueError("runtime observation requires a completed static mapping")
    if not runtime_observation and (observed or unique):
        raise ValueError("unobserved matrix cannot claim runtime candidates")
    if unique != (observed == 1):
        raise ValueError("unique observation must account for exactly one candidate")
    return {
        "format": OUTPUT_FORMAT,
        "target_relation_verified": target_relation,
        "static_mapping_complete": static_mapping,
        "runtime_observation_complete": runtime_observation,
        "dispatcher_candidate_count": candidates,
        "candidates_observed": observed,
        "uniquely_observed": unique,
        "requires_repeat_pair": True,
        "ready_for_trace_collection": target_relation and static_mapping and runtime_observation and unique,
    }


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path,
                        help="private aggregate candidate-matrix JSON")
    parser.add_argument("output", type=pathlib.Path,
                        help="new private sanitized candidate-matrix JSON")
    args = parser.parse_args()
    try:
        input_path = _outside_repository(args.input, "input")
        output_path = _outside_repository(args.output, "output")
        if input_path == output_path:
            raise ValueError("input and output paths must differ")
        if output_path.exists():
            raise ValueError("refusing to overwrite an existing private matrix")
        result = sanitize_matrix(json.loads(input_path.read_text(encoding="utf-8")))
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one source-free MovieControl phase-one candidate matrix")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
