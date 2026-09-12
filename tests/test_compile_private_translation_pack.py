"""Source-free tests for the private ID-only translation pack compiler."""
from __future__ import annotations
import importlib.util
import pathlib
import unittest

PATH = pathlib.Path(__file__).resolve().parents[1] / "tools" / "compile_private_translation_pack.py"
SPEC = importlib.util.spec_from_file_location("compile_private_translation_pack", PATH)
assert SPEC and SPEC.loader
PACK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACK)

class CompilePrivateTranslationPackTests(unittest.TestCase):
    def setUp(self) -> None:
        self.binding = {"format": PACK.BINDING_FORMAT, "parser_identity": "parser.fixture.v1", "source_set": "source.fixture.v1", "first_ordinal": 0, "ordinal_count": 2}
        self.source = {"format": PACK.SOURCE_FORMAT, "locale": "sv", "complete": False, "entries": [{"id": "off.retail.source.fixture.v1.1", "text": "Projektöversättning"}]}

    def test_builds_canonical_runtime_package(self) -> None:
        filename, package = PACK.build_pack(self.binding, self.source)
        self.assertEqual(filename, "sv.offl10n")
        self.assertTrue(package.startswith(PACK.MAGIC))
        self.assertIn("Projektöversättning".encode(), package)

    def test_rejects_source_field_and_bad_ids(self) -> None:
        source = dict(self.source); source["english"] = "not allowed"
        with self.assertRaises(ValueError): PACK.build_pack(self.binding, source)
        source = dict(self.source); source["entries"] = [{"id": "off.retail.source.fixture.v1.02", "text": "X"}]
        with self.assertRaises(ValueError): PACK.build_pack(self.binding, source)
        source["entries"] = [{"id": "off.retail.source.fixture.v1.2", "text": "X"}]
        with self.assertRaises(ValueError): PACK.build_pack(self.binding, source)

    def test_complete_pack_requires_exact_span_and_orders_entries(self) -> None:
        source = dict(self.source); source["complete"] = True
        with self.assertRaises(ValueError): PACK.build_pack(self.binding, source)
        source["entries"] = [{"id": "off.retail.source.fixture.v1.1", "text": "Ett"}, {"id": "off.retail.source.fixture.v1.0", "text": "Noll"}]
        _, package = PACK.build_pack(self.binding, source)
        self.assertLess(package.find(b".0"), package.find(b".1"))

    def test_rejects_unsupported_locale_and_empty_text(self) -> None:
        source = dict(self.source); source["locale"] = "sv-SE"
        with self.assertRaises(ValueError): PACK.build_pack(self.binding, source)
        source["locale"] = "sv"; source["entries"] = [{"id": "off.retail.source.fixture.v1.1", "text": ""}]
        with self.assertRaises(ValueError): PACK.build_pack(self.binding, source)

if __name__ == "__main__": unittest.main()
