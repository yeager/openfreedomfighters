"""Private soundtrack cue evidence must stay structural and fail closed."""

from __future__ import annotations

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import import_reviewed_soundtrack_cue_bindings as importer  # noqa: E402
import soundtrack_cue_evidence_bundle as bundle  # noqa: E402
import soundtrack_cue_observation_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {"observation_order": 1, "cue_token": 1,
                                "album_ordinal": 1, "format": "flac",
                                "comparison_outcome": "verified-match",
                                "timing_relation": "exact"}
    value.update(changes)
    return value


def record(events: list[dict[str, object]]) -> dict[str, object]:
    return {"format": trace.INPUT_FORMAT, "method_version": 1,
            "verified_data_manifest_fingerprint": "0" * 64, "platform": "windows",
            "architecture": "x86", "events": events}


class SoundtrackCueObservationTests(unittest.TestCase):
    def test_trace_keeps_only_structural_fixed_schema(self) -> None:
        result = trace.sanitize_trace(record([event()]))
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"][0]["cue_token"], 1)
        for forbidden in ("path", "title", "sample", "duration", "hash", "address", "bytes"):
            specimen = event()
            specimen[forbidden] = "forbidden"
            with self.subTest(forbidden=forbidden):
                with self.assertRaises(ValueError):
                    trace.sanitize_trace(record([specimen]))

    def test_trace_requires_consistent_match_and_timing_categories(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace(record([event(comparison_outcome="verified-match",
                                             timing_relation="not-equivalent")]))
        with self.assertRaises(ValueError):
            trace.sanitize_trace(record([event(comparison_outcome="rejected",
                                             timing_relation="exact")]))

    def test_bundle_requires_two_identical_positive_runs_and_distinct_negative(self) -> None:
        success = trace.sanitize_trace(record([event()]))
        rejected = trace.sanitize_trace(record([event(cue_token=2, album_ordinal=2,
                                                    format="mp3", comparison_outcome="rejected",
                                                    timing_relation="not-equivalent")]))
        result = bundle.sanitize_bundle(success, success, rejected)
        self.assertEqual(result["positive_repeat_count"], 2)
        self.assertEqual(result["bindings"], [{"cue_token": 1, "album_ordinal": 1, "format": "flac"}])
        with self.assertRaises(ValueError):
            bundle.sanitize_bundle(success, rejected, rejected)

    def test_importer_requires_catalog_digest_for_every_observed_binding(self) -> None:
        success = trace.sanitize_trace(record([event()]))
        rejected = trace.sanitize_trace(record([event(cue_token=2, album_ordinal=2,
                                                    comparison_outcome="rejected",
                                                    timing_relation="not-equivalent")]))
        evidence = bundle.sanitize_bundle(success, success, rejected)
        catalog = {"format": importer.CATALOG_FORMAT,
                   "editions": [{"album_ordinal": 1, "format": "flac", "sha256": "a" * 64}]}
        receipt = importer.build_receipt(evidence, catalog)
        self.assertEqual(receipt["bindings"][0]["expected_sha256"], "a" * 64)
        with self.assertRaises(ValueError):
            importer.build_receipt(evidence, {"format": importer.CATALOG_FORMAT, "editions": []})


if __name__ == "__main__":
    unittest.main()
