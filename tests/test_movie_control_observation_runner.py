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
import movie_control_cutscene_dispatch_trace as dispatch_trace  # noqa: E402
import movie_control_observer_probe_plan as probe_plan  # noqa: E402
import movie_control_phase_one_trace as phase_one_trace  # noqa: E402


def phase_one_event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "dispatch_order": 4, "phase": 1, "callback_ordinal": 3,
        "component_is_constructed": True, "owner_is_constructed_owner": True,
        "component_status_before": 0, "component_status_after": 4,
        "owner_status_before": 0, "owner_status_after": 4,
        "event_member_before": False, "event_member_after": True,
        "outcome": "success", "external_service": "entered",
        "global_lifecycle_entered": True, "global_lifecycle_completed": True,
        "global_lifecycle_outcome": "success", "ordinary_member_before": False,
        "ordinary_member_after": True, "phase_one_completed": True,
    }
    value.update(changes)
    return value


def dispatch_event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0, "phase": "player_activation", "callback_ordinal": 3,
        "movie_component_is_constructed": True,
        "movie_owner_is_constructed_owner": True,
        "sequence_component_is_constructed": True,
        "sequence_owner_is_constructed_owner": True,
        "movie_phase_one_completed": True, "movie_phase_two_completed": True,
        "component_status_before": 4, "component_status_after": 4,
        "owner_status_before": 0, "owner_status_after": 0,
        "event16_gate": "admitted", "handoff_sender_is_movie_owner": True,
        "handoff_target_is_sequence_owner": True, "handoff": "delivered",
        "delivery_mode": "synchronous", "player_activation": "started",
        "outcome": "success", "external_service": "entered",
    }
    value.update(changes)
    return value


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
        self.assertEqual(kwargs["timeout"], runner.DEFAULT_TIMEOUT_SECONDS)

    def test_collected_records_must_share_one_completed_callback_relation(self) -> None:
        phase = phase_one_trace.sanitize_trace({
            "format": phase_one_trace.INPUT_FORMAT, "events": [phase_one_event()],
        })
        dispatch = dispatch_trace.sanitize_trace({
            "format": dispatch_trace.INPUT_FORMAT, "events": [dispatch_event()],
        })
        runner._validate_trace_relation(phase, dispatch)
        mismatched = dispatch_trace.sanitize_trace({
            "format": dispatch_trace.INPUT_FORMAT,
            "events": [dispatch_event(callback_ordinal=4)],
        })
        with self.assertRaisesRegex(ValueError, "not tied"):
            runner._validate_trace_relation(phase, mismatched)

    def test_collection_rejects_unanchored_or_ambiguous_phase_one_records(self) -> None:
        dispatch = dispatch_trace.sanitize_trace({
            "format": dispatch_trace.INPUT_FORMAT, "events": [dispatch_event()],
        })
        failed = phase_one_trace.sanitize_trace({
            "format": phase_one_trace.INPUT_FORMAT,
            "events": [phase_one_event(
                outcome="failure", phase_one_completed=False,
                global_lifecycle_completed=False, global_lifecycle_outcome="failure",
            )],
        })
        with self.assertRaisesRegex(ValueError, "exactly one"):
            runner._validate_trace_relation(failed, dispatch)

    def test_invalid_observer_deadline_is_rejected_before_start(self) -> None:
        with self.assertRaisesRegex(ValueError, "timeout"):
            runner.execute_observation(
                observer=pathlib.Path("/private/observer"),
                canonical_plan=pathlib.Path("/private/plan.json"),
                workspace=pathlib.Path("/private/run"), timeout_seconds=0,
            )

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

    def test_failed_observer_cleanup_removes_only_protocol_raw_records(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1] / ".test-work" / "runner-failure-cleanup"
        root.mkdir(parents=True, exist_ok=True)
        (root / runner.PHASE_ONE_RAW_NAME).write_text("{}", encoding="utf-8")
        (root / runner.DISPATCH_RAW_NAME).write_text("{}", encoding="utf-8")
        marker = root / "operator-note"
        marker.write_text("keep", encoding="utf-8")
        runner._discard_raw_records(root)
        self.assertFalse((root / runner.PHASE_ONE_RAW_NAME).exists())
        self.assertFalse((root / runner.DISPATCH_RAW_NAME).exists())
        self.assertTrue(marker.exists())
        marker.unlink()
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
