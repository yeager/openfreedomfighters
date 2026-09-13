#!/usr/bin/env python3
"""Gate a repeated, complete startup-menu scene-request observation.

The inputs must be two byte-identical v2 outputs from
``menu_scene_request_trace.py``.  This utility does not inspect executables,
game data, archives, assets, screenshots, logs, or raw observer output.  It
retains only the already source-free structural trace after revalidating the
terminal selection-to-manager request chain.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any

import menu_scene_request_trace as scene_trace
import private_structural_json


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
INPUT_FORMAT = scene_trace.OUTPUT_FORMAT
OUTPUT_FORMAT = "off.startup-menu-scene-request-repeat-pair/v1"


def _validated_sanitized_trace(raw: Any) -> dict[str, Any]:
    if not isinstance(raw, dict) or frozenset(raw) != frozenset(("format", "events")):
        raise ValueError("sanitized trace must contain only format and events")
    if raw["format"] != INPUT_FORMAT:
        raise ValueError("unrecognized sanitized trace format")
    return scene_trace.sanitize_trace({
        "format": scene_trace.INPUT_FORMAT,
        "events": raw["events"],
    })


def _has_terminal_complete_chain(trace: dict[str, Any]) -> bool:
    """Require the last event to close the exact source-free request chain."""
    terminal = trace["events"][-1]
    if not (
        terminal["phase"] == "completion"
        and terminal["selection"] == "delivered"
        and terminal["active_window"] == "replaced"
        and terminal["active_window_selection_delivery"] == "delivered"
        and terminal["receiver_route"] == "scene_request"
        and terminal["receiver_manager_edge"] == "entered"
        and terminal["manager_request"] in ("request_only", "clear_then_request")
        and terminal["request_target"] == "validated"
        and terminal["outcome"] == "success"
    ):
        return False
    # A package need not be reached by this route.  If it is, its successful
    # admission is required; a candidate/rejection cannot close a success
    # chain and therefore must not authorize later package work.
    return terminal["package_admission"] in ("not_entered", "admitted")


def sanitize_repeat_pair(first: Any, second: Any) -> dict[str, Any]:
    """Return the identical complete trace only when both observations agree."""
    first_clean = _validated_sanitized_trace(first)
    second_clean = _validated_sanitized_trace(second)
    if first_clean["events"] != second_clean["events"]:
        raise ValueError("repeat observations disagree on the scene-request trace")
    if not _has_terminal_complete_chain(first_clean):
        raise ValueError("repeat observations lack a terminal complete scene-request chain")
    return {"format": OUTPUT_FORMAT, "events": first_clean["events"]}


def _read_identical_private_traces(first_path: pathlib.Path,
                                   second_path: pathlib.Path) -> tuple[Any, Any]:
    """Read both descriptor-bound files and make textual repeatability explicit."""
    first_bytes = private_structural_json.read_bytes(first_path, "first input")
    second_bytes = private_structural_json.read_bytes(second_path, "second input")
    if first_bytes != second_bytes:
        raise ValueError("repeat observations must be byte-identical sanitized v2 traces")
    return json.loads(first_bytes.decode("utf-8")), json.loads(second_bytes.decode("utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first", type=pathlib.Path, help="first private sanitized v2 trace")
    parser.add_argument("second", type=pathlib.Path, help="second private sanitized v2 trace")
    parser.add_argument("output", type=pathlib.Path, help="new private repeat-pair JSON path")
    args = parser.parse_args()
    try:
        first_path = private_structural_json.outside_repository(
            args.first, REPOSITORY_ROOT, "first input")
        second_path = private_structural_json.outside_repository(
            args.second, REPOSITORY_ROOT, "second input")
        output_path = private_structural_json.outside_repository(
            args.output, REPOSITORY_ROOT, "output")
        if len({first_path, second_path, output_path}) != 3:
            raise ValueError("inputs and output paths must differ")
        first, second = _read_identical_private_traces(first_path, second_path)
        result = sanitize_repeat_pair(first, second)
        private_structural_json.write_new_json(output_path, result, "private repeat-pair trace")
    except (OSError, UnicodeDecodeError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote one source-free startup-menu scene-request repeat-pair")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
