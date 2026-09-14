"""The coordinator repeat runner retains only the reviewed contract receipt."""

from __future__ import annotations

import pathlib
import subprocess
import sys
import unittest
from unittest import mock


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import startup_coordinator_repeat_observation_runner as runner  # noqa: E402
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


def success() -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})


def failure() -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(
        phase="failure", callback_ordinal=5, root_selection="rejected",
        camera_view="not_attempted", pass_context="not_attempted", delivery="not_attempted",
        outcome="failure",
    )]})


class StartupCoordinatorRepeatObservationRunnerTests(unittest.TestCase):
    def test_launches_two_separate_fresh_observers_without_streams_or_target(self) -> None:
        with mock.patch.object(runner, "_validate_observer", return_value=pathlib.Path("/private/observer")), \
             mock.patch.object(runner, "_outside_repository", side_effect=lambda item, _label: item), \
             mock.patch.object(runner, "_canonical_plan", return_value=pathlib.Path("/private/plan.json")), \
             mock.patch.object(pathlib.Path, "is_dir", return_value=True), \
             mock.patch.object(pathlib.Path, "is_symlink", return_value=False), \
             mock.patch.object(pathlib.Path, "exists", return_value=False), \
             mock.patch.object(runner, "_run_one", side_effect=[(success(), failure()), (success(), failure())]) as run_one, \
             mock.patch.object(runner.bundle, "_write_new_private_json_no_follow"):
            receipt = runner.execute_observation(
                observer=pathlib.Path("/private/observer"), canonical_plan=pathlib.Path("/private/plan.json"),
                first_workspace=pathlib.Path("/private/one"), second_workspace=pathlib.Path("/private/two"),
                output_directory=pathlib.Path("/private/output"),
            )
        self.assertEqual(len(run_one.call_args_list), 2)
        self.assertNotEqual(run_one.call_args_list[0].kwargs["workspace"], run_one.call_args_list[1].kwargs["workspace"])
        self.assertEqual(receipt["format"], runner.bundle.OUTPUT_FORMAT)

    def test_one_run_uses_only_literal_fresh_isolated_arguments(self) -> None:
        with mock.patch.object(runner, "_make_workspace"), \
             mock.patch.object(runner, "_collect_one", return_value=(success(), failure())):
            invoked: list[tuple[tuple[object, ...], dict[str, object]]] = []
            def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]:
                invoked.append((args, kwargs))
                return subprocess.CompletedProcess(args[0], 0)
            runner._run_one(observer=pathlib.Path("/private/observer"), plan=pathlib.Path("/private/plan.json"),
                            workspace=pathlib.Path("/private/workspace"), timeout_seconds=30, run=fake_run)
        command, kwargs = invoked[0]
        self.assertEqual(command[0][1:3], ("--mode", "fresh-isolated"))
        self.assertNotIn("attach", command[0])
        self.assertNotIn("pid", command[0])
        self.assertNotIn("target", command[0])
        self.assertFalse(kwargs["shell"])
        self.assertEqual(kwargs["stdin"], subprocess.DEVNULL)
        self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)
        self.assertEqual(kwargs["stderr"], subprocess.DEVNULL)

    def test_observer_failure_cleans_its_protocol_records(self) -> None:
        first, second = pathlib.Path("/private/one"), pathlib.Path("/private/two")
        with mock.patch.object(runner, "_make_workspace"), \
             mock.patch.object(runner, "_collect_one", return_value=(success(), failure())), \
             mock.patch.object(runner, "_discard_raw_records") as discard:
            def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]:
                raise subprocess.CalledProcessError(1, args[0])
            with self.assertRaisesRegex(ValueError, "did not complete"):
                runner._run_one(observer=pathlib.Path("/private/observer"), plan=pathlib.Path("/private/plan"),
                                workspace=second, timeout_seconds=30, run=fake_run)
        discard.assert_called_once_with(second)

    def test_private_path_rejects_parent_traversal_and_double_slash(self) -> None:
        with self.assertRaisesRegex(ValueError, "parent traversal"):
            runner._outside_repository(pathlib.Path("/private/../run"), "workspace")
        with self.assertRaisesRegex(ValueError, "double-slash"):
            runner._outside_repository(pathlib.Path("//private/run"), "workspace")


if __name__ == "__main__":
    unittest.main()
