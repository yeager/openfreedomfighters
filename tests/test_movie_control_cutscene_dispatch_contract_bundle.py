"""MovieControl dispatch contract bundles remain source-free and review-only."""

from __future__ import annotations

import json
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import movie_control_cutscene_dispatch_contract_bundle as bundle  # noqa: E402
import movie_control_cutscene_dispatch_repeat_pair as repeat_pair  # noqa: E402
import movie_control_cutscene_dispatch_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0, "phase": "player_activation", "callback_ordinal": 7,
        "movie_component_is_constructed": True, "movie_owner_is_constructed_owner": True,
        "sequence_component_is_constructed": True, "sequence_owner_is_constructed_owner": True,
        "movie_phase_one_completed": True, "movie_phase_two_completed": True,
        "component_status_before": 4, "component_status_after": 4,
        "owner_status_before": 0, "owner_status_after": 0, "event16_gate": "admitted",
        "handoff_sender_is_movie_owner": True, "handoff_target_is_sequence_owner": True,
        "handoff": "delivered", "delivery_mode": "synchronous",
        "player_activation": "started", "outcome": "success", "external_service": "entered",
    }
    value.update(changes)
    return value


def sanitized(*events: dict[str, object]) -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": list(events)})


def success_pair(*events: dict[str, object]) -> dict[str, object]:
    observation = sanitized(*(events or (event(),)))
    return repeat_pair.sanitize_repeat_pair(observation, observation)


def failure_trace(**changes: object) -> dict[str, object]:
    failure = {
        "phase": "failure", "event16_gate": "failed", "handoff": "failed", "delivery_mode": "not_observed",
        "player_activation": "not_started", "outcome": "failure", "external_service": "entered",
    }
    failure.update(changes)
    return sanitized(event(**failure))


class MovieControlCutsceneDispatchContractBundleTests(unittest.TestCase):
    def test_accepts_one_matching_success_and_failure_route(self) -> None:
        result = bundle.sanitize_contract_bundle(success_pair(), failure_trace())
        self.assertEqual(result["format"], bundle.OUTPUT_FORMAT)
        self.assertEqual(result["candidate"]["callback_ordinal"], 7)
        self.assertEqual(result["failure"]["handoff"], "failed")

    def test_rejects_mismatch_duplicate_and_success_in_failure_trace(self) -> None:
        for changes in (
            {"callback_ordinal": 8}, {"component_status_before": 8},
            {"owner_status_before": 8}, {"movie_phase_two_completed": False},
            {"event16_gate": "failed", "handoff_sender_is_movie_owner": False,
             "handoff_target_is_sequence_owner": False},
        ):
            with self.subTest(changes=changes):
                with self.assertRaises(ValueError):
                    bundle.sanitize_contract_bundle(success_pair(), failure_trace(**changes))
        duplicate = failure_trace()
        duplicate["events"].append(failure_trace()["events"][0])
        duplicate["events"][1]["observation_order"] = 1
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(success_pair(), duplicate)
        ambiguous = sanitized(
            event(observation_order=0),
            event(observation_order=1, phase="failure", handoff="failed",
                  event16_gate="failed", delivery_mode="not_observed", player_activation="not_started",
                  outcome="failure"),
        )
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(success_pair(), ambiguous)

    def test_rejects_missing_or_duplicate_success_routes(self) -> None:
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(success_pair(event(player_activation="not_started", phase="handoff")), failure_trace())
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(success_pair(event(), event(observation_order=1)), failure_trace())

    def test_rejects_raw_unknown_and_extra_fields(self) -> None:
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(sanitized(event()), failure_trace())
        malformed_success = success_pair()
        malformed_success["events"][0]["path"] = "forbidden"
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(malformed_success, failure_trace())
        malformed_failure = failure_trace()
        malformed_failure["events"][0]["address"] = 1
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(success_pair(), malformed_failure)

    def test_cli_writes_only_new_private_output(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            private = pathlib.Path(directory)
            success_path, failure_path, output_path = (private / name for name in ("success.json", "failure.json", "bundle.json"))
            success_path.write_text(json.dumps(success_pair()), encoding="utf-8")
            failure_path.write_text(json.dumps(failure_trace()), encoding="utf-8")
            old_argv = sys.argv
            try:
                sys.argv = ["bundle", str(success_path), str(failure_path), str(output_path)]
                self.assertEqual(bundle.main(), 0)
                self.assertEqual(json.loads(output_path.read_text(encoding="utf-8"))["format"], bundle.OUTPUT_FORMAT)
                self.assertEqual(bundle.main(), 1)
            finally:
                sys.argv = old_argv


if __name__ == "__main__":
    unittest.main()
