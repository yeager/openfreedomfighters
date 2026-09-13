"""The ParamAnim observer launcher must retain only repeat-gated evidence."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import unittest
from unittest import mock


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import paramanim_deferred_reader_observation_runner as runner  # noqa: E402
from test_paramanim_deferred_reader_repeat_pair import complete  # noqa: E402


class ParamAnimDeferredReaderObservationRunnerTests(unittest.TestCase):
    def _empty_workspace(self, name: str) -> pathlib.Path:
        path = pathlib.Path(__file__).resolve().parents[1] / ".test-work" / name
        path.mkdir(parents=True, exist_ok=True)
        for child in path.iterdir():
            child.unlink()
        return path

    def test_launch_uses_literal_fresh_isolated_without_target_or_shell(self) -> None:
        with mock.patch.object(runner, "_validate_observer_path", return_value=pathlib.Path("/private/observer")), \
             mock.patch.object(runner, "_outside_repository", side_effect=lambda value, _label: value), \
             mock.patch.object(runner, "_make_workspace"), \
             mock.patch.object(runner, "_collect", return_value={"events": []}):
            invoked: list[object] = []
            def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]:
                invoked.extend((args, kwargs))
                return subprocess.CompletedProcess(args[0], 0)
            runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                       workspace=pathlib.Path("/private/run"), run=fake_run)
        command, kwargs = invoked[0][0], invoked[1]
        self.assertEqual(command[1:3], ("--mode", "fresh-isolated"))
        self.assertNotIn("attach", command)
        self.assertNotIn("pid", command)
        self.assertNotIn("target", command)
        self.assertFalse(kwargs["shell"])
        self.assertEqual(kwargs["stdin"], subprocess.DEVNULL)
        self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)
        self.assertEqual(kwargs["stderr"], subprocess.DEVNULL)

    def test_collection_retains_only_sanitized_runs_and_repeat_pair(self) -> None:
        root = self._empty_workspace("paramanim-runner")
        for name in runner._RAW_NAMES:
            (root / name).write_text(json.dumps({
                "format": "off.paramanim-deferred-reader-observation.raw/v1",
                "events": complete()["events"],
            }), encoding="utf-8")
        pair = runner._collect(root)
        self.assertEqual(pair["format"], "off.paramanim-deferred-reader-repeat-pair/v1")
        self.assertEqual({child.name for child in root.iterdir()}, {
            runner.FIRST_SANITIZED_NAME, runner.SECOND_SANITIZED_NAME, runner.REPEAT_PAIR_NAME,
        })
        for child in root.iterdir():
            child.unlink()
        root.rmdir()

    def test_collection_rejects_extra_file_and_erases_both_raw_records(self) -> None:
        root = self._empty_workspace("paramanim-runner-extra")
        for name in runner._RAW_NAMES:
            (root / name).write_text("{}", encoding="utf-8")
        marker = root / "retail.txt"
        marker.write_text("must not be read", encoding="utf-8")
        with self.assertRaises(ValueError):
            runner._collect(root)
        self.assertFalse(any((root / name).exists() for name in runner._RAW_NAMES))
        self.assertTrue(marker.exists())
        marker.unlink()
        root.rmdir()

    def test_collection_rejects_raw_symlink_without_reading_target(self) -> None:
        root = self._empty_workspace("paramanim-runner-link")
        target = root.parent / "paramanim-runner-link-target.json"
        target.write_text('{"retail":"must not be read"}', encoding="utf-8")
        (root / runner.FIRST_RAW_NAME).symlink_to(target.name)
        (root / runner.SECOND_RAW_NAME).write_text("{}", encoding="utf-8")
        with self.assertRaises(ValueError):
            runner._collect(root)
        self.assertTrue(target.exists())
        self.assertFalse((root / runner.FIRST_RAW_NAME).exists())
        self.assertFalse((root / runner.SECOND_RAW_NAME).exists())
        target.unlink()
        root.rmdir()

    def test_failure_cleanup_removes_only_protocol_raw_records(self) -> None:
        root = self._empty_workspace("paramanim-runner-cleanup")
        for name in runner._RAW_NAMES:
            (root / name).write_text("{}", encoding="utf-8")
        marker = root / "operator-note"
        marker.write_text("keep", encoding="utf-8")
        runner._discard_raw_records(root)
        self.assertFalse(any((root / name).exists() for name in runner._RAW_NAMES))
        self.assertTrue(marker.exists())
        marker.unlink()
        root.rmdir()

    def test_rejects_invalid_timeout_before_observer_start(self) -> None:
        with self.assertRaisesRegex(ValueError, "timeout"):
            runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                       workspace=pathlib.Path("/private/run"), timeout_seconds=0)

    def test_rejects_symlinked_workspace_parent_before_resolution(self) -> None:
        parent = self._empty_workspace("paramanim-runner-parent")
        target = parent / "target"
        link = parent / "link"
        target.mkdir()
        link.symlink_to(target.name, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "must not traverse a symlink"):
            runner._outside_repository(link / "run", "workspace")
        link.unlink()
        target.rmdir()
        parent.rmdir()

    def test_rejects_double_slash_workspace_path(self) -> None:
        with self.assertRaisesRegex(ValueError, "double-slash"):
            runner._outside_repository(pathlib.Path("//private/run"), "workspace")


if __name__ == "__main__":
    unittest.main()
