"""The first-mission observer format must remain structural and source-free."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import first_mission_observation_trace as trace  # noqa: E402


FINGERPRINT = "0" * 64


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0,
        "probe": "launch",
        "boundary": "handoff",
        "state": "loading_visible",
        "visible_change": True,
    }
    value.update(changes)
    return value


def document(events: list[dict[str, object]]) -> dict[str, object]:
    return {
        "format": trace.INPUT_FORMAT,
        "method_version": 1,
        "verified_data_manifest_fingerprint": FINGERPRINT,
        "platform": "windows",
        "architecture": "x86",
        "input_device": "keyboard_mouse",
        "run_kind": "baseline",
        "events": events,
    }


class FirstMissionObservationTraceTests(unittest.TestCase):
    def test_keeps_only_fixed_structural_schema(self) -> None:
        source = document([event(), event(
            observation_order=1, probe="idle", boundary="hud", state="stable", visible_change=False,
        )])
        result = trace.sanitize_trace(source)
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"], source["events"])

    def test_accepts_every_documented_boundary_category(self) -> None:
        events = [event()]
        values = (
            ("control", "movement_and_camera"), ("camera", "rotates_with_look"),
            ("player", "controllable"), ("collision", "sliding"),
            ("interaction", "prompt_only"), ("mission", "unchanged"), ("hud", "stable"),
        )
        for order, (boundary, state) in enumerate(values, start=1):
            events.append(event(observation_order=order, probe="idle", boundary=boundary,
                                state=state, visible_change=order == 3))
        self.assertEqual(trace.sanitize_trace(document(events))["events"], events)

    def test_rejects_content_and_executable_fields(self) -> None:
        for forbidden in ("address", "offset", "symbol", "string", "asset", "bytes", "path", "screenshot", "object_name", "payload"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace(document([specimen]))

    def test_rejects_invalid_start_order_and_cross_boundary_state(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace(document([event(probe="idle")]))
        with self.assertRaises(ValueError):
            trace.sanitize_trace(document([event(boundary="player", state="controllable")]))
        with self.assertRaises(ValueError):
            trace.sanitize_trace(document([event(), event(
                observation_order=1, probe="launch", boundary="hud", state="stable", visible_change=False,
            )]))
        with self.assertRaises(ValueError):
            trace.sanitize_trace(document([event(), event(
                observation_order=0, probe="idle", boundary="hud", state="stable", visible_change=False,
            )]))

    def test_rejects_nonopaque_manifest_and_unbounded_metadata(self) -> None:
        source = document([event()])
        source["verified_data_manifest_fingerprint"] = "scene-name"
        with self.assertRaises(ValueError):
            trace.sanitize_trace(source)
        source = document([event()])
        source["platform"] = "other"
        with self.assertRaises(ValueError):
            trace.sanitize_trace(source)


if __name__ == "__main__":
    unittest.main()
