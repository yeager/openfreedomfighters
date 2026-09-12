"""Private reviewed-retail lookup artifact import must remain source-free."""

from __future__ import annotations

import json
import os
import pathlib
import sys
import tempfile
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import import_reviewed_retail_lookup_artifact as importer  # noqa: E402


def event(**changes: object) -> dict[str, object]:
    result: dict[str, object] = {
        "observation_order": 1,
        "call_ordinal": 1,
        "lookup_site": "site.fixture.lookup",
        "lookup_key_relation": "new",
        "catalog_ordinal": 2,
        "lookup_outcome": "resolved",
        "result_kind": "catalog-value",
        "format_argument_kinds": [],
    }
    result.update(changes)
    return result


def trace(*events: dict[str, object]) -> dict[str, object]:
    return {"format": importer.TRACE_FORMAT, "events": list(events)}


def read_u64(data: bytes, offset: int) -> tuple[int, int]:
    return int.from_bytes(data[offset:offset + 8], "little"), offset + 8


def read_blob(data: bytes, offset: int) -> tuple[bytes, int]:
    length, offset = read_u64(data, offset)
    return data[offset:offset + length], offset + length


class ImportReviewedRetailLookupArtifactTests(unittest.TestCase):
    def setUp(self) -> None:
        cache = pathlib.Path.home() / ".cache"
        cache.mkdir(mode=0o700, exist_ok=True)
        self.temporary_directory = tempfile.TemporaryDirectory(
            prefix="off-lookup-import-", dir=cache)
        self.root = pathlib.Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def binding(self) -> tuple[str, str, int, int]:
        return importer.binding_from_values("parser.fixture.v1", "source.fixture.v1", 3)

    def test_writes_exact_runtime_binary_for_unique_unformatted_events(self) -> None:
        data = importer.build_artifact(trace(event()), self.binding())
        self.assertTrue(data.startswith(importer.MAGIC))
        offset = len(importer.MAGIC)
        parser, offset = read_blob(data, offset)
        source_set, offset = read_blob(data, offset)
        first, offset = read_u64(data, offset)
        count, offset = read_u64(data, offset)
        observations, offset = read_u64(data, offset)
        site, offset = read_blob(data, offset)
        ordinal, offset = read_u64(data, offset)
        self.assertEqual((parser, source_set, first, count),
                         (b"parser.fixture.v1", b"source.fixture.v1", 0, 3))
        self.assertEqual((observations, site, ordinal, offset),
                         (1, b"site.fixture.lookup", 2, len(data)))

    def test_rejects_unresolved_and_formatted_events(self) -> None:
        invalid = (
            event(lookup_outcome="missing", catalog_ordinal=None,
                  result_kind="no-value"),
            event(result_kind="formatted-value", format_argument_kinds=["signed"]),
        )
        for specimen in invalid:
            with self.subTest(specimen=specimen):
                with self.assertRaisesRegex(ValueError, "unresolved or formatted"):
                    importer.build_artifact(trace(specimen), self.binding())

    def test_rejects_duplicate_labels_ordinals_and_out_of_range_ordinals(self) -> None:
        duplicate_site = trace(event(), event(observation_order=2, call_ordinal=2,
                                               catalog_ordinal=1))
        duplicate_ordinal = trace(event(), event(observation_order=2, call_ordinal=2,
                                                  lookup_site="site.fixture.other"))
        out_of_range = trace(event(catalog_ordinal=3))
        for specimen, reason in ((duplicate_site, "duplicate lookup site"),
                                 (duplicate_ordinal, "duplicate catalog ordinal"),
                                 (out_of_range, "outside the bound source span")):
            with self.subTest(reason=reason):
                with self.assertRaisesRegex(ValueError, reason):
                    importer.build_artifact(specimen, self.binding())

    def test_binding_manifest_is_fixed_source_free_schema(self) -> None:
        manifest = {"format": importer.BINDING_FORMAT,
                    "parser_identity": "parser.fixture.v1",
                    "source_set": "source.fixture.v1",
                    "first_ordinal": 0, "ordinal_count": 3}
        self.assertEqual(importer._binding(manifest), self.binding())
        manifest["path"] = "forbidden"
        with self.assertRaises(ValueError):
            importer._binding(manifest)

    def test_cli_never_overwrites_existing_output(self) -> None:
        trace_path = self.root / "trace.json"
        output_path = self.root / importer.OUTPUT_FILENAME
        trace_path.write_text(json.dumps(trace(event())), encoding="utf-8")
        output_path.write_bytes(b"keep")
        old_argv = sys.argv
        try:
            sys.argv = ["importer", str(trace_path), str(output_path),
                        "--parser-identity", "parser.fixture.v1",
                        "--source-set", "source.fixture.v1", "--ordinal-count", "3"]
            self.assertEqual(importer.main(), 1)
        finally:
            sys.argv = old_argv
        self.assertEqual(output_path.read_bytes(), b"keep")

    def test_cli_reads_source_free_manifest_and_creates_private_runtime_filename(self) -> None:
        trace_path = self.root / "trace.json"
        manifest_path = self.root / "binding.json"
        output_path = self.root / importer.OUTPUT_FILENAME
        trace_path.write_text(json.dumps(trace(event())), encoding="utf-8")
        manifest_path.write_text(json.dumps({
            "format": importer.BINDING_FORMAT,
            "parser_identity": "parser.fixture.v1",
            "source_set": "source.fixture.v1",
            "first_ordinal": 0,
            "ordinal_count": 3,
        }), encoding="utf-8")
        old_argv = sys.argv
        try:
            sys.argv = ["importer", str(trace_path), str(output_path),
                        "--binding-manifest", str(manifest_path)]
            self.assertEqual(importer.main(), 0)
        finally:
            sys.argv = old_argv
        self.assertTrue(output_path.read_bytes().startswith(importer.MAGIC))
        self.assertEqual(output_path.stat().st_mode & 0o777, 0o600)

    @unittest.skipUnless(hasattr(os, "symlink"), "symlinks unavailable")
    def test_cli_rejects_symlink_trace(self) -> None:
        actual = self.root / "actual.json"
        link = self.root / "trace.json"
        actual.write_text(json.dumps(trace(event())), encoding="utf-8")
        link.symlink_to(actual)
        old_argv = sys.argv
        try:
            sys.argv = ["importer", str(link), str(self.root / importer.OUTPUT_FILENAME),
                        "--parser-identity", "parser.fixture.v1",
                        "--source-set", "source.fixture.v1", "--ordinal-count", "3"]
            self.assertEqual(importer.main(), 1)
        finally:
            sys.argv = old_argv


if __name__ == "__main__":
    unittest.main()
