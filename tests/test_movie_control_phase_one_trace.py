"""The MovieControl observer format must not gain retail-content fields."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import movie_control_phase_one_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "dispatch_order": 0,
        "phase": 1,
        "callback_ordinal": 3,
        "component_is_constructed": True,
        "owner_is_constructed_owner": True,
        "component_status_before": 0,
        "component_status_after": 4,
        "owner_status_before": 0,
        "owner_status_after": 4,
        "event_member_before": False,
        "event_member_after": True,
        "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


class MovieControlPhaseOneTraceTests(unittest.TestCase):
    def test_keeps_only_fixed_structural_schema(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"], [event()])

    def test_rejects_retail_and_executable_fields(self) -> None:
        for forbidden in ("address", "offset", "symbol", "string", "asset", "bytes", "path", "screenshot"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [specimen]})

    def test_rejects_non_phase_one_and_non_monotonic_dispatch(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(phase=2)]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(dispatch_order=1), event(dispatch_order=1)]})


if __name__ == "__main__":
    unittest.main()
