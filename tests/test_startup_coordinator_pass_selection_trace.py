"""Startup coordinator pass-selection traces stay structural and fail closed."""

from __future__ import annotations

import pathlib
import tempfile
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import startup_coordinator_pass_selection_trace as trace  # noqa: E402

TEST_WORKSPACE = pathlib.Path(__file__).resolve().parents[1] / ".test-work"


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0,
        "phase": "completion",
        "callback_ordinal": 4,
        "coordinator": "entered",
        "root_selection": "selected",
        "camera_view": "enabled",
        "pass_context": "resolved",
        "delivery": "delivered",
        "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


class StartupCoordinatorPassSelectionTraceTests(unittest.TestCase):
    def test_keeps_a_complete_structural_selection(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"], [event()])

    def test_keeps_a_structural_failure(self) -> None:
        record = event(
            phase="failure", root_selection="rejected", camera_view="not_attempted",
            pass_context="not_attempted", delivery="not_attempted", outcome="failure",
        )
        self.assertEqual(trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [record]})["events"], [record])

    def test_rejects_raw_retail_or_executable_fields(self) -> None:
        for forbidden in ("address", "offset", "symbol", "string", "path", "asset", "bytes", "image", "time", "identity"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [specimen]})

    def test_rejects_incomplete_or_reordered_selection(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(camera_view="disabled")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(pass_context="not_attempted")]})
        first = event(
            observation_order=0, phase="root_selection", root_selection="selected",
            camera_view="not_attempted", pass_context="not_attempted", delivery="not_attempted",
            outcome="success",
        )
        second = event(observation_order=1, phase="camera_view")
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [first, second]})

    def test_rejects_later_claims_at_entry_and_unstructured_failure(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(
                phase="coordinator_entry", root_selection="selected", camera_view="enabled",
                pass_context="resolved", delivery="delivered",
            )]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(
                phase="failure", root_selection="selected", camera_view="enabled",
                pass_context="resolved", delivery="not_attempted", outcome="failure",
            )]})

    def test_requires_one_observer_local_callback_ordinal(self) -> None:
        first = event(observation_order=0, phase="root_selection", root_selection="selected",
                      camera_view="not_attempted", pass_context="not_attempted", delivery="not_attempted")
        second = event(observation_order=1, phase="completion", callback_ordinal=5)
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [first, second]})

    def test_private_io_refuses_symlinks_and_existing_output(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEST_WORKSPACE) as directory:
            root = pathlib.Path(directory)
            source = root / "source.json"
            source.write_text("{}", encoding="utf-8")
            link = root / "input-link.json"
            link.symlink_to(source)
            with self.assertRaises(ValueError):
                trace._outside_repository(link, "input")
            output = root / "result.json"
            output.write_text("existing", encoding="utf-8")
            with self.assertRaises(ValueError):
                trace._write_new_private_json_no_follow(output, {})


if __name__ == "__main__":
    unittest.main()
