"""The CutSequence player observer format must not admit retail-content fields."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import cut_sequence_player_lifecycle_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0,
        "phase": "phase_two",
        "callback_ordinal": 7,
        "sequence_component_constructed": True,
        "sequence_owner_constructed": True,
        "reader_graph_receipt": "complete",
        "component_status_before": 4,
        "component_status_after": 4,
        "owner_status_before": 4,
        "owner_status_after": 4,
        "player_state_before": "phase_one_ready",
        "player_state_after": "phase_two_ready",
        "receiver_state": "sealed",
        "member_sweep": "derived",
        "reference_sweep": "camera_and_sequence",
        "activation": "not_attempted",
        "completion": "not_observed",
        "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


class CutSequencePlayerLifecycleTraceTests(unittest.TestCase):
    def test_keeps_only_fixed_player_lifecycle_schema(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"], [event()])

    def test_rejects_retail_and_executable_fields(self) -> None:
        for forbidden in ("address", "offset", "symbol", "string", "asset", "bytes", "path", "screenshot", "member", "reference"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [specimen]})

    def test_rejects_invalid_player_transitions(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(phase="activation", player_state_before="phase_one_ready",
                                                   player_state_after="active", activation="started")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(receiver_state="open")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(member_sweep="derived", phase="activation")]})


if __name__ == "__main__":
    unittest.main()
