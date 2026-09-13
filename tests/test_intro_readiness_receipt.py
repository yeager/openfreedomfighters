"""The intro-readiness receipt retains only aggregate cold-probe evidence."""

from __future__ import annotations

import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import intro_readiness_receipt as receipt  # noqa: E402


TEST_WORKSPACE = pathlib.Path.home() / ".cache" / "openfreedomfighters-test"

TRANSCRIPT = """intro-readiness-probe=completed
intro-readiness-reader-bracket=complete
reader-coverage-discovered=420
reader-coverage-recognized=109
reader-coverage-applied=104
reader-coverage-unapplied=316
lifecycle-coverage=readers required=420 covered=104 uncovered=316
lifecycle-coverage=components required=383 covered=0 uncovered=383
lifecycle-coverage=owners required=471 covered=0 uncovered=471
lifecycle-coverage-status=reader-coverage
intro-readiness-lifecycle=not-admitted
intro-readiness-renderer=not-created
intro-readiness-audio=not-started
intro-readiness-playback=not-started
"""


class IntroReadinessReceiptTests(unittest.TestCase):
    def test_rejects_a_parent_symlink_before_resolution(self) -> None:
        TEST_WORKSPACE.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=TEST_WORKSPACE) as directory:
            private = pathlib.Path(directory)
            target = private / "target"
            target.mkdir()
            alias = private / "alias"
            alias.symlink_to(target.name, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "must not traverse a symlink"):
                receipt._outside_repository(alias / "transcript.txt", "transcript")

    def test_retains_aggregate_coverage_and_all_cold_statuses(self) -> None:
        result = receipt.receipt_from_transcript(
            TRANSCRIPT + "retail-source-label=must-not-survive\n")
        self.assertEqual(result["format"], receipt.FORMAT)
        self.assertEqual(result["probe"], "intro-readiness")
        self.assertEqual(result["reader_coverage"],
                         {"discovered": 420, "recognized": 109, "applied": 104, "unapplied": 316})
        self.assertEqual(result["lifecycle_coverage"]["owners"],
                         {"required": 471, "covered": 0, "uncovered": 471})
        self.assertEqual({key: result[key] for key in ("lifecycle", "renderer", "audio", "playback")},
                         {"lifecycle": "not_admitted", "renderer": "not_created",
                          "audio": "not_started", "playback": "not_started"})
        rendered = str(result)
        for forbidden in ("path", "offset", "handle", "payload", "source-label", "must-not-survive"):
            self.assertNotIn(forbidden, rendered)

    def test_rejects_incomplete_or_inconsistent_evidence(self) -> None:
        cases = (
            (TRANSCRIPT.replace("intro-readiness-probe=completed\n", ""), "completed intro readiness"),
            (TRANSCRIPT.replace("reader-coverage-applied=104", "reader-coverage-applied=110"),
             "inconsistent reader"),
            (TRANSCRIPT.replace("uncovered=383", "uncovered=1"), "inconsistent lifecycle"),
            (TRANSCRIPT.replace("lifecycle-coverage-status=reader-coverage",
                                "lifecycle-coverage-status=complete"), "disagrees"),
            (TRANSCRIPT.replace("intro-readiness-audio=not-started\n", ""), "cold statuses"),
            (TRANSCRIPT + "reader-coverage-applied=104\n", "duplicate reader coverage"),
        )
        for transcript, message in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(ValueError, message):
                    receipt.receipt_from_transcript(transcript)


if __name__ == "__main__":
    unittest.main()
