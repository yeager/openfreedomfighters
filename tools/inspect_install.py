#!/usr/bin/env python3
"""Create a non-extracting structural report for a Freedom Fighters install."""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import pathlib
import struct
import sys
import zipfile


PE_DIRECTORY_NAMES = (
    "exports",
    "imports",
    "resources",
    "exceptions",
    "certificates",
    "base_relocations",
    "debug",
    "architecture",
    "global_pointer",
    "tls",
    "load_config",
    "bound_imports",
    "import_address_table",
    "delay_imports",
    "clr_runtime",
    "reserved",
)


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _rva_to_offset(rva: int, sections: list[dict[str, object]]) -> int:
    for section in sections:
        start = int(section["virtual_address"])
        extent = max(int(section["virtual_size"]), int(section["raw_size"]))
        if start <= rva < start + extent:
            return int(section["raw_offset"]) + (rva - start)
    raise ValueError(f"RVA 0x{rva:x} does not map to a section")


def _cstring(image: bytes, offset: int, limit: int = 4096) -> str:
    if offset < 0 or offset >= len(image):
        raise ValueError("string offset outside image")
    end = image.find(b"\0", offset, min(len(image), offset + limit))
    if end < 0:
        raise ValueError("unterminated string")
    return image[offset:end].decode("ascii", "replace")


