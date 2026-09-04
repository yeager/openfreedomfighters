import importlib.util
import pathlib
import unittest


MODULE_PATH = pathlib.Path(__file__).parents[1] / "tools" / "code_census.py"
SPEC = importlib.util.spec_from_file_location("code_census", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class MappedAsciiTests(unittest.TestCase):
    def test_reads_only_mapped_ascii_identifier(self):
        image = bytearray(0x120)
        image[0x100:0x10D] = b"Example.dll\0"
        sections = [
            {
                "virtual_address": 0x1000,
                "virtual_size": 0x20,
                "raw_offset": 0x100,
                "raw_size": 0x20,
            }
        ]
        self.assertEqual(
            MODULE._mapped_ascii(bytes(image), 0x401000, 0x400000, sections),
            "Example.dll",
        )
        self.assertIsNone(
            MODULE._mapped_ascii(bytes(image), 0x402000, 0x400000, sections)
        )


if __name__ == "__main__":
    unittest.main()

