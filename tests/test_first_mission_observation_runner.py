"""The first-mission observer launcher must retain only structural evidence."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import unittest
from unittest import mock


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import first_mission_observation_runner as runner  # noqa: E402
import first_mission_observation_trace as trace  # noqa: E402


def record(run_kind: str, events: list[dict[str, object]]) -> dict[str, object]:
    return {
        "format": trace.INPUT_FORMAT, "method_version": 1,
        "verified_data_manifest_fingerprint": "0" * 64, "platform": "windows",
        "architecture": "x86", "input_device": "keyboard_mouse", "run_kind": run_kind,
        "events": events,
    }


def event(order: int, probe: str, boundary: str, state: str, changed: bool) -> dict[str, object]:
    return {"observation_order": order, "probe": probe, "boundary": boundary,
            "state": state, "visible_change": changed}


def baseline() -> dict[str, object]:
    return record("baseline", [event(0, "launch", "handoff", "loading_visible", True),
                                event(1, "idle", "hud", "stable", False)])


def experiment() -> dict[str, object]:
    return record("input_experiment", [
        event(0, "launch", "handoff", "loading_visible", True),
        event(1, "movement", "control", "movement_and_camera", True),
        event(2, "movement", "mission", "loading", False),
    ])


class FirstMissionObservationRunnerTests(unittest.TestCase):
    def test_launch_uses_no_shell_or_target_selector(self) -> None:
        with mock.patch.object(runner, "_validate_observer_path", return_value=pathlib.Path("/private/observer")), \
             mock.patch.object(runner, "_outside_repository", side_effect=lambda value, _label: value), \
             mock.patch.object(runner, "_make_workspace"), \
             mock.patch.object(runner, "_collect", return_value={"baseline_run_count": 2, "experiment_run_count": 1}):
            invoked: list[object] = []
            def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]:
                invoked.extend((args, kwargs))
                return subprocess.CompletedProcess(args[0], 0)
            runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                       workspace=pathlib.Path("/private/run"), run=fake_run)
        command, kwargs = invoked[0][0], invoked[1]
        self.assertIn("fresh-isolated", command)
        self.assertNotIn("attach", command)
        self.assertNotIn("pid", command)
        self.assertFalse(kwargs["shell"])
        self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)
        self.assertEqual(kwargs["stderr"], subprocess.DEVNULL)

    def test_collection_sanitizes_three_records_then_retains_only_safe_forms(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1] / ".test-work" / "first-mission-runner"
        root.mkdir(parents=True, exist_ok=True)
        for name, value in zip(runner._RAW_NAMES, (baseline(), baseline(), experiment()), strict=True):
            (root / name).write_text(json.dumps(value), encoding="utf-8")
        aggregate = runner._collect(root)
        self.assertEqual(aggregate["baseline_run_count"], 2)
        self.assertEqual({child.name for child in root.iterdir()},
                         set(runner._SANITIZED_NAMES) | {runner.BUNDLE_NAME})
        for child in root.iterdir():
            child.unlink()
        root.rmdir()

    def test_collection_rejects_extra_records_and_removes_all_raw_protocol_records(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1] / ".test-work" / "first-mission-runner-extra"
        root.mkdir(parents=True, exist_ok=True)
        for name in runner._RAW_NAMES:
            (root / name).write_text("{}", encoding="utf-8")
        (root / "retail.txt").write_text("forbidden", encoding="utf-8")
        with self.assertRaises(ValueError):
            runner._collect(root)
        self.assertFalse(any((root / name).exists() for name in runner._RAW_NAMES))
        self.assertTrue((root / "retail.txt").exists())
        (root / "retail.txt").unlink()
        root.rmdir()

    def test_collection_rejects_raw_symlink_without_reading_target(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1] / ".test-work" / "first-mission-runner-link"
        root.mkdir(parents=True, exist_ok=True)
        target = root.parent / "first-mission-runner-link-target.json"
        target.write_text('{"retail":"must not be read"}', encoding="utf-8")
        (root / runner.FIRST_BASELINE_RAW_NAME).symlink_to(target.name)
        for name in runner._RAW_NAMES[1:]:
            (root / name).write_text("{}", encoding="utf-8")
        with self.assertRaises(ValueError):
            runner._collect(root)
        self.assertTrue(target.exists())
        self.assertFalse((root / runner.FIRST_BASELINE_RAW_NAME).exists())
        target.unlink()
        root.rmdir()

    def test_invalid_deadline_is_rejected_before_start(self) -> None:
        with self.assertRaisesRegex(ValueError, "timeout"):
            runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                       workspace=pathlib.Path("/private/run"), timeout_seconds=0)


if __name__ == "__main__":
    unittest.main()
