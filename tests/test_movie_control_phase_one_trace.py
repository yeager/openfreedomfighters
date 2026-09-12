"""The MovieControl observer format must not gain retail-content fields."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import movie_control_phase_one_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "dispatch_order": 0,
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


class MovieControlPhaseOneTraceTests(unittest.TestCase):
    def test_keeps_only_fixed_structural_schema(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"], [event()])

    def test_rejects_retail_and_executable_fields(self) -> None:
        for forbidden in ("identifier", "address", "offset", "symbol", "string", "asset", "payload", "bytes", "path", "screenshot"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [specimen]})

    def test_rejects_non_phase_one_and_non_monotonic_dispatch(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(phase=2)]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(dispatch_order=1), event(dispatch_order=1)]})

    def test_correlates_global_lifecycle_ordinary_membership_and_completion(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
        observed = result["events"][0]
        self.assertTrue(observed["global_lifecycle_entered"])
        self.assertTrue(observed["global_lifecycle_completed"])
        self.assertEqual(observed["global_lifecycle_outcome"], "success")
        self.assertFalse(observed["ordinary_member_before"])
        self.assertTrue(observed["ordinary_member_after"])
        self.assertTrue(observed["phase_one_completed"])

    def test_rejects_incoherent_global_lifecycle_claims(self) -> None:
        for changes in (
            {"global_lifecycle_entered": False},
            {"global_lifecycle_completed": False, "global_lifecycle_outcome": "success"},
            {"global_lifecycle_outcome": "failure"},
            {"global_lifecycle_entered": False, "global_lifecycle_completed": False,
             "global_lifecycle_outcome": "not_observed", "phase_one_completed": True},
        ):
            with self.subTest(changes=changes):
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(**changes)]})


if __name__ == "__main__":
    unittest.main()
