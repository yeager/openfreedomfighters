"""The launch-time intro-audio access boundary must remain source-free."""
from __future__ import annotations

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import intro_audio_launch_access_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {"observation_order": 0, "intro_stream_ordinal": 1,
                                "bank": "local", "access_result": "opened"}
    value.update(changes)
    return value


class IntroAudioLaunchAccessTraceTests(unittest.TestCase):
    def test_accepts_only_a_fresh_structural_record(self) -> None:
        raw = {"format": trace.INPUT_FORMAT, "fresh_isolated": True, "events": [event()]}
        self.assertEqual(trace.sanitize_trace(raw),
                         {"format": trace.OUTPUT_FORMAT, "fresh_isolated": True, "events": [event()]})

    def test_rejects_source_and_executable_details(self) -> None:
        for forbidden in ("path", "name", "hash", "bytes", "address", "offset", "pid", "time", "string"):
            with self.subTest(forbidden=forbidden):
                specimen = event(); specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "fresh_isolated": True, "events": [specimen]})

    def test_requires_order_and_deduplicates_exact_access_results(self) -> None:
        second = event(observation_order=1, intro_stream_ordinal=2, bank="global", access_result="missing")
        self.assertEqual(len(trace.sanitize_trace({"format": trace.INPUT_FORMAT, "fresh_isolated": True, "events": [event(), second]})["events"]), 2)
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "fresh_isolated": False, "events": [event()]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "fresh_isolated": True, "events": [event(), event(observation_order=1)]})


if __name__ == "__main__":
    unittest.main()
