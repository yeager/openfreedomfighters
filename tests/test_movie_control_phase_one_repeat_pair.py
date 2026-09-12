"""Repeated MovieControl phase-one observations remain structural and exact."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import movie_control_phase_one_repeat_pair as pair  # noqa: E402
import movie_control_phase_one_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "dispatch_order": 4,
        "phase": 1,
        "callback_ordinal": 3,
        "component_is_constructed": True,
        "owner_is_constructed_owner": True,
        "component_status_before": 0,
        "component_status_after": 4,
        "owner_status_before": 0,
        "owner_status_after": 4,
        "event_member_before": False,
        "event_member_after": True,
        "outcome": "success",
        "external_service": "entered",
        "global_lifecycle_entered": True,
        "global_lifecycle_completed": True,
        "global_lifecycle_outcome": "success",
        "ordinary_member_before": False,
        "ordinary_member_after": True,
        "phase_one_completed": True,
    }
    value.update(changes)
    return value


def sanitized(*events: dict[str, object]) -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": list(events)})


class MovieControlPhaseOneRepeatPairTests(unittest.TestCase):
    def test_accepts_one_identical_completed_constructed_candidate_per_run(self) -> None:
        first = sanitized(event())
        second = sanitized(event())
        result = pair.sanitize_repeat_pair(first, second)
        self.assertEqual(result["format"], pair.OUTPUT_FORMAT)
        self.assertEqual(result["candidate"]["callback_ordinal"], 3)
        self.assertEqual(result["candidate"]["component_status_after"], 4)

    def test_rejects_incomplete_or_unconstructed_candidates(self) -> None:
        variants = (
            event(component_is_constructed=False),
            event(owner_is_constructed_owner=False),
            event(global_lifecycle_entered=False, global_lifecycle_completed=False,
                  global_lifecycle_outcome="not_observed", phase_one_completed=False),
            event(global_lifecycle_completed=False, global_lifecycle_outcome="not_observed",
                  phase_one_completed=False),
            event(global_lifecycle_completed=False, global_lifecycle_outcome="failure",
                  outcome="failure", phase_one_completed=False),
        )
        for candidate in variants:
            with self.subTest(candidate=candidate):
                with self.assertRaises(ValueError):
                    pair.sanitize_repeat_pair(sanitized(candidate), sanitized(candidate))

    def test_requires_exact_candidate_and_effect_relation(self) -> None:
        first = sanitized(event())
        for changed in (
            event(callback_ordinal=4),
            event(dispatch_order=5),
            event(component_status_after=8),
            event(owner_status_after=8),
            event(event_member_after=False),
            event(external_service="not_entered"),
            event(ordinary_member_after=False),
        ):
            with self.subTest(changed=changed):
                with self.assertRaises(ValueError):
                    pair.sanitize_repeat_pair(first, sanitized(changed))

    def test_rejects_ambiguous_candidates_and_non_sanitized_input(self) -> None:
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(sanitized(event(), event(dispatch_order=5)), sanitized(event()))
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
