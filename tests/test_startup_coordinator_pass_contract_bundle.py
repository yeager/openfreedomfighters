"""The native coordinator receipt is assembled only from reviewed traces."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import startup_coordinator_pass_contract_bundle as bundle  # noqa: E402
import startup_coordinator_pass_selection_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0, "phase": "completion", "callback_ordinal": 4,
        "coordinator": "entered", "root_selection": "selected", "camera_view": "enabled",
        "pass_context": "resolved", "delivery": "delivered", "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


def sanitized(*events: dict[str, object]) -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": list(events)})


def rejected(callback: int = 5) -> dict[str, object]:
    return sanitized(event(phase="failure", callback_ordinal=callback, root_selection="rejected",
                           camera_view="not_attempted", pass_context="not_attempted",
                           delivery="not_attempted", outcome="failure"))


class StartupCoordinatorPassContractBundleTests(unittest.TestCase):
    def test_exact_repeated_success_and_separate_rejection_builds_native_receipt(self) -> None:
        completed = sanitized(event())
        receipt = bundle.bundle_contract(completed, completed, rejected())
        self.assertEqual(receipt["format"], bundle.OUTPUT_FORMAT)
        self.assertEqual(set(receipt), {"format", "candidate", "repeat", "failure"})
        self.assertEqual(receipt["candidate"], receipt["repeat"])
        self.assertEqual(set(receipt["candidate"]), {
            "pass_order", "coordinator_constructed", "manager_constructed", "pass_entered",
            "work_list_ready", "selected_work", "work_admission", "pass_completed", "outcome",
            "external_service",
        })
        self.assertEqual(receipt["candidate"]["work_admission"], "admitted")
        self.assertEqual(receipt["failure"]["work_admission"], "rejected")

    def test_raw_different_success_or_nonterminal_rejection_is_refused(self) -> None:
        completed = sanitized(event())
        raw = {"format": trace.INPUT_FORMAT, "events": [event()]}
        with self.assertRaises(ValueError):
            bundle.bundle_contract(raw, completed, rejected())
        with self.assertRaises(ValueError):
            bundle.bundle_contract(completed, sanitized(event(callback_ordinal=6)), rejected())
        nonterminal_failure = sanitized(event(phase="root_selection", root_selection="candidate",
                                              camera_view="not_attempted", pass_context="not_attempted",
                                              delivery="not_attempted", outcome="success"))
        with self.assertRaises(ValueError):
            bundle.bundle_contract(completed, completed, nonterminal_failure)

    def test_callbacks_must_separate_positive_and_negative_evidence(self) -> None:
        completed = sanitized(event())
        with self.assertRaisesRegex(ValueError, "distinct"):
            bundle.bundle_contract(completed, completed, rejected(callback=4))


if __name__ == "__main__":
    unittest.main()
