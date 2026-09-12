"""The startup-menu scene-request observer must remain source-free."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import menu_scene_request_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0,
        "phase": "receiver",
        "callback_ordinal": 4,
        "menu_component_constructed": True,
        "reader_graph_receipt": "complete",
        "component_status_before": 4,
        "component_status_after": 4,
        "owner_status_before": 4,
        "owner_status_after": 4,
        "selection": "delivered",
        "active_window": "replaced",
        "receiver_route": "receiver_only",
        "manager_request": "not_entered",
        "request_target": "not_observed",
        "package_admission": "not_entered",
        "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


class MenuSceneRequestTraceTests(unittest.TestCase):
    def test_keeps_only_fixed_structural_schema(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"], [event()])

    def test_allows_only_a_complete_request_and_admission_chain(self) -> None:
        record = event(
            phase="package_admission",
            receiver_route="scene_request",
            manager_request="clear_then_request",
            request_target="validated",
            package_admission="admitted",
        )
        self.assertEqual(trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [record]})["events"], [record])

    def test_rejects_content_and_executable_fields(self) -> None:
        for forbidden in ("address", "offset", "symbol", "string", "asset", "bytes", "path", "screenshot", "target_name", "mission"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [specimen]})

    def test_rejects_unproven_handoff_and_backward_phase(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(receiver_route="scene_request")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(), event(observation_order=1, phase="selection", selection="resolved", active_window="not_observed")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(phase="package_admission", receiver_route="scene_request", manager_request="request_only", request_target="retained", package_admission="candidate")]})


if __name__ == "__main__":
    unittest.main()
