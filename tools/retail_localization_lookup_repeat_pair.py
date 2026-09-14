#!/usr/bin/env python3
"""Accept two identical, source-free retail localization observations."""

from __future__ import annotations

from typing import Any

import retail_localization_lookup_trace as trace


INPUT_FORMAT = trace.OUTPUT_FORMAT
OUTPUT_FORMAT = "off.retail-localization-lookup-repeat-pair/v1"


def _validated_sanitized_trace(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != frozenset(("format", "events")):
        raise ValueError("sanitized trace must contain only format and events")
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized sanitized trace format")
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": raw["events"]})


def sanitize_repeat_pair(first: Any, second: Any) -> dict[str, Any]:
    """Return a repeat pair only for exact complete structural agreement."""
    first_clean = _validated_sanitized_trace(first)
    second_clean = _validated_sanitized_trace(second)
    if first_clean["events"] != second_clean["events"]:
        raise ValueError("repeat observations disagree on lookup behavior")
    return {"format": OUTPUT_FORMAT, "events": first_clean["events"]}
