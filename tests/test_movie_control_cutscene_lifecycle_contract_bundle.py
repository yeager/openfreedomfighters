"""End-to-end MovieControl lifecycle evidence must remain structural only."""

from __future__ import annotations

import json
import pathlib
import sys
import tempfile
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import cut_sequence_player_lifecycle_trace as player_trace  # noqa: E402
import movie_control_cutscene_dispatch_contract_bundle as dispatch_bundle  # noqa: E402
import movie_control_cutscene_dispatch_repeat_pair as dispatch_repeat  # noqa: E402
import movie_control_cutscene_dispatch_trace as dispatch_trace  # noqa: E402
import movie_control_cutscene_lifecycle_contract_bundle as bundle  # noqa: E402
import movie_control_phase_one_contract_bundle as phase_bundle  # noqa: E402
import movie_control_phase_one_repeat_pair as phase_repeat  # noqa: E402
import movie_control_phase_one_trace as phase_trace  # noqa: E402


def phase_event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "dispatch_order": 4, "phase": 1, "callback_ordinal": 7,
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


def phase_contract() -> dict[str, object]:
    success = phase_trace.sanitize_trace({"format": phase_trace.INPUT_FORMAT, "events": [phase_event()]})
    pair = phase_repeat.sanitize_repeat_pair(success, success)
    failure = phase_trace.sanitize_trace({"format": phase_trace.INPUT_FORMAT, "events": [phase_event(
        component_status_after=0, owner_status_after=0, event_member_after=False,
        ordinary_member_after=False, outcome="failure", global_lifecycle_completed=False,
        global_lifecycle_outcome="failure", phase_one_completed=False,
    )]})
    return phase_bundle.sanitize_contract_bundle(pair, failure)


def dispatch_event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0, "phase": "player_activation", "callback_ordinal": 7,
        "movie_component_is_constructed": True, "movie_owner_is_constructed_owner": True,
        "sequence_component_is_constructed": True, "sequence_owner_is_constructed_owner": True,
        "movie_phase_one_completed": True, "movie_phase_two_completed": True,
        "component_status_before": 4, "component_status_after": 4,
        "owner_status_before": 0, "owner_status_after": 0, "event16_gate": "admitted",
        "handoff_sender_is_movie_owner": True, "handoff_target_is_sequence_owner": True,
        "handoff": "delivered", "delivery_mode": "synchronous", "player_activation": "started",
        "outcome": "success", "external_service": "entered",
    }
    value.update(changes)
    return value


def dispatch_contract() -> dict[str, object]:
    success = dispatch_trace.sanitize_trace({"format": dispatch_trace.INPUT_FORMAT, "events": [dispatch_event()]})
    pair = dispatch_repeat.sanitize_repeat_pair(success, success)
    failure = dispatch_trace.sanitize_trace({"format": dispatch_trace.INPUT_FORMAT, "events": [dispatch_event(
        phase="failure", event16_gate="failed", handoff="failed", delivery_mode="not_observed",
        player_activation="not_started", outcome="failure",
    )]})
    return dispatch_bundle.sanitize_contract_bundle(pair, failure)


def player_event(phase: str, before: str, after: str, receiver: str, activation: str,
                 completion: str, order: int, **changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": order, "phase": phase, "callback_ordinal": 7,
        "sequence_component_constructed": True, "sequence_owner_constructed": True,
        "reader_graph_receipt": "complete", "component_status_before": 4,
        "component_status_after": 4, "owner_status_before": 4, "owner_status_after": 4,
        "player_state_before": before, "player_state_after": after, "receiver_state": receiver,
        "member_sweep": "not_entered", "reference_sweep": "not_entered",
        "activation": activation, "completion": completion, "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


def player_success() -> dict[str, object]:
    return player_trace.sanitize_trace({"format": player_trace.INPUT_FORMAT, "events": [
        player_event("phase_one", "cold", "phase_one_ready", "open", "not_attempted", "not_observed", 0),
        player_event("phase_two", "phase_one_ready", "phase_two_ready", "sealed", "not_attempted", "not_observed", 1,
                     member_sweep="derived", reference_sweep="camera_and_sequence"),
        player_event("activation", "phase_two_ready", "active", "sealed", "started", "pending", 2),
        player_event("completion", "active", "completed", "sealed", "not_attempted", "completed", 3),
    ]})


def player_failure() -> dict[str, object]:
    return player_trace.sanitize_trace({"format": player_trace.INPUT_FORMAT, "events": [
        player_event("failure", "phase_two_ready", "failed", "sealed", "failed", "failed", 0,
                     outcome="failure"),
    ]})


class MovieControlCutsceneLifecycleContractBundleTests(unittest.TestCase):
    def test_accepts_repeatable_complete_player_route_and_failure(self) -> None:
        result = bundle.sanitize_contract_bundle(
            phase_contract(), dispatch_contract(), player_success(), player_success(), player_failure())
        self.assertEqual(result["format"], bundle.OUTPUT_FORMAT)
        self.assertEqual([event["phase"] for event in result["player_route"]],
                         ["phase_one", "phase_two", "activation", "completion"])

    def test_rejects_retail_fields_mismatched_callback_and_nonrepeatable_player_route(self) -> None:
        malformed = dispatch_contract()
        malformed["candidate"]["address"] = 1
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(phase_contract(), malformed, player_success(), player_success(), player_failure())
        mismatched = player_success()
        mismatched["events"][2]["callback_ordinal"] = 8
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(phase_contract(), dispatch_contract(), player_success(), mismatched, player_failure())
        bad_failure = player_failure()
        bad_failure["events"][0]["component_status_before"] = 8
        with self.assertRaises(ValueError):
            bundle.sanitize_contract_bundle(phase_contract(), dispatch_contract(), player_success(), player_success(), bad_failure)

    def test_cli_uses_new_regular_private_files_without_following_symlinks(self) -> None:
        records = (phase_contract(), dispatch_contract(), player_success(), player_success(), player_failure())
        with tempfile.TemporaryDirectory() as directory:
            private = pathlib.Path(directory)
            inputs = [private / f"input-{index}.json" for index in range(len(records))]
            for path, record in zip(inputs, records, strict=True):
                path.write_text(json.dumps(record), encoding="utf-8")
            output = private / "bundle.json"
            old_argv = sys.argv
            try:
                sys.argv = ["bundle", *(str(path) for path in inputs), str(output)]
                self.assertEqual(bundle.main(), 0)
                self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["format"], bundle.OUTPUT_FORMAT)
                self.assertEqual(bundle.main(), 1)
            finally:
                sys.argv = old_argv
            target = private / "private-target.json"
            target.write_text('{"path":"must not be read"}', encoding="utf-8")
            link = private / "link.json"
            link.symlink_to(target.name)
            with self.assertRaisesRegex(ValueError, "must not be a symlink"):
                bundle._outside_repository(link, "phase")
            self.assertTrue(target.exists())


if __name__ == "__main__":
    unittest.main()
