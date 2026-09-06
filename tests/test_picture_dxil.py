"""Container-boundary tests only; synthetic chunks are not executable DXIL."""

import importlib.util
from pathlib import Path
import struct
import unittest


_TOOL = Path(__file__).resolve().parents[1] / "tools/build_picture_dxil.py"
_SPEC = importlib.util.spec_from_file_location("picture_dxil_generator", _TOOL)
_MODULE = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_MODULE)
validate_container = _MODULE.validate_container


def container(chunks):
    """Build structurally bounded chunks, without fabricating compiler output."""
    header = bytearray(32 + 4 * len(chunks))
    header[:4] = b"DXBC"
    struct.pack_into("<I", header, 28, len(chunks))
    for index, (tag, payload) in enumerate(chunks):
        struct.pack_into("<I", header, 32 + index * 4, len(header))
        header.extend(tag + struct.pack("<I", len(payload)) + payload)
    struct.pack_into("<I", header, 24, len(header))
    return header


class PictureDxilContainerTests(unittest.TestCase):
    def test_structurally_bounded_dxil_tag_is_accepted(self):
        # This validates only the helper's container bounds/tag contract. DXC
        # and its validator remain required to produce an actual shader.
        validate_container(container([(b"DXIL", b"synthetic bounds fixture")]))

    def test_multiple_chunks_and_last_dxil_are_accepted(self):
        validate_container(container([(b"STAT", b"1234"), (b"DXIL", b"5678")]))

    def test_short_or_wrong_magic_rejected(self):
        good = container([(b"DXIL", b"1234")])
        for length in (0, 3, 4, 24, 28, 31):
            with self.subTest(length=length), self.assertRaises(RuntimeError):
                validate_container(good[:length])
        good[:4] = b"NOPE"
        with self.assertRaises(RuntimeError):
            validate_container(good)

    def test_declared_size_must_equal_actual_size(self):
        for difference in (-1, 1):
            blob = container([(b"DXIL", b"1234")])
            struct.pack_into("<I", blob, 24, len(blob) + difference)
            with self.subTest(difference=difference), self.assertRaises(RuntimeError):
                validate_container(blob)

    def test_chunk_table_cannot_exceed_container(self):
        blob = container([(b"DXIL", b"1234")])
        struct.pack_into("<I", blob, 28, 0xFFFFFFFF)
        with self.assertRaises(RuntimeError):
            validate_container(blob)

    def test_chunk_offsets_must_follow_table_and_fit_header(self):
        for offset in (0, 31, 32, 35, 41, 48, 0xFFFFFFFF):
            blob = container([(b"DXIL", b"1234")])
            struct.pack_into("<I", blob, 32, offset)
            with self.subTest(offset=offset), self.assertRaises(RuntimeError):
                validate_container(blob)

    def test_chunk_payload_size_cannot_exceed_remaining_bytes(self):
        for declared in (5, 0xFFFFFFFF):
            blob = container([(b"DXIL", b"1234")])
            struct.pack_into("<I", blob, 40, declared)
            with self.subTest(declared=declared), self.assertRaises(RuntimeError):
                validate_container(blob)

    def test_empty_or_non_dxil_container_rejected(self):
        for chunks in ([], [(b"STAT", b"1234")], [(b"DXBC", b"DXIL")]):
            with self.subTest(chunks=chunks), self.assertRaises(RuntimeError):
                validate_container(container(chunks))

    def test_later_malformed_chunk_not_hidden_by_dxil_tag(self):
        blob = container([(b"DXIL", b"1234"), (b"STAT", b"5678")])
        struct.pack_into("<I", blob, 36, 0xFFFFFFFF)
        with self.assertRaises(RuntimeError):
            validate_container(blob)


if __name__ == "__main__":
    unittest.main()
