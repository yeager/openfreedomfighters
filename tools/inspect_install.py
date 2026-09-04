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


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def pe_summary(path: pathlib.Path) -> dict[str, object]:
    with path.open("rb") as handle:
        dos = handle.read(64)
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
    return {
        "machine": f"0x{machine:04x}",
        "bitness": 32 if magic == 0x10B else 64,
        "timestamp": timestamp,
        "characteristics": f"0x{characteristics:04x}",
        "entrypoint_rva": entrypoint,
        "image_base": image_base,
        "sections": section_rows,
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

