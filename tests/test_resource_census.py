import importlib.util
import pathlib
import struct
import tempfile
import unittest
import zipfile


MODULE_PATH = pathlib.Path(__file__).parents[1] / "tools" / "resource_census.py"
SPEC = importlib.util.spec_from_file_location("resource_census", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class ResourceCensusTests(unittest.TestCase):
    def test_aggregate_without_extracting(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            with zipfile.ZipFile(root / "one.ZIP", "w") as archive:
                archive.writestr(
                    "SCENES/ONE.ANM",
                    struct.pack("<III", 0x00414E4D, 1, 16) + b"a" * 4,
                )
                archive.writestr("SCENES/ONE.PRM", b"PRM!" + b"b" * 4)
            with zipfile.ZipFile(root / "two.ZIP", "w") as archive:
                archive.writestr(
                    "SCENES/TWO.ANM",
                    struct.pack("<III", 0x00414E4D, 2, 24) + b"c" * 12,
                )
            result = MODULE.census(root)

        self.assertEqual(result["archive_count"], 2)
        self.assertEqual(result["member_count"], 3)
        animation = result["formats"][".anm"]
        self.assertEqual(animation["count"], 2)
        self.assertEqual(animation["common_prefix_hex"], "4d4e4100")
        self.assertEqual(animation["size"], {"min": 16, "median": 20, "max": 24})
        self.assertEqual(
            animation["known_invariants"]["magic_is_MNA_nul"],
            {"passed": 2, "failed": 0},
        )
        self.assertEqual(
            animation["known_invariants"]["word_2_is_file_size"],
            {"passed": 2, "failed": 0},
        )


if __name__ == "__main__":
    unittest.main()
