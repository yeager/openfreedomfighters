from __future__ import annotations
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import intro_loader_tail_observation as trace


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 0, "stage": "entry", "callback_ordinal": 11,
        "tail": "not_entered", "named_global": "not_present", "renderer_payload": "not_present",
        "renderer_state": "not_attempted", "associations": "not_present", "source_lease": "not_attempted",
        "camera": "not_attempted", "outer_scene": "not_attempted", "between_scene": "not_attempted",
        "finalization": "not_attempted", "spatial": "not_attempted", "saved_0x4000": "not_attempted", "outcome": "pending"}
    value.update(changes); return value


def success() -> dict[str, object]:
    records = [
        event(tail="entered"),
        event(observation_order=1, stage="named_global", tail="entered", named_global="accepted"),
        event(observation_order=2, stage="renderer", tail="entered", named_global="accepted", renderer_payload="consumed", renderer_state="selected"),
        event(observation_order=3, stage="renderer", tail="entered", named_global="accepted", renderer_payload="consumed", renderer_state="restored"),
        event(observation_order=4, stage="associations", tail="entered", named_global="accepted", renderer_payload="consumed", renderer_state="restored", associations="applied"),
        event(observation_order=5, stage="source_lease", tail="entered", named_global="accepted", renderer_payload="consumed", renderer_state="restored", associations="applied", source_lease="released"),
        event(observation_order=6, stage="camera", tail="entered", named_global="accepted", renderer_payload="consumed", renderer_state="restored", associations="applied", source_lease="released", camera="fallback_registered"),
        event(observation_order=7, stage="outer_scene", tail="entered", named_global="accepted", renderer_payload="consumed", renderer_state="restored", associations="applied", source_lease="released", camera="fallback_registered", outer_scene="called"),
        event(observation_order=8, stage="between_scene", tail="entered", named_global="accepted", renderer_payload="consumed", renderer_state="restored", associations="applied", source_lease="released", camera="fallback_registered", outer_scene="called", between_scene="called"),
        event(observation_order=9, stage="finalization", tail="entered", named_global="accepted", renderer_payload="consumed", renderer_state="restored", associations="applied", source_lease="released", camera="fallback_registered", outer_scene="called", between_scene="called", finalization="called"),
        event(observation_order=10, stage="saved_resources", tail="entered", named_global="accepted", renderer_payload="consumed", renderer_state="restored", associations="applied", source_lease="released", camera="fallback_registered", outer_scene="called", between_scene="called", finalization="called", spatial="admitted", saved_0x4000="applied"),
        event(observation_order=11, stage="completion", tail="entered", named_global="accepted", renderer_payload="consumed", renderer_state="restored", associations="applied", source_lease="released", camera="fallback_registered", outer_scene="called", between_scene="called", finalization="called", spatial="admitted", saved_0x4000="applied", outcome="success")]
    return {"format": trace.INPUT_FORMAT, "events": records}


class IntroLoaderTailObservationTests(unittest.TestCase):
    def test_keeps_only_complete_concrete_tail(self) -> None:
        clean = trace.sanitize_trace(success())
        self.assertEqual(clean["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(clean["events"][-1]["stage"], "completion")

    def test_rejects_ambiguous_completion_without_restoration_or_service(self) -> None:
        for field, value in (("renderer_state", "selected"), ("source_lease", "not_attempted"), ("camera", "not_attempted"), ("outer_scene", "not_attempted"), ("saved_0x4000", "not_attempted")):
            raw = success(); raw["events"][-1][field] = value
            with self.subTest(field=field):
                with self.assertRaises(ValueError): trace.sanitize_trace(raw)

    def test_rejects_free_form_and_wrong_callback(self) -> None:
        raw = success(); raw["events"][0]["address"] = 1
        with self.assertRaises(ValueError): trace.sanitize_trace(raw)
        raw = success(); raw["events"][1]["callback_ordinal"] = 12
        with self.assertRaises(ValueError): trace.sanitize_trace(raw)

    def test_accepts_a_terminal_failure_only(self) -> None:
        raw = {"format": trace.INPUT_FORMAT, "events": [event(tail="entered"), event(observation_order=1, stage="failure", tail="entered", named_global="rejected", outcome="failure")]}
        self.assertEqual(trace.sanitize_trace(raw)["events"][-1]["outcome"], "failure")


if __name__ == "__main__": unittest.main()
