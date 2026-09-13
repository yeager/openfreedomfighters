"""The menu scene-request repeat gate must be exact and fail closed."""

from __future__ import annotations

import json
import pathlib
import stat
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
TEST_WORKSPACE = pathlib.Path.home() / ".cache" / "openfreedomfighters-test"
sys.path.insert(0, str(ROOT / "tools"))
import menu_scene_request_repeat_pair as pair  # noqa: E402
import menu_scene_request_trace as trace  # noqa: E402
import private_structural_json as structural_json  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0,
        "phase": "completion",
        "callback_ordinal": 4,
        "menu_component_constructed": True,
        "reader_graph_receipt": "complete",
        "component_status_before": 4,
        "component_status_after": 4,
        "owner_status_before": 4,
        "owner_status_after": 4,
        "selection": "delivered",
        "active_window": "replaced",
        "active_window_selection_delivery": "delivered",
        "receiver_route": "scene_request",
        "receiver_manager_edge": "entered",
        "manager_request": "clear_then_request",
        "request_target": "validated",
        "package_admission": "not_entered",
        "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


def sanitized(**changes: object) -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(**changes)]})


class MenuSceneRequestRepeatPairTests(unittest.TestCase):
    def test_accepts_complete_identical_chain_with_or_without_package_admission(self) -> None:
        no_package = sanitized()
        result = pair.sanitize_repeat_pair(no_package, no_package)
        self.assertEqual(result, {"format": pair.OUTPUT_FORMAT, "events": [event()]})
        admitted = sanitized(package_admission="admitted")
        self.assertEqual(pair.sanitize_repeat_pair(admitted, admitted)["events"], [
            event(package_admission="admitted")
        ])

    def test_rejects_partial_or_nonterminal_or_different_chains(self) -> None:
        complete = sanitized()
        with self.assertRaisesRegex(ValueError, "disagree"):
            pair.sanitize_repeat_pair(complete, sanitized(callback_ordinal=5))
        with self.assertRaisesRegex(ValueError, "terminal complete"):
            pair.sanitize_repeat_pair(
                sanitized(phase="scene_manager"), sanitized(phase="scene_manager"))
        with self.assertRaisesRegex(ValueError, "terminal complete"):
            pair.sanitize_repeat_pair(
                sanitized(package_admission="candidate"), sanitized(package_admission="candidate"))

    def test_rejects_raw_or_extra_schema(self) -> None:
        raw = {"format": trace.INPUT_FORMAT, "events": [event()]}
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(raw, raw)
        malformed = sanitized()
        malformed["unexpected"] = "forbidden"
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(malformed, malformed)

    def test_private_inputs_need_identical_bytes_and_new_mode_0600_output(self) -> None:
        TEST_WORKSPACE.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=TEST_WORKSPACE) as directory:
            private = pathlib.Path(directory)
            record = sanitized()
            first = private / "first.json"
            second = private / "second.json"
            first.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
            second.write_text(json.dumps(record, separators=(",", ":")) + "\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "byte-identical"):
                pair._read_identical_private_traces(first, second)
            second.write_bytes(first.read_bytes())
            self.assertEqual(pair._read_identical_private_traces(first, second), (record, record))
            output = private / "receipt.json"
            structural_json.write_new_json(output, {"safe": True}, "output")
            self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o600)
            with self.assertRaises(ValueError):
                structural_json.write_new_json(output, {"safe": True}, "output")

    def test_rejects_parent_symlink_before_reading(self) -> None:
        TEST_WORKSPACE.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=TEST_WORKSPACE) as directory:
            private = pathlib.Path(directory)
            target = private / "target"
            target.mkdir()
            alias = private / "alias"
            alias.symlink_to(target.name, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "must not traverse a symlink"):
                structural_json.outside_repository(alias / "record.json", ROOT, "input")


if __name__ == "__main__":
    unittest.main()
