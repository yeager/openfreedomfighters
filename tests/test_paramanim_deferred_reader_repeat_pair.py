"""Repeated ParamAnim observations must agree before reader review."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import paramanim_deferred_reader_observation as observation  # noqa: E402
import paramanim_deferred_reader_repeat_pair as repeat_pair  # noqa: E402


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


def rejected(order: int = 1) -> dict[str, object]:
    return event(observation_order=order, stage="failure",
                 owner_input_form="rejected_malformed",
                 component_input_form="rejected_malformed",
                 terminal_rule="rejected_missing", destination_write="not_observed",
                 reader_prerequisite="not_observed", raw_value_preservation="not_observed",
                 ownership="not_observed", duplicate_reentry="not_observed",
                 failure_rollback="no_write", outcome="failure")


def preparation() -> dict[str, object]:
    return event(stage="deferred_preparation", destination_write="not_observed",
                 reader_prerequisite="not_observed", raw_value_preservation="not_observed",
                 ownership="not_observed", duplicate_reentry="not_observed")


def unsupported(order: int = 2) -> dict[str, object]:
    return rejected(order).copy() | {
        "owner_input_form": "rejected_unsupported",
        "component_input_form": "rejected_unsupported",
    }


def sanitized(*events: dict[str, object]) -> dict[str, object]:
    return observation.sanitize_observation({"format": observation.INPUT_FORMAT, "events": list(events)})


class ParamAnimDeferredReaderRepeatPairTests(unittest.TestCase):
    def test_accepts_identical_complete_contracts(self) -> None:
        first = sanitized(preparation(), event(observation_order=1), rejected(2), unsupported(3))
        result = repeat_pair.sanitize_repeat_pair(
            first, sanitized(preparation(), event(observation_order=1), rejected(2), unsupported(3)))
        self.assertEqual(result["format"], repeat_pair.OUTPUT_FORMAT)
        self.assertEqual(result["events"], first["events"])

    def test_rejects_changed_contract_or_unsanitized_input(self) -> None:
        first = sanitized(preparation(), event(observation_order=1), rejected(2), unsupported(3))
        with self.assertRaises(ValueError):
            repeat_pair.sanitize_repeat_pair(
                first, sanitized(preparation(), event(observation_order=1, ownership="owner_local"), rejected(2), unsupported(3)))
        with self.assertRaises(ValueError):
            repeat_pair.sanitize_repeat_pair(
                {"format": observation.INPUT_FORMAT, "events": [preparation(), event(observation_order=1), rejected(2), unsupported(3)]}, first)

    def test_requires_one_write_and_negative_evidence(self) -> None:
        with self.assertRaises(ValueError):
            repeat_pair.sanitize_repeat_pair(sanitized(event()), sanitized(event()))
        with self.assertRaises(ValueError):
            repeat_pair.sanitize_repeat_pair(
                sanitized(preparation(), event(observation_order=1), event(observation_order=2, stage="component_reader",
                                         destination_write="component_reader",
                                         reader_prerequisite="preparation_and_owner_reader_before_component_reader"),
                          rejected(3), unsupported(4)),
                sanitized(preparation(), event(observation_order=1), event(observation_order=2, stage="component_reader",
                                         destination_write="component_reader",
                                         reader_prerequisite="preparation_and_owner_reader_before_component_reader"),
                          rejected(3), unsupported(4)))

    def test_rejects_retail_or_identity_fields_on_revalidation(self) -> None:
        specimen = sanitized(preparation(), event(observation_order=1), rejected(2), unsupported(3))
        specimen["events"][0]["address"] = 1
        with self.assertRaises(ValueError):
            repeat_pair.sanitize_repeat_pair(
                specimen, sanitized(preparation(), event(observation_order=1), rejected(2), unsupported(3)))


if __name__ == "__main__":
    unittest.main()
