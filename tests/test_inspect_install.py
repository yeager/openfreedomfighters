import importlib.util
import pathlib
import struct
import tempfile
import unittest


MODULE_PATH = pathlib.Path(__file__).parents[1] / "tools" / "inspect_install.py"
SPEC = importlib.util.spec_from_file_location("inspect_install", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)

TEST_ROOT = pathlib.Path.cwd() / ".test-work"
TEST_ROOT.mkdir(exist_ok=True)


class PeSummaryTests(unittest.TestCase):
    def test_known_ordinal_resolution_is_case_insensitive(self):
        self.assertEqual(MODULE.resolve_ordinal("WS2_32.DLL", 115), "WSAStartup")
        self.assertEqual(MODULE.resolve_ordinal("DSOUND.dll", 2), "DirectSoundEnumerateA")
        self.assertIsNone(MODULE.resolve_ordinal("unknown.dll", 1))

    def test_minimal_pe32(self):
        image = bytearray(0x200)
        image[:2] = b"MZ"
        struct.pack_into("<I", image, 0x3C, 0x80)
        image[0x80:0x84] = b"PE\0\0"
        struct.pack_into("<HHIIIHH", image, 0x84, 0x14C, 1, 123, 0, 0, 0xE0, 0x102)
        optional = 0x98
        struct.pack_into("<H", image, optional, 0x10B)
        struct.pack_into("<I", image, optional + 16, 0x1234)
        struct.pack_into("<I", image, optional + 28, 0x400000)
        struct.pack_into("<I", image, optional + 92, 16)
        struct.pack_into("<II", image, optional + 96 + 9 * 8, 0x1040, 24)
        section = optional + 0xE0
        image[section:section + 8] = b".text\0\0\0"
        struct.pack_into("<IIII", image, section + 8, 0x100, 0x1000, 0x80, 0x180)
        struct.pack_into("<IIIIII", image, 0x1C0, 0, 0, 0, 0x401060, 0, 0)
        struct.pack_into("<II", image, 0x1E0, 0x401234, 0)
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            path = pathlib.Path(directory) / "fixture.exe"
            path.write_bytes(image)
            result = MODULE.pe_summary(path)
        self.assertEqual(result["machine"], "0x014c")
        self.assertEqual(result["bitness"], 32)
        self.assertEqual(result["entrypoint_rva"], 0x1234)
        self.assertEqual(result["sections"][0]["name"], ".text")
        self.assertEqual(result["imports"], [])
        self.assertEqual(
            result["data_directories"]["tls"],
            {"address": 0x1040, "address_kind": "rva", "size": 24},
        )
        self.assertEqual(result["tls_callback_rvas"], [0x1234])
        self.assertIsNone(result["load_config"])

    def test_pe32_load_config_reports_only_bounded_metadata(self):
        image = bytearray(0x400)
        image[:2] = b"MZ"
        struct.pack_into("<I", image, 0x3C, 0x80)
        image[0x80:0x84] = b"PE\0\0"
        struct.pack_into("<HHIIIHH", image, 0x84, 0x14C, 1, 123, 0, 0, 0xE0, 0x102)
        optional = 0x98
        struct.pack_into("<H", image, optional, 0x10B)
        struct.pack_into("<I", image, optional + 28, 0x400000)
        struct.pack_into("<I", image, optional + 92, 16)
        struct.pack_into("<II", image, optional + 96 + 10 * 8, 0x1040, 64)
        section = optional + 0xE0
        image[section:section + 8] = b".text\0\0\0"
        struct.pack_into("<IIII", image, section + 8, 0x200, 0x1000, 0x200, 0x200)
        # A bounded 64-byte directory reaches SecurityCookie but not the later
        # SafeSEH or Control Flow Guard fields, regardless of a larger declared
        # structure size.
        struct.pack_into("<I", image, 0x240, 64)
        struct.pack_into("<I", image, 0x240 + 12, 0x10)
        struct.pack_into("<I", image, 0x240 + 16, 0x20)
        struct.pack_into("<I", image, 0x240 + 44, 0x40)
        struct.pack_into("<I", image, 0x240 + 60, 0x401100)
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            path = pathlib.Path(directory) / "fixture.exe"
            path.write_bytes(image)
            result = MODULE.pe_summary(path)
        self.assertEqual(
            result["load_config"],
            {
                "declared_size": 64,
                "directory_size": 64,
                "parsed_size": 64,
                "layout": "PE32",
                "timestamp": 0,
                "major_version": 0,
                "minor_version": 0,
                "global_flags_clear": 0x10,
                "global_flags_set": 0x20,
                "critical_section_default_timeout": 0,
                "decommit_free_block_threshold": 0,
                "decommit_total_free_threshold": 0,
                "lock_prefix_table_present": False,
                "maximum_allocation_size": 0,
                "virtual_memory_threshold": 0,
                "process_heap_flags": 0x40,
                "process_affinity_mask": 0,
                "csd_version": 0,
                "dependent_load_flags": 0,
                "edit_list_present": False,
                "security_cookie_present": True,
                "seh_metadata_available": False,
                "seh_table_present": False,
                "seh_handler_count": None,
                "guard_cf_metadata_available": False,
                "guard_cf_table_present": False,
                "guard_cf_function_count": None,
                "guard_flags": None,
            },
        )

    def test_pe32_plus_load_config_uses_64_bit_field_offsets(self):
        image = bytearray(0x500)
        image[:2] = b"MZ"
        struct.pack_into("<I", image, 0x3C, 0x80)
        image[0x80:0x84] = b"PE\0\0"
        struct.pack_into("<HHIIIHH", image, 0x84, 0x8664, 1, 7, 0, 0, 0xF0, 0x22)
        optional = 0x98
        struct.pack_into("<H", image, optional, 0x20B)
        struct.pack_into("<Q", image, optional + 24, 0x140000000)
        struct.pack_into("<I", image, optional + 108, 16)
        struct.pack_into("<II", image, optional + 112 + 10 * 8, 0x1040, 96)
        section = optional + 0xF0
        image[section:section + 8] = b".text\0\0\0"
        struct.pack_into("<IIII", image, section + 8, 0x300, 0x1000, 0x300, 0x200)
        load = 0x240
        struct.pack_into("<I", image, load, 96)
        struct.pack_into("<IHH", image, load + 4, 11, 2, 3)
        struct.pack_into("<III", image, load + 12, 4, 5, 6)
        struct.pack_into("<QQ", image, load + 24, 7, 8)
        struct.pack_into("<QQQ", image, load + 40, 0x140001000, 9, 10)
        struct.pack_into("<IQHH", image, load + 64, 12, 13, 14, 15)
        struct.pack_into("<QQ", image, load + 80, 0x140002000, 0x140003000)
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            path = pathlib.Path(directory) / "fixture64.exe"
            path.write_bytes(image)
            result = MODULE.pe_summary(path)
        self.assertEqual(result["load_config"], {
            "declared_size": 96, "directory_size": 96, "parsed_size": 96,
            "layout": "PE32+", "timestamp": 11, "major_version": 2,
            "minor_version": 3, "global_flags_clear": 4, "global_flags_set": 5,
            "critical_section_default_timeout": 6, "decommit_free_block_threshold": 7,
            "decommit_total_free_threshold": 8, "lock_prefix_table_present": True,
            "maximum_allocation_size": 9, "virtual_memory_threshold": 10,
            "process_heap_flags": 12, "process_affinity_mask": 13,
            "csd_version": 14, "dependent_load_flags": 15,
            "edit_list_present": True, "security_cookie_present": True,
        })


if __name__ == "__main__":
    unittest.main()
