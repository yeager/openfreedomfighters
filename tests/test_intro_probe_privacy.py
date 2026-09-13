"""Cold intro coverage output must remain aggregate-only."""

from __future__ import annotations

import pathlib
import unittest


MAIN = pathlib.Path(__file__).parents[1] / "src" / "main.cpp"


class IntroProbePrivacyTests(unittest.TestCase):
    def test_reader_coverage_never_emits_source_class_labels(self) -> None:
        source = MAIN.read_text(encoding="utf-8")
        self.assertNotIn("reader-unimplemented-source-type=", source)
        self.assertNotIn("reader-signature-source-type=", source)

    def test_lifecycle_matrix_is_aggregate_only(self) -> None:
        source = MAIN.read_text(encoding="utf-8")
        self.assertIn('\"lifecycle-coverage=\"', source)
        self.assertIn('\"lifecycle-coverage-status=\"', source)
        self.assertNotIn('\"lifecycle-owner=\"', source)
        self.assertNotIn('\"lifecycle-component=\"', source)
        self.assertNotIn('\"lifecycle-reader=\"', source)

    def test_readiness_probe_stops_before_cut_command_session(self) -> None:
        source = MAIN.read_text(encoding="utf-8")
        readiness = source.index("if(readiness_only)")
        command_session = source.index("session->prepare_supported_first_cut_player()")
        self.assertLess(readiness, command_session)
        self.assertIn('"intro-readiness-probe=completed\\n"', source)
        self.assertIn('"intro-readiness-playback=not-started\\n"', source)
        self.assertNotIn('"intro-readiness-source-"', source)


if __name__ == "__main__":
    unittest.main()
