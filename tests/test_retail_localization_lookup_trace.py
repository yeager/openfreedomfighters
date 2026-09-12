"""The retail localization observer format must remain source-free."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import retail_localization_lookup_trace as trace  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "observation_order": 1,
        "call_ordinal": 1,
        "lookup_key_relation": "new",
        "catalog_ordinal": 17,
        "lookup_outcome": "resolved",
        "result_kind": "formatted-value",
        "format_argument_kinds": ["signed", "opaque-string"],
    }
    value.update(changes)
    return value


class RetailLocalizationLookupTraceTests(unittest.TestCase):
    def test_keeps_only_fixed_structural_schema(self) -> None:
        result = trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [event()]})
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["events"], [event()])

    def test_rejects_source_and_executable_fields(self) -> None:
        for forbidden in ("key", "string", "text", "argument", "address", "offset", "symbol", "path", "bytes", "asset", "screenshot"):
            with self.subTest(forbidden=forbidden):
                specimen = event()
                specimen[forbidden] = 1
                with self.assertRaises(ValueError):
                    trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [specimen]})

    def test_rejects_unresolved_value_and_formatting(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(lookup_outcome="missing", catalog_ordinal=None,
                                                   result_kind="catalog-value")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(lookup_outcome="failure", catalog_ordinal=None,
                                                   result_kind="failure", format_argument_kinds=["signed"])]})

    def test_requires_strict_order_and_valid_same_key_relation(self) -> None:
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT,
                                  "events": [event(lookup_key_relation="same-as-prior")]})
        with self.assertRaises(ValueError):
            trace.sanitize_trace({"format": trace.INPUT_FORMAT, "events": [
                event(observation_order=1, call_ordinal=1),
                event(observation_order=1, call_ordinal=2, lookup_key_relation="same-as-prior"),
            ]})


if __name__ == "__main__":
    unittest.main()
