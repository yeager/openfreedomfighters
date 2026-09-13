#!/usr/bin/env python3
"""Sanitize source-free visual state-change evidence from a private intro run.

An external observer may compare an isolated original-process capture set, but
this utility never sees that set.  It reads only a small categorical JSON
record and writes a new private receipt.  It neither launches, attaches to, nor
controls a process; it never accepts images, pixel hashes, text, timings,
paths, executable details, or resource identifiers.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.intro-visual-timeline.raw/v1"
OUTPUT_FORMAT = "off.intro-visual-timeline/v1"
MAX_SAMPLES = 128
MAX_EXTENT = 32768

_SESSION_SCOPES = frozenset(("fresh_isolated",))
_RELATIONS = frozenset(("initial", "unchanged", "changed"))


def _exact(value: dict[str, Any], fields: frozenset[str], label: str) -> None:
    if frozenset(value) != fields:
        raise ValueError(f"{label} has unsupported fields")


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or value < 0 or value > maximum:
        raise ValueError(f"{label} must be a bounded unsigned integer")
    return value


def _extent(value: Any) -> dict[str, int]:
    if not isinstance(value, dict):
        raise ValueError("capture_extent must be an object")
    _exact(value, frozenset(("width", "height")), "capture_extent")
    width = _natural(value["width"], "capture_extent.width", MAX_EXTENT)
    height = _natural(value["height"], "capture_extent.height", MAX_EXTENT)
    if width == 0 or height == 0:
        raise ValueError("capture_extent cannot be empty")
    return {"width": width, "height": height}


def sanitize_audit(raw: Any) -> dict[str, Any]:
    """Return only bounded, source-free state-change evidence."""
    if not isinstance(raw, dict):
        raise ValueError("audit must be an object")
    _exact(raw, frozenset(("format", "session_scope", "capture_extent", "samples")), "audit")
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized audit format")
    if raw["session_scope"] not in _SESSION_SCOPES:
        raise ValueError("audit must describe a fresh isolated process")
    extent = _extent(raw["capture_extent"])
    samples = raw["samples"]
    if not isinstance(samples, list) or len(samples) < 2 or len(samples) > MAX_SAMPLES:
        raise ValueError("audit must contain between two and 128 samples")

    changed_orders: list[int] = []
    prior_order = -1
    for index, sample in enumerate(samples):
        if not isinstance(sample, dict):
            raise ValueError("sample must be an object")
        _exact(sample, frozenset(("capture_order", "relation")), "sample")
        order = _natural(sample["capture_order"], "capture_order", MAX_SAMPLES)
        if order <= prior_order:
            raise ValueError("capture_order must strictly increase")
        prior_order = order
        relation = sample["relation"]
        if relation not in _RELATIONS:
            raise ValueError("sample has unsupported relation")
        if index == 0:
            if relation != "initial":
                raise ValueError("first sample must be initial")
        elif relation == "initial":
            raise ValueError("only the first sample may be initial")
        elif relation == "changed":
            changed_orders.append(order)

    if not changed_orders:
        raise ValueError("audit does not establish a visual state change")
    return {
        "format": OUTPUT_FORMAT,
        "session_scope": "fresh_isolated",
        "capture_extent": extent,
        "sample_count": len(samples),
        "changed_capture_orders": changed_orders,
    }


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _write_new_private_json(path: pathlib.Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        handle = path.open("x", encoding="utf-8")
    except FileExistsError as error:
        raise ValueError("refusing to overwrite an existing private receipt") from error
    with handle:
        handle.write(json.dumps(value, indent=2) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path, help="private observer JSON")
    parser.add_argument("output", type=pathlib.Path, help="new private sanitized JSON")
    args = parser.parse_args()
    try:
        input_path = _outside_repository(args.input, "input")
        output_path = _outside_repository(args.output, "output")
        if input_path == output_path:
            raise ValueError("input and output paths must differ")
        raw = json.loads(input_path.read_text(encoding="utf-8"))
        sanitized = sanitize_audit(raw)
        _write_new_private_json(output_path, sanitized)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {sanitized['sample_count']} source-free intro timeline samples")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
