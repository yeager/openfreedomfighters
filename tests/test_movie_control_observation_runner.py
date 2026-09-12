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
             mock.patch.object(runner, "_read_regular_json_path_no_follow", return_value={"format": probe_plan.OUTPUT_FORMAT, "probes": [
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

    def test_collection_rejects_a_raw_record_symlink_without_reading_its_target(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1] / ".test-work" / "runner-symlink"
        root.mkdir(parents=True, exist_ok=True)
        target = root.parent / "runner-symlink-target.json"
        target.write_text('{"retail":"must not be read"}', encoding="utf-8")
        phase = root / runner.PHASE_ONE_RAW_NAME
        phase.symlink_to(target.name)
        (root / runner.DISPATCH_RAW_NAME).write_text("{}", encoding="utf-8")
        with self.assertRaises(ValueError):
            runner._collect(root)
        self.assertTrue(target.exists())
        self.assertFalse(phase.exists())
        self.assertFalse((root / runner.DISPATCH_RAW_NAME).exists())
        target.unlink()
        root.rmdir()

    def test_collection_rejects_an_oversized_regular_record_before_json_decode(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1] / ".test-work" / "runner-oversized"
        root.mkdir(parents=True, exist_ok=True)
        phase = root / runner.PHASE_ONE_RAW_NAME
        phase.write_bytes(b" " * (runner.MAX_RAW_RECORD_BYTES + 1))
        (root / runner.DISPATCH_RAW_NAME).write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "size limit"):
            runner._collect(root)
        self.assertFalse(phase.exists())
        self.assertFalse((root / runner.DISPATCH_RAW_NAME).exists())
        root.rmdir()

    def test_collection_refuses_a_workspace_symlink(self) -> None:
        parent = pathlib.Path(__file__).resolve().parents[1] / ".test-work"
        target = parent / "runner-workspace-target"
        workspace = parent / "runner-workspace-link"
        target.mkdir(parents=True, exist_ok=True)
        workspace.symlink_to(target.name, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "real directory"):
            runner._collect(workspace)
        workspace.unlink()
        target.rmdir()

    def test_runner_rejects_final_plan_symlink_before_resolving_it(self) -> None:
        parent = pathlib.Path(__file__).resolve().parents[1] / ".test-work"
        target = parent / "runner-plan-target.json"
        link = parent / "runner-plan-link.json"
        target.write_text('{"retail":"must not be read"}', encoding="utf-8")
        link.symlink_to(target.name)
        with self.assertRaisesRegex(ValueError, "canonical plan must not be a symlink"):
            runner._outside_repository(link, "canonical plan")
        self.assertTrue(target.exists())
        link.unlink()
        target.unlink()

    def test_plan_reader_rejects_a_final_symlink_without_reading_the_target(self) -> None:
        parent = pathlib.Path(__file__).resolve().parents[1] / ".test-work"
        target = parent / "runner-plan-reader-target.json"
        link = parent / "runner-plan-reader-link.json"
        target.write_text('{"retail":"must not be read"}', encoding="utf-8")
        link.symlink_to(target.name)
        with self.assertRaisesRegex(ValueError, "canonical plan must be a regular private file"):
            runner._read_regular_json_path_no_follow(link, "canonical plan")
        self.assertTrue(target.exists())
        link.unlink()
        target.unlink()


if __name__ == "__main__":
    unittest.main()
