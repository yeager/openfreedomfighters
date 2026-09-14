"""Private structural JSON paths must not escape through a parent symlink."""

from __future__ import annotations

import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import private_structural_json as structural_json  # noqa: E402


class PrivateStructuralJsonTests(unittest.TestCase):
    def test_rejects_final_and_parent_symlinks_before_resolution(self) -> None:
        work = ROOT / ".test-work" / "private-structural-json-symlink"
        target = work / "target"
        link = work / "link"
        target.mkdir(parents=True, exist_ok=True)
        link.symlink_to(target.name, target_is_directory=True)
        try:
            with self.assertRaisesRegex(ValueError, "must not traverse a symlink"):
                structural_json.outside_repository(link / "record.json", ROOT, "input")
        finally:
            link.unlink()
            target.rmdir()
            work.rmdir()

    def test_rejects_parent_traversal_before_resolution(self) -> None:
        with self.assertRaisesRegex(ValueError, "must not contain parent traversal"):
            structural_json.outside_repository(pathlib.Path("..") / "private.json", ROOT, "input")

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux preserves a // lexical anchor")
    def test_rejects_double_slash_repository_alias_before_opening(self) -> None:
        alias = pathlib.Path("//" + str(ROOT).lstrip("/")) / "README.md"
        with self.assertRaisesRegex(ValueError, "standard absolute path"):
            structural_json.outside_repository(alias, ROOT, "input")

    def test_rejects_duplicate_json_fields(self) -> None:
        work = ROOT / ".test-work" / "private-structural-json-duplicate.json"
        work.parent.mkdir(parents=True, exist_ok=True)
        work.write_text('{"format":"first","format":"second"}', encoding="utf-8")
        try:
            with self.assertRaisesRegex(ValueError, "duplicate field"):
                structural_json.read_json(work, "input")
        finally:
            work.unlink()


if __name__ == "__main__":
    unittest.main()
