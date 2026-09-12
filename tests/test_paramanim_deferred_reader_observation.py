"""The ParamAnim deferred-reader observation format must remain source-free."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import paramanim_deferred_reader_observation as observation  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0, "stage": "owner_reader",
        "owner_input_form": "accepted_bounded", "component_input_form": "accepted_bounded",
        "terminal_rule": "required", "attachment_delimiter_rule": "required",
        "trailing_bytes_policy": "accepted_none_only", "destination_write": "owner_reader",
        "reader_prerequisite": "preparation_before_owner_reader",
        "raw_value_preservation": "preserved", "ownership": "reader_local",
        "duplicate_reentry": "rejected_duplicate", "failure_rollback": "not_observed",
        "later_consumer": "none", "outcome": "success", "side_effect": "none",
    }
    value.update(changes)
    return value


class ParamAnimDeferredReaderObservationTests(unittest.TestCase):
    def test_keeps_only_the_bounded_structural_schema(self) -> None:
        result = observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": [event()]})
        self.assertEqual(result, {"format": observation.OUTPUT_FORMAT, "events": [event()]})

    def test_rejects_source_and_identity_fields(self) -> None:
        for forbidden in ("id", "string", "path", "asset", "bytes", "address", "offset", "symbol", "screenshot"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": [specimen]})

    def test_rejected_input_cannot_claim_a_write_or_side_effect(self) -> None:
        rejected = event(owner_input_form="rejected_malformed", component_input_form="rejected_malformed",
                         terminal_rule="rejected_missing", destination_write="not_observed",
                         reader_prerequisite="not_observed",
                         raw_value_preservation="not_observed", ownership="not_observed",
                         duplicate_reentry="not_observed", failure_rollback="no_write", outcome="failure")
        self.assertEqual(observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": [rejected]})["events"], [rejected])
        with self.assertRaises(ValueError):
            observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": [event(owner_input_form="rejected_malformed")]})

    def test_write_requires_complete_grammar_and_state_contract(self) -> None:
        for changes in (
            {"terminal_rule": "rejected_missing"},
            {"attachment_delimiter_rule": "rejected_duplicate"},
            {"trailing_bytes_policy": "rejected_present"},
            {"reader_prerequisite": "not_observed"},
            {"raw_value_preservation": "not_observed"},
            {"ownership": "not_observed"},
            {"duplicate_reentry": "not_observed"},
            {"destination_write": "component_reader"},
        ):
            with self.subTest(changes=changes):
                with self.assertRaises(ValueError):
                    observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": [event(**changes)]})

    def test_enforces_lifecycle_stage_order_and_consumer_boundary(self) -> None:
        callback = event(observation_order=1, stage="later_callback", destination_write="not_observed",
                         reader_prerequisite="not_observed",
                         raw_value_preservation="not_observed", ownership="not_observed",
                         duplicate_reentry="not_observed", later_consumer="later_callback")
        result = observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": [event(), callback]})
        self.assertEqual(result["events"], [event(), callback])
        with self.assertRaises(ValueError):
            observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": [callback, event(observation_order=2)]})
        with self.assertRaises(ValueError):
            observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": [event(later_consumer="later_callback")]})

    def test_requires_the_full_deferred_reader_order_for_each_write_boundary(self) -> None:
        component = event(stage="component_reader", destination_write="component_reader",
                          reader_prerequisite="preparation_and_owner_reader_before_component_reader")
        result = observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": [component]})
        self.assertEqual(result["events"], [component])
        with self.assertRaises(ValueError):
            observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": [
                event(stage="component_reader", destination_write="component_reader")
            ]})

    def test_rollback_requires_an_accepted_post_write_failure(self) -> None:
        rollback = event(stage="failure", destination_write="not_observed",
                         reader_prerequisite="not_observed",
                         raw_value_preservation="not_observed", ownership="not_observed",
                         duplicate_reentry="not_observed", failure_rollback="rolled_back",
                         outcome="failure")
        self.assertEqual(
            observation.sanitize_observation(
                {"format": observation.INPUT_FORMAT, "events": [rollback]})["events"], [rollback])
        for changes in (
            {"stage": "owner_reader"},
            {"outcome": "success"},
            {"owner_input_form": "rejected_malformed"},
            {"destination_write": "owner_reader"},
        ):
            with self.subTest(changes=changes):
                with self.assertRaises(ValueError):
                    observation.sanitize_observation(
                        {"format": observation.INPUT_FORMAT,
                         "events": [rollback | changes]})


if __name__ == "__main__":
    unittest.main()
