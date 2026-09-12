"""Repeated coordinator observations must be exact, terminal and sanitized."""

from __future__ import annotations

import pathlib
import tempfile
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import startup_coordinator_pass_selection_repeat_pair as pair  # noqa: E402
import startup_coordinator_pass_selection_trace as trace  # noqa: E402

TEST_WORKSPACE = pathlib.Path(__file__).resolve().parents[1] / ".test-work"


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


class StartupCoordinatorPassSelectionRepeatPairTests(unittest.TestCase):
    def test_accepts_an_identical_terminal_pair(self) -> None:
        observation = sanitized(event())
        result = pair.sanitize_repeat_pair(observation, observation)
        self.assertEqual(result["format"], pair.OUTPUT_FORMAT)
        self.assertEqual(result["events"], [event()])

    def test_rejects_different_or_nonterminal_pair(self) -> None:
        first = sanitized(event())
        changed = sanitized(event(callback_ordinal=5))
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(first, changed)
        nonterminal = sanitized(event(
            phase="pass_context", delivery="not_attempted", pass_context="resolved",
        ))
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(nonterminal, nonterminal)

    def test_rejects_raw_or_extra_fields(self) -> None:
        raw = {"format": trace.INPUT_FORMAT, "events": [event()]}
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(raw, raw)
        malformed = sanitized(event())
        malformed["raw"] = "forbidden"
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(malformed, malformed)

    def test_private_io_refuses_symlinks_and_existing_output(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEST_WORKSPACE) as directory:
            root = pathlib.Path(directory)
            source = root / "source.json"
            source.write_text("{}", encoding="utf-8")
            link = root / "input-link.json"
            link.symlink_to(source)
            with self.assertRaises(ValueError):
                pair._outside_repository(link, "first input")
            output = root / "result.json"
            output.write_text("existing", encoding="utf-8")
            with self.assertRaises(ValueError):
                pair._write_new_private_json_no_follow(output, {})


if __name__ == "__main__":
    unittest.main()
