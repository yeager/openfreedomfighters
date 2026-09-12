"""The private MovieControl observation launcher must not widen its boundary."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import unittest
from unittest import mock


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import movie_control_observation_runner as runner  # noqa: E402
import movie_control_observer_probe_plan as probe_plan  # noqa: E402


class MovieControlObservationRunnerTests(unittest.TestCase):
    def test_canonical_plan_accepts_only_the_fixed_opaque_protocol(self) -> None:
        canonical = {"format": probe_plan.OUTPUT_FORMAT, "probes": [
            {"slot": slot, "point": point}
            for slot, point in enumerate(probe_plan.PROTOCOL_POINTS)
        ]}
        self.assertEqual(runner._canonical_probe_plan(canonical), canonical)
        canonical["address"] = 123
        with self.assertRaises(ValueError):
            runner._canonical_probe_plan(canonical)

    def test_launch_uses_no_shell_or_attach_selector(self) -> None:
        with mock.patch.object(runner, "_validate_observer_path", return_value=pathlib.Path("/private/observer")), \
             mock.patch.object(runner, "_outside_repository", side_effect=lambda value, _label: value), \
             mock.patch.object(runner, "_read_json", return_value={"format": probe_plan.OUTPUT_FORMAT, "probes": [
                 {"slot": slot, "point": point}
                 for slot, point in enumerate(probe_plan.PROTOCOL_POINTS)
             ]}), \
             mock.patch.object(pathlib.Path, "is_file", return_value=True), \
             mock.patch.object(pathlib.Path, "is_symlink", return_value=False), \
             mock.patch.object(runner, "_make_workspace"), \
             mock.patch.object(runner, "_collect", return_value=({"events": []}, {"events": []})):
            invoked: list[object] = []
            def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]:
                invoked.extend((args, kwargs))
                return subprocess.CompletedProcess(args[0], 0)
            runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                       canonical_plan=pathlib.Path("/private/plan.json"),
                                       workspace=pathlib.Path("/private/run"), run=fake_run)
        command, kwargs = invoked[0][0], invoked[1]
        self.assertIn("fresh-isolated", command)
        self.assertNotIn("attach", command)
        self.assertFalse(kwargs["shell"])
        self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)
        self.assertEqual(kwargs["stderr"], subprocess.DEVNULL)

    def test_collection_rejects_extra_or_content_bearing_records(self) -> None:
        # Schema validation itself is covered by the dedicated trace tests; the
        # runner must not accept an arbitrary third file as an export channel.
        root = pathlib.Path(__file__).resolve().parents[1] / ".test-work" / "runner-extra"
        if root.exists():
            for child in root.iterdir():
                child.unlink()
        else:
            root.mkdir(parents=True)
        (root / runner.PHASE_ONE_RAW_NAME).write_text("{}", encoding="utf-8")
        (root / runner.DISPATCH_RAW_NAME).write_text("{}", encoding="utf-8")
        (root / "retail.txt").write_text("forbidden", encoding="utf-8")
        with self.assertRaises(ValueError):
            runner._collect(root)
        self.assertFalse((root / runner.PHASE_ONE_RAW_NAME).exists())
        self.assertFalse((root / runner.DISPATCH_RAW_NAME).exists())
        for child in root.iterdir():
            child.unlink()
        root.rmdir()


if __name__ == "__main__":
    unittest.main()
