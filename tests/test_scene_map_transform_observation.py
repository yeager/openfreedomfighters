from __future__ import annotations

import json
import pathlib
import stat
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import scene_map_transform_contract_bundle as bundle
import scene_map_transform_trace as trace


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {"observation_order": 0, "stage": "entry", "callback_ordinal": 4,
                                "scene": "entered", "parent_relation": "not_observed", "local_relation": "not_observed",
                                "world_relation": "not_observed", "render_boundary": "not_entered", "outcome": "pending"}
    value.update(changes)
    return value


def success(callback: int = 4) -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [
        event(callback_ordinal=callback),
        event(observation_order=1, stage="parent_relation", callback_ordinal=callback, scene="active", parent_relation="ready"),
        event(observation_order=2, stage="local_relation", callback_ordinal=callback, scene="active", parent_relation="ready", local_relation="ready"),
        event(observation_order=3, stage="world_relation", callback_ordinal=callback, scene="active", parent_relation="ready", local_relation="ready", world_relation="resolved"),
        event(observation_order=4, stage="render_boundary", callback_ordinal=callback, scene="active", parent_relation="ready", local_relation="ready", world_relation="resolved", render_boundary="consumed"),
        event(observation_order=5, stage="completion", callback_ordinal=callback, scene="active", parent_relation="ready", local_relation="ready", world_relation="resolved", render_boundary="consumed", outcome="success"),
    ]})


def failure() -> dict[str, object]:
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [
        event(callback_ordinal=5),
        event(observation_order=1, stage="failure", callback_ordinal=5, scene="active", outcome="failure"),
    ]})


class SceneMapTransformObservationTests(unittest.TestCase):
    def test_sanitizes_only_ordered_vector_free_relations(self) -> None:
        result = success()
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(len(result["events"]), 6)

    def test_rejects_retail_identity_numeric_and_transform_payloads(self) -> None:
        for forbidden in ("id", "name", "asset", "path", "address", "matrix", "vector", "position", "rotation", "scale", "float"):
            with self.subTest(forbidden=forbidden):
                raw = event()
                raw[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [raw]})

    def test_requires_parent_then_local_then_world_then_render(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(stage="world_relation", scene="active", world_relation="resolved")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(stage="completion", scene="active", parent_relation="ready", local_relation="ready", world_relation="resolved", outcome="success")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event(stage="failure", scene="active", render_boundary="consumed", outcome="failure")]})

    def test_contract_requires_identical_complete_success_pair_and_failure(self) -> None:
        receipt = bundle.bundle_contract(success(), success(), failure())
        self.assertTrue(receipt["candidate"]["world_relation_resolved"])
        with self.assertRaises(ValueError):
            bundle.bundle_contract(success(), success(6), failure())
        with self.assertRaises(ValueError):
            bundle.bundle_contract(success(), success(), success(5))

    def test_cli_uses_private_nofollow_owner_only_files(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            private = pathlib.Path(directory)
            source, output = private / "raw.json", private / "trace.json"
            source.write_text(json.dumps({"format": trace.INPUT_FORMAT, "events": success()["events"]}), encoding="utf-8")
            old = sys.argv
            try:
                sys.argv = ["map-transform", str(source), str(output)]
                self.assertEqual(trace.main(), 0)
            finally:
                sys.argv = old
            self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o600)
            link = private / "link.json"; link.symlink_to(source.name)
            with self.assertRaisesRegex(ValueError, "must not contain a symlink"):
                trace._outside_repository(link, "input")
            oversized = private / "large.json"; oversized.write_bytes(b" " * (trace.MAX_PRIVATE_RECORD_BYTES + 1))
            with self.assertRaisesRegex(ValueError, "bounded regular private file"):
                trace._read_private_json_no_follow(oversized, "input")

    def test_bundle_cli_refuses_existing_or_linked_private_records(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            private = pathlib.Path(directory)
            candidate, repeat, rejected, output = (private / name for name in ("candidate.json", "repeat.json", "failure.json", "receipt.json"))
            for path, value in ((candidate, success()), (repeat, success()), (rejected, failure())):
                path.write_text(json.dumps(value), encoding="utf-8")
            old = sys.argv
            try:
                sys.argv = ["map-transform-bundle", str(candidate), str(repeat), str(rejected), str(output)]
                self.assertEqual(bundle.main(), 0)
            finally:
                sys.argv = old
            self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o600)
            linked = private / "linked.json"; linked.symlink_to(candidate.name)
            with self.assertRaisesRegex(ValueError, "must not contain a symlink"):
                trace._outside_repository(linked, "candidate")
            old = sys.argv
            try:
                sys.argv = ["map-transform-bundle", str(candidate), str(repeat), str(rejected), str(output)]
                self.assertEqual(bundle.main(), 1)
            finally:
                sys.argv = old


if __name__ == "__main__":
    unittest.main()
