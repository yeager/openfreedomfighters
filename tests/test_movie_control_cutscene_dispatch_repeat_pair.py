"""Repeated MovieControl handoff observations remain source-free and exact."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import movie_control_cutscene_dispatch_repeat_pair as pair  # noqa: E402
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


def sanitized(*events: dict[str, object]) -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": list(events)})


class MovieControlCutsceneDispatchRepeatPairTests(unittest.TestCase):
    def test_accepts_two_identical_completed_handoff_observations(self) -> None:
        first = sanitized(event())
        result = pair.sanitize_repeat_pair(first, sanitized(event()))
        self.assertEqual(result["format"], pair.OUTPUT_FORMAT)
        self.assertEqual(result["events"], first["events"])

    def test_accepts_two_identical_failed_boundary_observations(self) -> None:
        failure = event(
            phase="failure", event16_gate="failed", handoff="not_attempted",
            delivery_mode="not_observed", player_activation="not_started",
            outcome="failure", external_service="not_entered",
        )
        result = pair.sanitize_repeat_pair(sanitized(failure), sanitized(failure))
        self.assertEqual(result["events"][0]["event16_gate"], "failed")

    def test_rejects_any_changed_structural_relation(self) -> None:
        first = sanitized(event())
        for changed in (
            event(callback_ordinal=8),
            event(component_status_after=8),
            event(owner_status_after=4),
            event(external_service="not_entered"),
        ):
            with self.subTest(changed=changed):
                with self.assertRaises(ValueError):
                    pair.sanitize_repeat_pair(first, sanitized(changed))

    def test_rejects_a_nonterminal_or_non_sanitized_trace(self) -> None:
        waiting = event(
            phase="event16", event16_gate="waiting", handoff="not_attempted",
            delivery_mode="not_observed", player_activation="not_started",
            handoff_sender_is_movie_owner=False,
            handoff_target_is_sequence_owner=False, external_service="not_entered",
        )
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(sanitized(waiting), sanitized(waiting))
        raw = {"format": trace.INPUT_FORMAT, "events": [event()]}
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(raw, raw)

    def test_rejects_retail_or_executable_fields_via_revalidation(self) -> None:
        specimen = sanitized(event())
        specimen["events"][0]["address"] = 1
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(specimen, sanitized(event()))


if __name__ == "__main__":
    unittest.main()
