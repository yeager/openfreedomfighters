"""Repeat first-mission evidence stays private, structural, and bounded."""

from __future__ import annotations

import json
import os
import pathlib
import stat
import sys
import tempfile
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import first_mission_observation_repeat_bundle as bundle  # noqa: E402
import first_mission_observation_trace as trace  # noqa: E402


FINGERPRINT = "0" * 64


def event(order: int, probe: str, boundary: str, state: str, changed: bool) -> dict[str, object]:
    return {
        "observation_order": order,
        "probe": probe,
        "boundary": boundary,
        "state": state,
        "visible_change": changed,
    }


def document(run_kind: str, events: list[dict[str, object]]) -> dict[str, object]:
    return trace.sanitize_trace({
        "format": trace.INPUT_FORMAT,
        "method_version": 1,
        "verified_data_manifest_fingerprint": FINGERPRINT,
        "platform": "windows",
        "architecture": "x86",
        "input_device": "keyboard_mouse",
        "run_kind": run_kind,
        "events": events,
    })


def baseline() -> dict[str, object]:
    return document("baseline", [
        event(0, "launch", "handoff", "loading_visible", True),
        event(1, "idle", "hud", "stable", False),
    ])


def experiment() -> dict[str, object]:
    return document("input_experiment", [
        event(0, "launch", "handoff", "loading_visible", True),
        event(1, "movement", "control", "movement_and_camera", True),
        event(2, "movement", "mission", "loading", False),
    ])


class FirstMissionObservationRepeatBundleTests(unittest.TestCase):
    def test_retains_aggregate_only_for_two_identical_baselines_and_one_probe(self) -> None:
        result = bundle.sanitize_repeat_bundle(baseline(), baseline(), experiment())
        self.assertEqual(result["format"], bundle.OUTPUT_FORMAT)
        self.assertEqual(result["baseline_run_count"], 2)
        self.assertEqual(result["experiment_probe"], "movement")
        self.assertEqual(result["visible_action_outcome"], {
            "boundary": "control", "state": "movement_and_camera"})
        self.assertEqual(result["reset_or_terminal_outcome"], {
            "boundary": "mission", "state": "loading"})
        self.assertNotIn("events", result)

    def test_rejects_nonrepeatable_or_uncontrolled_evidence(self) -> None:
        changed_baseline = baseline()
        changed_baseline["events"][1]["state"] = "hidden"
        with self.assertRaisesRegex(ValueError, "agree exactly"):
            bundle.sanitize_repeat_bundle(baseline(), changed_baseline, experiment())
        multiple = experiment()
        multiple["events"][2]["probe"] = "look"
        with self.assertRaisesRegex(ValueError, "exactly one non-launch probe"):
            bundle.sanitize_repeat_bundle(baseline(), baseline(), multiple)
        missing_terminal = experiment()
        missing_terminal["events"][2]["state"] = "unchanged"
        with self.assertRaisesRegex(ValueError, "reset or terminal"):
            bundle.sanitize_repeat_bundle(baseline(), baseline(), missing_terminal)

    def test_rejects_mismatched_metadata_and_extra_content(self) -> None:
        wrong_metadata = experiment()
        wrong_metadata["architecture"] = "x86_64"
        with self.assertRaisesRegex(ValueError, "identical metadata"):
            bundle.sanitize_repeat_bundle(baseline(), baseline(), wrong_metadata)
        contaminated = experiment()
        contaminated["events"][1]["address"] = 1
        with self.assertRaises(ValueError):
            bundle.sanitize_repeat_bundle(baseline(), baseline(), contaminated)

    def test_cli_uses_new_owner_only_files_without_following_symlinks(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            private = pathlib.Path(directory)
            records = (baseline(), baseline(), experiment())
            inputs = [private / f"input-{index}.json" for index in range(len(records))]
            for path, record in zip(inputs, records, strict=True):
                path.write_text(json.dumps(record), encoding="utf-8")
            output = private / "bundle.json"
            old_argv = sys.argv
            try:
                sys.argv = ["bundle", *(str(path) for path in inputs), str(output)]
                self.assertEqual(bundle.main(), 0)
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o600)
                self.assertEqual(bundle.main(), 1)
            finally:
                sys.argv = old_argv
            target = private / "target.json"
            target.write_text(json.dumps(baseline()), encoding="utf-8")
            link = private / "link.json"
            link.symlink_to(target.name)
            with self.assertRaisesRegex(ValueError, "must not be a symlink"):
                bundle._outside_repository(link, "first baseline")
            self.assertTrue(target.exists())


if __name__ == "__main__":
    unittest.main()
