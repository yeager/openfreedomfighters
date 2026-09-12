"""The CutSequenceCommand observer format must not gain retail-content fields."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import cut_sequence_command_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0,
        "phase": "phase_one",
        "callback_ordinal": 3,
        "component_is_constructed": True,
        "reader_graph_receipt": "complete",
        "component_status_before": 0,
        "component_status_after": 4,
        "owner_status_before": 0,
        "owner_status_after": 4,
        "registration_attempt": "succeeded",
        "registration_order_category": "before_target_delivery",
        "delivery": "target_delivered",
        "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


class CutSequenceCommandTraceTests(unittest.TestCase):
    def test_keeps_only_fixed_structural_schema(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"], [event()])

    def test_rejects_retail_and_executable_fields(self) -> None:
        for forbidden in ("address", "offset", "symbol", "string", "asset", "bytes", "path", "screenshot", "value", "retail_value"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [specimen]})

    def test_rejects_backward_phase_and_ambiguous_delivery(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(observation_order=0, phase="reader"), event(observation_order=1, phase="construction")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(delivery="context_only")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(registration_attempt="not_attempted", registration_order_category="without_target_delivery", delivery="not_attempted")]})


if __name__ == "__main__":
    unittest.main()
