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


if __name__ == "__main__":
    unittest.main()
