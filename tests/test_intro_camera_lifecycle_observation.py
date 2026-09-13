from __future__ import annotations
import pathlib
import sys
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import intro_camera_lifecycle_contract_bundle as bundle
import intro_camera_lifecycle_trace as trace
import intro_camera_observation_runner as runner
from unittest import mock
import subprocess

def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {"observation_order": 0, "stage": "entry", "callback_ordinal": 8, "sequence": "not_entered", "controller": "not_observed", "camera_owner": "not_observed", "transform": "not_observed", "view": "not_observed", "frame_delivery": "not_attempted", "outcome": "pending"}; value.update(changes); return value

def success(callback: int = 8) -> dict[str, object]:
    records = [event(callback_ordinal=callback), event(observation_order=1, stage="controller", callback_ordinal=callback, sequence="active", controller="ready"), event(observation_order=2, stage="camera_owner", callback_ordinal=callback, sequence="active", controller="ready", camera_owner="resolved"), event(observation_order=3, stage="transform", callback_ordinal=callback, sequence="active", controller="ready", camera_owner="resolved", transform="composed"), event(observation_order=4, stage="view", callback_ordinal=callback, sequence="active", controller="ready", camera_owner="resolved", transform="composed", view="admitted"), event(observation_order=5, stage="frame_delivery", callback_ordinal=callback, sequence="active", controller="ready", camera_owner="resolved", transform="composed", view="admitted", frame_delivery="delivered", outcome="success"), event(observation_order=6, stage="completion", callback_ordinal=callback, sequence="active", controller="ready", camera_owner="resolved", transform="composed", view="admitted", frame_delivery="delivered", outcome="success")]
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": records})

def failure() -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(callback_ordinal=9), event(observation_order=1, stage="failure", callback_ordinal=9, sequence="active", controller="ready", camera_owner="resolved", transform="rejected", outcome="failure")]})

class IntroCameraLifecycleObservationTests(unittest.TestCase):
    def test_keeps_only_complete_ordered_lifecycle(self) -> None:
        self.assertEqual(len(success()["events"]), 7)
        self.assertTrue(bundle.bundle_contract(success(), success(), failure())["candidate"]["frame_delivered"])
    def test_rejects_raw_camera_material(self) -> None:
        for field in ("address", "offset", "identity", "path", "asset", "matrix", "vector", "position", "rotation", "scale", "float", "time", "image", "bytes", "text"):
            raw = event(); raw[field] = 1
            with self.subTest(field=field):
                with self.assertRaises(ValueError): trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [raw]})
    def test_requires_owner_transform_view_and_delivery_in_order(self) -> None:
        with self.assertRaises(ValueError): trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(stage="view", sequence="active", controller="ready", view="admitted")]})
        with self.assertRaises(ValueError): trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(stage="completion", sequence="active", controller="ready", camera_owner="resolved", transform="composed", view="admitted", outcome="success")]})
    def test_bundle_rejects_drift_and_no_failure(self) -> None:
        with self.assertRaises(ValueError): bundle.bundle_contract(success(), success(10), failure())
        with self.assertRaises(ValueError): bundle.bundle_contract(success(), success(), success(9))
    def test_runner_has_fresh_isolated_no_stream_command(self) -> None:
        raw = lambda record: {"format": trace.INPUT_FORMAT, "events": record["events"]}
        with mock.patch.object(runner, "_observer", return_value=pathlib.Path("/private/observer")), mock.patch.object(runner, "_workspace", return_value=pathlib.Path("/private/workspace")), mock.patch.object(pathlib.Path, "iterdir", return_value=[pathlib.Path(name) for name in runner.RAW_NAMES]), mock.patch.object(runner, "_read", side_effect=[raw(success()), raw(success()), raw(failure())]), mock.patch.object(runner, "_discard_raw"), mock.patch.object(runner, "_write_new"):
            called: list[object] = []
            def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]: called.extend((args, kwargs)); return subprocess.CompletedProcess(args[0], 0)
            runner.execute_observation(observer=pathlib.Path("/private/observer"), workspace=pathlib.Path("/private/workspace"), run=fake_run)
        self.assertIn("fresh-isolated", called[0][0]); self.assertFalse(called[1]["shell"]); self.assertEqual(called[1]["stdout"], subprocess.DEVNULL)

if __name__ == "__main__": unittest.main()
