"""The reader-frontier receipt must discard private aggregate measurements."""

from __future__ import annotations

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import intro_reader_frontier_receipt as receipt  # noqa: E402

def frontier(name: str) -> str:
    return (f"reader-frontier-family={name} owners=11 instances=11 deferred-blocks=11 "
            "profiled-blocks=11 unprofiled-blocks=0 distinct-bounded-shapes=1 "
            "largest-bounded-shape=11 repeated-bounded-shape=yes")


TRANSCRIPT = "\n".join(("first-cut-cold-probe=completed",
                          *(frontier(name) for name in ("parameter-animation", "particle-emitter",
                              "film-grain-camera-setup", "lens-flare-control", "lens-flare-lights",
                              "scroll-texture")))) + "\n"


class IntroReaderFrontierReceiptTests(unittest.TestCase):
    def test_discards_all_frontier_measurements(self) -> None:
        result = receipt.receipt_from_transcript(TRANSCRIPT)
        self.assertEqual(result, {"format": receipt.FORMAT, "probe": "first-cut-cold",
                                  "reader_frontier": "observed", "reader_admission": "unchanged",
                                  "playback": "not_started"})
        rendered = str(result)
        for forbidden in ("11", "owners", "bytes", "path", "id"):
            self.assertNotIn(forbidden, rendered)

    def test_requires_completed_probe_and_well_formed_aggregate_line(self) -> None:
        with self.assertRaisesRegex(ValueError, "completed cold probing"):
            receipt.receipt_from_transcript(TRANSCRIPT.replace("first-cut-cold-probe=completed\n", ""))
        with self.assertRaisesRegex(ValueError, "complete aggregate"):
            receipt.receipt_from_transcript(TRANSCRIPT.replace("owners=11", "owners=private"))
        with self.assertRaisesRegex(ValueError, "complete aggregate"):
            receipt.receipt_from_transcript(TRANSCRIPT.replace("scroll-texture", "particle-emitter"))


if __name__ == "__main__":
    unittest.main()
