#!/usr/bin/env python3
"""Accept two identical, source-free ParamAnim reader observations.

Inputs must already have been sanitized by
``paramanim_deferred_reader_observation.py``.  This tool neither opens an
executable nor reads game data.  It revalidates both small categorical records,
requires the same complete reader contract in two fresh observations, and
writes its result outside the repository without overwriting an existing file.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any

import paramanim_deferred_reader_observation as observation


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = observation.OUTPUT_FORMAT
OUTPUT_FORMAT = "off.paramanim-deferred-reader-repeat-pair/v1"


def _validated_sanitized_observation(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != frozenset(("format", "events")):
        raise ValueError("sanitized observation must contain only format and events")
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized sanitized observation format")
    return observation.sanitize_observation({
        "format": observation.INPUT_FORMAT,
        "events": raw["events"],
    })


def _has_complete_contract(events: list[dict[str, Any]]) -> bool:
    """Require positive and negative evidence before any reader is considered."""
    write_indexes = [index for index, event in enumerate(events)
                     if event["destination_write"] in ("owner_reader", "component_reader")]
    if len(write_indexes) != 1:
        return False
    write_index = write_indexes[0]
    writer = events[write_index]
    if not any(event["stage"] == "deferred_preparation" and
               event["outcome"] == "success" for event in events[:write_index]):
        return False
    if writer["destination_write"] == "component_reader" and not any(
            event["stage"] == "owner_reader" and event["outcome"] == "success"
            for event in events[:write_index]):
        return False
    rejected_forms = {
        form for event in events
        for form in (event["owner_input_form"], event["component_input_form"])
        if form in ("rejected_malformed", "rejected_unsupported")
    }
    if rejected_forms != {"rejected_malformed", "rejected_unsupported"}:
        return False
    callback_indexes = [index for index, event in enumerate(events)
                        if event["stage"] == "later_callback"]
    consumer_indexes = [index for index, event in enumerate(events)
                        if event["later_consumer"] == "later_callback"]
    if bool(callback_indexes) != bool(consumer_indexes):
        return False
    if callback_indexes and (len(callback_indexes) != 1 or len(consumer_indexes) != 1 or
                             callback_indexes[0] <= write_index or
                             events[callback_indexes[0]]["outcome"] != "success"):
        return False
    return True


def sanitize_repeat_pair(first: Any, second: Any) -> dict[str, Any]:
    """Return an exact repeated contract only when both runs agree."""
    first_clean = _validated_sanitized_observation(first)
    second_clean = _validated_sanitized_observation(second)
    if first_clean["events"] != second_clean["events"]:
        raise ValueError("repeat observations disagree on the reader contract")
    if not _has_complete_contract(first_clean["events"]):
        raise ValueError("repeat observations lack a complete reader contract")
    return {"format": OUTPUT_FORMAT, "events": first_clean["events"]}


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first", type=pathlib.Path, help="first private sanitized observation")
    parser.add_argument("second", type=pathlib.Path, help="second private sanitized observation")
    parser.add_argument("output", type=pathlib.Path, help="new private repeat-pair JSON path")
    args = parser.parse_args()
    try:
        first_path = _outside_repository(args.first, "first input")
        second_path = _outside_repository(args.second, "second input")
        output_path = _outside_repository(args.output, "output")
        if len({first_path, second_path, output_path}) != 3:
            raise ValueError("inputs and output paths must differ")
        if output_path.exists():
            raise ValueError("refusing to overwrite an existing private observation")
        result = sanitize_repeat_pair(
            json.loads(first_path.read_text(encoding="utf-8")),
            json.loads(second_path.read_text(encoding="utf-8")),
        )
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one source-free ParamAnim deferred-reader repeat-pair")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
