"""The CutSequenceList observer format must not gain retail-content fields."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import cut_sequence_list_phase_one_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0,
        "phase": "phase_one",
        "callback_ordinal": 3,
        "component_is_constructed": True,
        "owner_is_constructed_owner": True,
        "reader_graph_receipt": "complete",
        "component_status_before": 0,
        "component_status_after": 4,
        "owner_status_before": 0,
        "owner_status_after": 4,
        "collection_before": "created",
        "collection_after": "retained",
        "command_window": "open",
        "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


class CutSequenceListPhaseOneTraceTests(unittest.TestCase):
    def test_keeps_only_fixed_structural_schema(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"], [event()])

    def test_rejects_retail_and_executable_fields(self) -> None:
        for forbidden in ("address", "offset", "symbol", "string", "asset", "bytes", "path", "screenshot", "value", "command"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [specimen]})

    def test_rejects_invalid_phase_and_collection_relations(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(phase="construction", reader_graph_receipt="complete")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(collection_before="retained", collection_after="created")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(command_window="accepted")]})


if __name__ == "__main__":
    unittest.main()
