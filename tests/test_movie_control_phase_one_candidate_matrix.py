"""MovieControl candidate matrices must remain aggregate and source-free."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import movie_control_phase_one_candidate_matrix as matrix  # noqa: E402


def record(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "format": matrix.INPUT_FORMAT,
        "target_relation_verified": True,
        "static_mapping_complete": True,
        "runtime_observation_complete": True,
        "dispatcher_candidate_count": 9,
        "candidates_observed": 1,
        "uniquely_observed": True,
    }
    value.update(changes)
    return value


class MovieControlPhaseOneCandidateMatrixTests(unittest.TestCase):
    def test_accepts_a_unique_real_observation_summary(self) -> None:
        result = matrix.sanitize_matrix(record())
        self.assertEqual(result["format"], matrix.OUTPUT_FORMAT)
        self.assertTrue(result["ready_for_trace_collection"])
        self.assertTrue(result["requires_repeat_pair"])

    def test_keeps_incomplete_analysis_pending(self) -> None:
        result = matrix.sanitize_matrix(record(
            static_mapping_complete=False, runtime_observation_complete=False,
            candidates_observed=0, uniquely_observed=False,
        ))
        self.assertFalse(result["ready_for_trace_collection"])

    def test_rejects_incoherent_counts_and_stages(self) -> None:
        for changed in (
            {"dispatcher_candidate_count": 0},
            {"candidates_observed": 2, "uniquely_observed": True},
            {"candidates_observed": 10, "uniquely_observed": False},
            {"static_mapping_complete": False},
            {"runtime_observation_complete": False},
            {"target_relation_verified": False},
        ):
            with self.subTest(changed=changed):
                with self.assertRaises(ValueError):
                    matrix.sanitize_matrix(record(**changed))

    def test_rejects_identity_content_and_executable_fields(self) -> None:
        for field in ("address", "offset", "path", "source_type", "name", "bytes", "callback_id"):
            with self.subTest(field=field):
                specimen = record()
                specimen[field] = 1
                with self.assertRaises(ValueError):
                    matrix.sanitize_matrix(specimen)


if __name__ == "__main__":
    unittest.main()
