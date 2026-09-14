"""The localization repeat gate must retain only exact structural agreement."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import retail_localization_lookup_repeat_pair as pair  # noqa: E402
import retail_localization_lookup_trace as trace  # noqa: E402


def clean(**changes: object) -> dict[str, object]:
    event: dict[str, object] = {
        "observation_order": 1, "call_ordinal": 1, "lookup_site": "site.fixture.lookup",
        "lookup_key_relation": "new", "catalog_ordinal": 17, "lookup_outcome": "resolved",
        "result_kind": "formatted-value", "format_argument_kinds": ["signed"],
    }
    event.update(changes)
    return trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event]})


class RetailLocalizationLookupRepeatPairTests(unittest.TestCase):
    def test_keeps_exact_sanitized_pair(self) -> None:
        record = clean()
        self.assertEqual(pair.sanitize_repeat_pair(record, record), {
            "format": pair.OUTPUT_FORMAT, "events": record["events"]})

    def test_rejects_different_structural_behavior(self) -> None:
        with self.assertRaisesRegex(ValueError, "disagree"):
            pair.sanitize_repeat_pair(clean(), clean(lookup_outcome="missing", catalog_ordinal=None,
                                                      result_kind="no-value", format_argument_kinds=[]))

    def test_revalidates_sanitized_schema(self) -> None:
        record = clean()
        record["unexpected"] = True
        with self.assertRaises(ValueError):
            pair.sanitize_repeat_pair(record, record)
