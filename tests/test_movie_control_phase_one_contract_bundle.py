"""MovieControl phase-one contract bundles stay structural and review-only."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import movie_control_phase_one_contract_bundle as bundle  # noqa: E402
import movie_control_phase_one_repeat_pair as repeat_pair  # noqa: E402
import movie_control_phase_one_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "dispatch_order": 4, "phase": 1, "callback_ordinal": 3,
        "component_is_constructed": True, "owner_is_constructed_owner": True,
        "component_status_before": 0, "component_status_after": 4,
        "owner_status_before": 0, "owner_status_after": 4,
        "event_member_before": False, "event_member_after": True,
        "outcome": "success", "external_service": "entered",
        "global_lifecycle_entered": True, "global_lifecycle_completed": True,
        "global_lifecycle_outcome": "success", "ordinary_member_before": False,
        "ordinary_member_after": True, "phase_one_completed": True,
    }
    value.update(changes)
    return value


def success_pair() -> dict[str, object]:
    success = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
    return repeat_pair.sanitize_repeat_pair(success, success)


def failure_trace(**changes: object) -> dict[str, object]:
    failure = {
        "component_status_after": 0, "owner_status_after": 0,
        "event_member_after": False, "ordinary_member_after": False,
        "outcome": "failure", "external_service": "entered",
        "global_lifecycle_completed": False, "global_lifecycle_outcome": "failure",
        "phase_one_completed": False,
    }
    failure.update(changes)
    return trace.sanitize_trace({
        "format": trace.INPUT_FORMAT,
        "events": [event(**failure)],
    })


class MovieControlPhaseOneContractBundleTests(unittest.TestCase):
    def test_accepts_repeated_success_and_matching_failure(self) -> None:
        result = bundle.sanitize_contract_bundle(success_pair(), failure_trace())
        self.assertEqual(result["format"], bundle.OUTPUT_FORMAT)
        self.assertEqual(result["candidate"]["callback_ordinal"], 3)
        self.assertEqual(result["failure"]["outcome"], "failure")
        self.assertFalse(result["failure"]["phase_one_completed"])

    def test_rejects_nonmatching_callback_or_preconditions(self) -> None:
        for changed in (
            {"callback_ordinal": 4}, {"dispatch_order": 5},
            {"component_status_before": 1}, {"owner_status_before": 1},
            {"event_member_before": True}, {"ordinary_member_before": True},
        ):
            with self.subTest(changed=changed):
                with self.assertRaises(ValueError):
                    bundle.sanitize_contract_bundle(success_pair(), failure_trace(**changed))

    def test_rejects_completion_success_or_ambiguous_failure_trace(self) -> None:
        for changed in (
            {"phase_one_completed": True},
            {"global_lifecycle_completed": True, "global_lifecycle_outcome": "success"},
            {"outcome": "success"},
        ):
            with self.subTest(changed=changed):
                with self.assertRaises(ValueError):
                    bundle.sanitize_contract_bundle(success_pair(), failure_trace(**changed))
        ambiguous = failure_trace()
        ambiguous["events"].append(event(dispatch_order=4))
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(success_pair(), ambiguous)

    def test_rejects_raw_unknown_or_retail_fields(self) -> None:
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle({"format": trace.OUTPUT_FORMAT, "events": [event()]}, failure_trace())
        specimen = failure_trace()
        specimen["events"][0]["address"] = 1
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(success_pair(), specimen)
        malformed_pair = success_pair()
        malformed_pair["candidate"]["path"] = "forbidden"
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(malformed_pair, failure_trace())


if __name__ == "__main__":
    unittest.main()