def pe_summary(path: pathlib.Path) -> dict[str, object]:
    image = path.read_bytes()
    with path.open("rb") as handle:
        dos = image[:64]
        if len(dos) != 64 or dos[:2] != b"MZ":
            raise ValueError("not an MZ executable")
        pe_offset = struct.unpack_from("<I", dos, 0x3C)[0]
        handle.seek(pe_offset)
        signature = handle.read(4)
        coff = handle.read(20)
        if signature != b"PE\0\0" or len(coff) != 20:
            raise ValueError("invalid PE header")
        machine, sections, timestamp, _, _, optional_size, characteristics = struct.unpack(
            "<HHIIIHH", coff
        )
        optional = handle.read(optional_size)
        if len(optional) != optional_size:
            raise ValueError("truncated optional header")
        magic = struct.unpack_from("<H", optional)[0]
        if magic not in (0x10B, 0x20B):
            raise ValueError(f"unsupported optional-header magic 0x{magic:x}")
        image_base_offset = 28 if magic == 0x10B else 24
        image_base_format = "<I" if magic == 0x10B else "<Q"
        image_base = struct.unpack_from(image_base_format, optional, image_base_offset)[0]
        entrypoint = struct.unpack_from("<I", optional, 16)[0]
        section_rows = []
        for _ in range(sections):
            row = handle.read(40)
            if len(row) != 40:
                raise ValueError("truncated section table")
            name = row[:8].split(b"\0", 1)[0].decode("ascii", "replace")
            virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
                "<IIII", row, 8
            )
            section_rows.append(
                {
                    "name": name,
                    "virtual_address": virtual_address,
                    "virtual_size": virtual_size,
                    "raw_offset": raw_offset,
                    "raw_size": raw_size,
                }
            )
    directory_offset = 96 if magic == 0x10B else 112
    directory_count_offset = 92 if magic == 0x10B else 108
    declared_directory_count = (
        struct.unpack_from("<I", optional, directory_count_offset)[0]
        if len(optional) >= directory_count_offset + 4
        else 0
    )
    available_directory_count = max(0, (len(optional) - directory_offset) // 8)
    directory_count = min(
        declared_directory_count,
        available_directory_count,
        len(PE_DIRECTORY_NAMES),
    )
    directories: dict[str, dict[str, object]] = {}
    directory_rows: list[tuple[int, int]] = []
    for index in range(directory_count):
        rva, size = struct.unpack_from("<II", optional, directory_offset + index * 8)
        directory_rows.append((rva, size))
        if rva or size:
            directories[PE_DIRECTORY_NAMES[index]] = {
                "address": rva,
                "address_kind": "file_offset" if index == 4 else "rva",
                "size": size,
            }

    exports: dict[str, object] = {
        "image_name": None,
        "function_count": 0,
        "named": [],
    }
    if directory_rows:
        export_rva, export_size = directory_rows[0]
        if export_rva and export_size >= 40:
            export_offset = _rva_to_offset(export_rva, section_rows)
            if export_offset + 40 > len(image):
                raise ValueError("truncated export directory")
            (
                _,
                _,
                _,
                _,
                image_name_rva,
                ordinal_base,
                function_count,
                name_count,
                functions_rva,
                names_rva,
                ordinals_rva,
            ) = struct.unpack_from("<IIHHIIIIIII", image, export_offset)
            if function_count > 1_000_000 or name_count > function_count:
                raise ValueError("unreasonable export counts")
            functions_offset = _rva_to_offset(functions_rva, section_rows)
            names_offset = _rva_to_offset(names_rva, section_rows)
            ordinals_offset = _rva_to_offset(ordinals_rva, section_rows)
            if functions_offset + function_count * 4 > len(image):
                raise ValueError("truncated export address table")
            if names_offset + name_count * 4 > len(image):
                raise ValueError("truncated export name table")
            if ordinals_offset + name_count * 2 > len(image):
                raise ValueError("truncated export ordinal table")
            named_exports = []
            for index in range(name_count):
                name_rva = struct.unpack_from("<I", image, names_offset + index * 4)[0]
                ordinal_index = struct.unpack_from(
                    "<H", image, ordinals_offset + index * 2
                )[0]
                if ordinal_index >= function_count:
                    raise ValueError("export ordinal exceeds function table")
                function_rva = struct.unpack_from(
                    "<I", image, functions_offset + ordinal_index * 4
                )[0]
                named_exports.append(
                    {
                        "name": _cstring(image, _rva_to_offset(name_rva, section_rows)),
                        "ordinal": ordinal_base + ordinal_index,
                        "rva": function_rva,
                    }
                )
            exports = {
                "image_name": _cstring(
                    image, _rva_to_offset(image_name_rva, section_rows)
                ),
                "function_count": function_count,
                "named": named_exports,
            }

    imports: list[dict[str, object]] = []
    if len(directory_rows) > 1:
        import_rva, import_size = directory_rows[1]
        if import_rva and import_size:
            descriptor_offset = _rva_to_offset(import_rva, section_rows)
            max_descriptors = min(import_size // 20, 1024)
            thunk_width = 4 if magic == 0x10B else 8
            thunk_format = "<I" if magic == 0x10B else "<Q"
            ordinal_mask = 0x80000000 if magic == 0x10B else 0x8000000000000000
            for index in range(max_descriptors):
                offset = descriptor_offset + index * 20
                if offset + 20 > len(image):
                    raise ValueError("truncated import descriptor")
                original_thunk, _, _, name_rva, first_thunk = struct.unpack_from(
                    "<IIIII", image, offset
                )
                if not any((original_thunk, name_rva, first_thunk)):
                    break
                dll = _cstring(image, _rva_to_offset(name_rva, section_rows))
                thunk_rva = original_thunk or first_thunk
                thunk_offset = _rva_to_offset(thunk_rva, section_rows)
                symbols = []
                for symbol_index in range(16384):
                    item_offset = thunk_offset + symbol_index * thunk_width
                    if item_offset + thunk_width > len(image):
                        raise ValueError("truncated import thunk")
                    thunk = struct.unpack_from(thunk_format, image, item_offset)[0]
                    if thunk == 0:
                        break
                    if thunk & ordinal_mask:
                        symbols.append({"ordinal": thunk & 0xFFFF})
                    else:
                        name_offset = _rva_to_offset(thunk, section_rows)
                        if name_offset + 2 > len(image):
                            raise ValueError("truncated import-by-name record")
                        hint = struct.unpack_from("<H", image, name_offset)[0]
                        symbols.append({"name": _cstring(image, name_offset + 2), "hint": hint})
                imports.append({"dll": dll, "symbols": symbols})

    tls_callbacks: list[int] = []
    if len(directory_rows) > 9:
        tls_rva, tls_size = directory_rows[9]
        minimum_tls_size = 24 if magic == 0x10B else 40
        if tls_rva and tls_size >= minimum_tls_size:
            tls_offset = _rva_to_offset(tls_rva, section_rows)
            callback_field_offset = 12 if magic == 0x10B else 24
            pointer_format = "<I" if magic == 0x10B else "<Q"
            pointer_width = 4 if magic == 0x10B else 8
            if tls_offset + callback_field_offset + pointer_width > len(image):
                raise ValueError("truncated TLS directory")
            callback_array_va = struct.unpack_from(
                pointer_format, image, tls_offset + callback_field_offset
            )[0]
            if callback_array_va:
                if callback_array_va < image_base:
                    raise ValueError("TLS callback array precedes image base")
                callback_array_offset = _rva_to_offset(
                    callback_array_va - image_base, section_rows
                )
                for index in range(256):
                    offset = callback_array_offset + index * pointer_width
                    if offset + pointer_width > len(image):
                        raise ValueError("truncated TLS callback array")
                    callback_va = struct.unpack_from(pointer_format, image, offset)[0]
                    if callback_va == 0:
                        break
                    if callback_va < image_base:
                        raise ValueError("TLS callback precedes image base")
                    tls_callbacks.append(callback_va - image_base)
                else:
                    raise ValueError("TLS callback array is not terminated")
    return {
        "machine": f"0x{machine:04x}",
        "bitness": 32 if magic == 0x10B else 64,
        "timestamp": timestamp,
        "characteristics": f"0x{characteristics:04x}",
        "entrypoint_rva": entrypoint,
        "image_base": image_base,
        "sections": section_rows,
        "imports": imports,
        "exports": exports,
        "data_directories": directories,
        "tls_callback_rvas": tls_callbacks,
    }


def inspect(root: pathlib.Path, private_paths: bool) -> dict[str, object]:
    files = sorted(path for path in root.rglob("*") if path.is_file())
    extensions: collections.Counter[str] = collections.Counter()
    zip_members: collections.Counter[str] = collections.Counter()
    zip_uncompressed = 0
    zip_member_count = 0
    total_bytes = 0
    for path in files:
        total_bytes += path.stat().st_size
        extensions[path.suffix.lower() or "[none]"] += 1
        if path.suffix.lower() == ".zip":
            try:
                with zipfile.ZipFile(path) as archive:
                    for member in archive.infolist():
                        if member.is_dir():
                            continue
                        zip_member_count += 1
                        zip_uncompressed += member.file_size
                        zip_members[pathlib.PurePosixPath(member.filename).suffix.lower() or "[none]"] += 1
            except zipfile.BadZipFile:
                zip_members["[invalid-zip]"] += 1

    executable = next((root / name for name in ("Freedom.Exe", "Freedom.exe") if (root / name).is_file()), None)
    if executable is None:
        raise FileNotFoundError("Freedom.Exe was not found at the installation root")

    report: dict[str, object] = {
        "schema": 1,
        "file_count": len(files),
        "total_bytes": total_bytes,
        "extensions": dict(sorted(extensions.items())),
        "archives": {
            "member_count": zip_member_count,
            "uncompressed_bytes": zip_uncompressed,
            "member_extensions": dict(sorted(zip_members.items())),
        },
        "executable": {
            "size": executable.stat().st_size,
            "sha256": sha256(executable),
            "pe": pe_summary(executable),
        },
    }
    if private_paths:
        report["private"] = {
            "install_root": str(root.resolve()),
            "files": [str(path.relative_to(root)) for path in files],
        }
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("install", type=pathlib.Path)
    parser.add_argument(
        "--private-paths",
        action="store_true",
        help="include local paths and filenames; never commit this output",
    )
    args = parser.parse_args()
    try:
        result = inspect(args.install.expanduser(), args.private_paths)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    json.dump(result, sys.stdout, indent=2, sort_keys=True)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
