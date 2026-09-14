"""The retail localization launcher must retain only repeat-gated evidence."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import unittest
from unittest import mock


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import retail_localization_lookup_observation_runner as runner  # noqa: E402
from test_retail_localization_lookup_repeat_pair import clean  # noqa: E402


class RetailLocalizationLookupObservationRunnerTests(unittest.TestCase):
    def _empty_workspace(self, name: str) -> pathlib.Path:
        path = pathlib.Path(__file__).resolve().parents[1] / ".test-work" / name
        path.mkdir(parents=True, exist_ok=True)
        for child in path.iterdir():
            child.unlink()
        return path

    def test_launches_two_literal_fresh_isolated_observers_without_target_or_shell(self) -> None:
        with mock.patch.object(runner, "_validate_observer_path", return_value=pathlib.Path("/private/observer")), \
             mock.patch.object(runner, "_outside_repository", side_effect=lambda value, _label: value), \
             mock.patch.object(runner, "_make_workspace"), \
             mock.patch.object(runner, "_collect", return_value={"events": []}):
            invoked: list[tuple[object, object]] = []
            def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]:
                invoked.append((args, kwargs))
                return subprocess.CompletedProcess(args[0], 0)
            runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                       workspace=pathlib.Path("/private/run"), run=fake_run)
        self.assertEqual(len(invoked), 2)
        for raw_name, (args, kwargs) in zip(runner._RAW_NAMES, invoked, strict=True):
            command = args[0]
            self.assertEqual(command[1:3], ("--mode", "fresh-isolated"))
            self.assertEqual(command[3], "--output")
            self.assertTrue(str(command[4]).endswith(raw_name))
            self.assertNotIn("attach", command)
            self.assertNotIn("pid", command)
            self.assertNotIn("target", command)
            self.assertFalse(kwargs["shell"])
            self.assertEqual(kwargs["stdin"], subprocess.DEVNULL)
            self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)
            self.assertEqual(kwargs["stderr"], subprocess.DEVNULL)

    def test_collection_retains_only_sanitized_runs_and_repeat_pair(self) -> None:
        root = self._empty_workspace("retail-localization-runner")
        raw = {"format": "off.retail-localization-lookup.raw/v1", "events": clean()["events"]}
        for name in runner._RAW_NAMES:
            (root / name).write_text(json.dumps(raw), encoding="utf-8")
        pair = runner._collect(root)
        self.assertEqual(pair["format"], "off.retail-localization-lookup-repeat-pair/v1")
        self.assertEqual({child.name for child in root.iterdir()}, {
            runner.FIRST_SANITIZED_NAME, runner.SECOND_SANITIZED_NAME, runner.REPEAT_PAIR_NAME,
        })
        for child in root.iterdir():
            child.unlink()
        root.rmdir()

    def test_collection_rejects_extra_file_and_erases_only_raw_records(self) -> None:
        root = self._empty_workspace("retail-localization-runner-extra")
        for name in runner._RAW_NAMES:
            (root / name).write_text("{}", encoding="utf-8")
        note = root / "operator-note"
        note.write_text("must not be read", encoding="utf-8")
        with self.assertRaises(ValueError):
            runner._collect(root)
        self.assertFalse(any((root / name).exists() for name in runner._RAW_NAMES))
        self.assertTrue(note.exists())
        note.unlink()
        root.rmdir()

    def test_rejects_raw_symlink_without_reading_target(self) -> None:
        root = self._empty_workspace("retail-localization-runner-link")
        target = root.parent / "retail-localization-runner-target.json"
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

    def test_second_observer_failure_discards_protocol_records(self) -> None:
        with mock.patch.object(runner, "_validate_observer_path", return_value=pathlib.Path("/private/observer")), \
             mock.patch.object(runner, "_outside_repository", side_effect=lambda value, _label: value), \
             mock.patch.object(runner, "_make_workspace"), \
             mock.patch.object(runner, "_discard_raw_records") as discard:
            calls = 0
            def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]:
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise subprocess.CalledProcessError(1, args[0])
                return subprocess.CompletedProcess(args[0], 0)
            with self.assertRaisesRegex(ValueError, "did not complete"):
                runner.execute_observation(observer=pathlib.Path("/private/observer"),
                                           workspace=pathlib.Path("/private/run"), run=fake_run)
        self.assertEqual(calls, 2)
        discard.assert_called_once_with(pathlib.Path("/private/run"))

    def test_rejects_symlink_parent_and_double_slash_workspace(self) -> None:
        parent = self._empty_workspace("retail-localization-runner-parent")
        target, link = parent / "target", parent / "link"
        target.mkdir()
        link.symlink_to(target.name, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "must not traverse a symlink"):
            runner._outside_repository(link / "run", "workspace")
        with self.assertRaisesRegex(ValueError, "double-slash"):
            runner._outside_repository(pathlib.Path("//private/run"), "workspace")
        with self.assertRaisesRegex(ValueError, "parent traversal"):
            runner._outside_repository(pathlib.Path("/private/../run"), "workspace")
        link.unlink()
        target.rmdir()
        parent.rmdir()
