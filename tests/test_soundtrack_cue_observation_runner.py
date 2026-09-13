"""The private soundtrack observer launcher must not select or expose a target."""

from __future__ import annotations

import pathlib
import subprocess
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import soundtrack_cue_observation_runner as runner  # noqa: E402


class SoundtrackCueObservationRunnerTests(unittest.TestCase):
    def test_only_launches_a_fresh_isolated_observer_without_shell(self) -> None:
        with mock.patch.object(runner, "_outside", side_effect=lambda value, _label: value), \
             mock.patch.object(pathlib.Path, "is_file", return_value=True), \
             mock.patch.object(pathlib.Path, "is_symlink", return_value=False), \
             mock.patch.object(pathlib.Path, "exists", return_value=False), \
             mock.patch.object(pathlib.Path, "mkdir"), \
             mock.patch.object(runner.os, "access", return_value=True), \
             mock.patch.object(runner, "_collect", return_value={"bindings": []}):
            calls: list[object] = []
            def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]:
                calls.extend((args, kwargs))
                return subprocess.CompletedProcess(args[0], 0)
            runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                       workspace=pathlib.Path("/private/work"), run=fake_run)
        command, kwargs = calls[0][0], calls[1]
        self.assertIn("fresh-isolated", command)
        self.assertNotIn("attach", command)
        self.assertNotIn("pid", command)
        self.assertFalse(kwargs["shell"])
        self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)

    def test_rejects_invalid_timeout_before_launch(self) -> None:
        with self.assertRaisesRegex(ValueError, "timeout"):
            runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                       workspace=pathlib.Path("/private/work"), timeout_seconds=0)


if __name__ == "__main__":
    unittest.main()
