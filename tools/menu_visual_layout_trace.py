#!/usr/bin/env python3
"""Sanitize private, source-free measurements of a retail menu layout.

The input is produced by an external observer from a legally owned running
copy. It contains geometry and categorical UI roles only: no labels, pixels,
resource names, screenshots, paths, hashes, or executable addresses. The
result is a review-only private trace that can later justify an
OpenFreedomFighters menu-layout contract. It is deliberately not a renderer,
asset extractor, or a substitute for a recovered picture-resource join.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = "off.menu-visual-layout.raw/v1"
OUTPUT_FORMAT = "off.menu-visual-layout/v1"
MAX_SAMPLES = 64
MAX_ELEMENTS = 128
MAX_EXTENT = 32768
MAX_COORDINATE = 65536

_PHASES = frozenset(("entry", "focused", "submenu", "confirmation"))
_ROLES = frozenset((
    "backdrop", "panel", "heading", "row_label", "row_value",
    "selected_focus", "selector_previous", "selector_next", "action",
    "panel_border", "panel_strip",
))


def _exact(value: dict[str, Any], fields: frozenset[str], label: str) -> None:
    if frozenset(value) != fields:
        raise ValueError(f"{label} has unsupported fields")


def _natural(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or value < 0 or value > maximum:
        raise ValueError(f"{label} must be a bounded unsigned integer")
    return value


def _rect(value: Any, label: str) -> dict[str, int]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    _exact(value, frozenset(("x", "y", "width", "height")), label)
    x = _natural(value["x"], f"{label}.x", MAX_COORDINATE)
    y = _natural(value["y"], f"{label}.y", MAX_COORDINATE)
    width = _natural(value["width"], f"{label}.width", MAX_COORDINATE)
    height = _natural(value["height"], f"{label}.height", MAX_COORDINATE)
    if width == 0 or height == 0 or x + width > MAX_COORDINATE or y + height > MAX_COORDINATE:
        raise ValueError(f"{label} is outside bounded coordinate space")
    return {"x": x, "y": y, "width": width, "height": height}


def _inside(rect: dict[str, int], outer: dict[str, int]) -> bool:
    return (rect["x"] >= outer["x"] and rect["y"] >= outer["y"] and
            rect["x"] + rect["width"] <= outer["x"] + outer["width"] and
            rect["y"] + rect["height"] <= outer["y"] + outer["height"])


def sanitize_trace(raw: Any) -> dict[str, Any]:
    """Return the only permitted, image-free menu-layout trace."""
    if not isinstance(raw, dict):
        raise ValueError("trace must be an object")
    _exact(raw, frozenset(("format", "samples")), "trace")
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized trace format")
    samples = raw["samples"]
    if not isinstance(samples, list) or not samples or len(samples) > MAX_SAMPLES:
        raise ValueError("trace must contain between one and 64 samples")
    clean_samples: list[dict[str, Any]] = []
    prior_order = -1
    for sample in samples:
        if not isinstance(sample, dict):
            raise ValueError("sample must be an object")
        _exact(sample, frozenset(("observation_order", "phase", "viewport", "panel", "elements")), "sample")
        order = _natural(sample["observation_order"], "observation_order", MAX_SAMPLES)
        if order <= prior_order:
            raise ValueError("observation order must strictly increase")
        prior_order = order
        if sample["phase"] not in _PHASES:
            raise ValueError("sample has unsupported phase")
        viewport = sample["viewport"]
        if not isinstance(viewport, dict):
            raise ValueError("viewport must be an object")
        _exact(viewport, frozenset(("width", "height")), "viewport")
        viewport_width = _natural(viewport["width"], "viewport.width", MAX_EXTENT)
        viewport_height = _natural(viewport["height"], "viewport.height", MAX_EXTENT)
        if viewport_width == 0 or viewport_height == 0:
            raise ValueError("viewport cannot be empty")
        panel = _rect(sample["panel"], "panel")
        if panel["x"] + panel["width"] > viewport_width or panel["y"] + panel["height"] > viewport_height:
            raise ValueError("panel must fit inside viewport")
        elements = sample["elements"]
        if not isinstance(elements, list) or not elements or len(elements) > MAX_ELEMENTS:
            raise ValueError("sample must contain between one and 128 elements")
        clean_elements: list[dict[str, Any]] = []
        roles: set[str] = set()
        for element in elements:
            if not isinstance(element, dict):
                raise ValueError("element must be an object")
            _exact(element, frozenset(("role", "bounds")), "element")
            role = element["role"]
            if role not in _ROLES:
                raise ValueError("element has unsupported role")
            bounds = _rect(element["bounds"], "element.bounds")
            if role != "backdrop" and not _inside(bounds, panel):
                raise ValueError("non-backdrop element must fit inside panel")
            if role in ("backdrop", "panel", "heading", "selected_focus") and role in roles:
                raise ValueError("sample duplicates a singleton visual role")
            roles.add(role)
            clean_elements.append({"role": role, "bounds": bounds})
        if "panel" not in roles or "heading" not in roles:
            raise ValueError("sample must identify panel and heading geometry")
        if sample["phase"] == "focused" and "selected_focus" not in roles:
            raise ValueError("focused sample must identify selected focus geometry")
        clean_samples.append({
            "observation_order": order,
            "phase": sample["phase"],
            "viewport": {"width": viewport_width, "height": viewport_height},
            "panel": panel,
            "elements": clean_elements,
        })
    return {"format": OUTPUT_FORMAT, "samples": clean_samples}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _write_new_private_json_no_follow(path: pathlib.Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        handle = path.open("x", encoding="utf-8")
    except FileExistsError as error:
        raise ValueError("refusing to overwrite an existing private trace") from error
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
        sanitized = sanitize_trace(raw)
        _write_new_private_json_no_follow(output_path, sanitized)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(sanitized['samples'])} source-free menu-layout samples")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
