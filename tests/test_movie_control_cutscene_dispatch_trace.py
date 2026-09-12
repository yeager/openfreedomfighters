"""The MovieControl-to-player observer format must remain source-free."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import movie_control_cutscene_dispatch_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0,
        "phase": "player_activation",
        "callback_ordinal": 7,
        "movie_component_is_constructed": True,
        "movie_owner_is_constructed_owner": True,
        "sequence_component_is_constructed": True,
        "sequence_owner_is_constructed_owner": True,
        "movie_phase_one_completed": True,
        "movie_phase_two_completed": True,
        "component_status_before": 4,
        "component_status_after": 4,
        "owner_status_before": 0,
        "owner_status_after": 0,
        "event16_gate": "admitted",
        "handoff_sender_is_movie_owner": True,
        "handoff_target_is_sequence_owner": True,
        "handoff": "delivered",
        "delivery_mode": "synchronous",
        "player_activation": "started",
        "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


class MovieControlCutsceneDispatchTraceTests(unittest.TestCase):
    def test_keeps_only_fixed_structural_schema(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"], [event()])

    def test_rejects_retail_and_executable_fields(self) -> None:
        for forbidden in ("address", "offset", "symbol", "string", "asset", "bytes", "path", "screenshot", "target_id"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [specimen]})

    def test_rejects_unbound_or_premature_player_start(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(handoff_target_is_sequence_owner=False)]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(phase="handoff", player_activation="started")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(movie_phase_one_completed=False)]})

    def test_requires_a_delivered_bound_handoff_for_synchronous_delivery(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(handoff="attempted")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(handoff_target_is_sequence_owner=False,
                                                   delivery_mode="synchronous")]})

    def test_records_phase_two_relation_without_making_it_a_new_event_gate(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                       "events": [event(movie_phase_two_completed=False)]})
        self.assertFalse(result["events"][0]["movie_phase_two_completed"])

    def test_rejects_backwards_phase_and_unexplained_failure(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [
                event(observation_order=1, phase="player_activation"),
                event(observation_order=2, phase="handoff", player_activation="not_started"),
            ]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(outcome="failure")]})


if __name__ == "__main__":
    unittest.main()
