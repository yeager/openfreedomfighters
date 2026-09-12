"""The startup coordinator private runner has no data-export boundary."""

from __future__ import annotations

import pathlib
import subprocess
import sys
import unittest
from unittest import mock


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import startup_coordinator_observation_runner as runner  # noqa: E402
import startup_coordinator_observer_probe_plan as probe_plan  # noqa: E402
import startup_coordinator_pass_selection_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0, "phase": "completion", "callback_ordinal": 4,
        "coordinator": "entered", "root_selection": "selected", "camera_view": "enabled",
        "pass_context": "resolved", "delivery": "delivered", "outcome": "success",
        "external_service": "entered",
    }
    value.update(changes)
    return value


def successful() -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})


def rejected() -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(
        phase="failure", callback_ordinal=5, root_selection="rejected",
        camera_view="not_attempted", pass_context="not_attempted", delivery="not_attempted",
        outcome="failure",
    )]})


class StartupCoordinatorObservationRunnerTests(unittest.TestCase):
    def test_canonical_plan_is_fixed_and_opaque(self) -> None:
        canonical = {"format": probe_plan.OUTPUT_FORMAT, "probes": [
            {"slot": slot, "point": point}
            for slot, point in enumerate(probe_plan.PROTOCOL_POINTS)
        ]}
        self.assertEqual(runner._canonical_probe_plan(canonical), canonical)
        canonical["address"] = 1
        with self.assertRaises(ValueError):
            runner._canonical_probe_plan(canonical)

    def test_launch_has_no_shell_output_or_target_selector(self) -> None:
        plan = {"format": probe_plan.OUTPUT_FORMAT, "probes": [
            {"slot": slot, "point": point}
            for slot, point in enumerate(probe_plan.PROTOCOL_POINTS)
        ]}
        with mock.patch.object(runner, "_validate_observer_path", return_value=pathlib.Path("/private/observer")), \
             mock.patch.object(runner, "_outside_repository", side_effect=lambda item, _label: item), \
             mock.patch.object(runner, "_read_regular_json_path_no_follow", return_value=plan), \
             mock.patch.object(pathlib.Path, "is_file", return_value=True), \
             mock.patch.object(runner, "_make_workspace"), \
             mock.patch.object(runner, "_collect", return_value=(successful(), rejected())):
            invoked: list[object] = []
            def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]:
                invoked.extend((args, kwargs))
                return subprocess.CompletedProcess(args[0], 0)
            runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                       canonical_plan=pathlib.Path("/private/plan.json"),
                                       workspace=pathlib.Path("/private/workspace"), run=fake_run)
        command, kwargs = invoked[0][0], invoked[1]
        self.assertIn("fresh-isolated", command)
        self.assertNotIn("attach", command)
        self.assertFalse(kwargs["shell"])
        self.assertEqual(kwargs["stdin"], subprocess.DEVNULL)
        self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)
        self.assertEqual(kwargs["stderr"], subprocess.DEVNULL)
        self.assertEqual(kwargs["timeout"], runner.DEFAULT_TIMEOUT_SECONDS)

    def test_relation_requires_one_success_one_failure_and_distinct_callbacks(self) -> None:
        runner._validate_relation(successful(), rejected())
        same_callback = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(
            phase="failure", root_selection="rejected", camera_view="not_attempted",
            pass_context="not_attempted", delivery="not_attempted", outcome="failure",
        )]})
        with self.assertRaisesRegex(ValueError, "distinct"):
            runner._validate_relation(successful(), same_callback)

    def test_rejects_invalid_timeout_before_observer_start(self) -> None:
        with self.assertRaisesRegex(ValueError, "timeout"):
            runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                       canonical_plan=pathlib.Path("/private/plan.json"),
                                       workspace=pathlib.Path("/private/workspace"), timeout_seconds=0)

    def test_collection_deletes_raw_records_when_extra_file_is_present(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1] / ".test-work" / "coordinator-runner-extra"
        root.mkdir(parents=True, exist_ok=True)
        (root / runner.SUCCESS_RAW_NAME).write_text("{}", encoding="utf-8")
        (root / runner.FAILURE_RAW_NAME).write_text("{}", encoding="utf-8")
        marker = root / "forbidden-export"
        marker.write_text("keep", encoding="utf-8")
        with self.assertRaises(ValueError):
            runner._collect(root)
        self.assertFalse((root / runner.SUCCESS_RAW_NAME).exists())
        self.assertFalse((root / runner.FAILURE_RAW_NAME).exists())
        self.assertTrue(marker.exists())
        marker.unlink()
        root.rmdir()


if __name__ == "__main__":
    unittest.main()
