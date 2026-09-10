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

TEST_ROOT = pathlib.Path.cwd() / ".test-work"
TEST_ROOT.mkdir(exist_ok=True)


class ResourceCensusTests(unittest.TestCase):
    def test_aggregate_without_extracting(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            root = pathlib.Path(directory)
            with zipfile.ZipFile(root / "one.ZIP", "w") as archive:
                archive.writestr(
                    "SCENES/ONE.ANM",
                    struct.pack("<IIIII", 0x00414E4D, 0x80000018, 24, 12, 10)
                    + b"a" * 4,
                )
                archive.writestr("SCENES/ONE.PRM", b"PRM!" + b"b" * 4)
                archive.writestr(
                    "SCENES/ONE.SUP",
                    struct.pack(
                        "<IIIIII",
                        0,
                        0x80000024,
                        36,
                        1,
                        0x46434C44,
                        20,
                    )
                    + b"fixture.dlc\0",
                )
            with zipfile.ZipFile(root / "two.ZIP", "w") as archive:
                archive.writestr(
                    "SCENES/TWO.ANM",
                    struct.pack("<IIIII", 0x00414E4D, 0x80000020, 32, 12, 10)
                    + b"c" * 12,
                )
            result = MODULE.census(root)

        self.assertEqual(result["archive_count"], 2)
        self.assertEqual(result["member_count"], 4)
        animation = result["formats"][".anm"]
        self.assertEqual(animation["count"], 2)
        self.assertEqual(animation["common_prefix_hex"], "4d4e4100")
        self.assertEqual(animation["size"], {"min": 24, "median": 28, "max": 32})
        for invariant in (
            "magic_is_MNA_nul",
            "word_1_is_flagged_file_size",
            "word_2_is_file_size",
            "directory_word_is_10_through_15_and_format_is_10",
        ):
            self.assertEqual(
                animation["known_invariants"][invariant],
                {"passed": 2, "failed": 0},
            )
        support = result["formats"][".sup"]
        for invariant in (
            "word_0_is_zero",
            "word_1_is_flagged_file_size",
            "word_2_is_file_size",
            "word_3_is_one",
            "word_4_is_DLCF",
            "DLCF_descriptor_size_matches",
            "DLCF_layout_is_scalar_or_array",
        ):
            self.assertEqual(
                support["known_invariants"][invariant],
                {"passed": 1, "failed": 0},
            )


if __name__ == "__main__":
    unittest.main()
