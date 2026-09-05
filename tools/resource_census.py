#!/usr/bin/env python3
"""Report aggregate resource structure without extracting retail assets."""

from __future__ import annotations

import argparse
import collections
import json
import pathlib
import statistics
import struct
import sys
import zipfile


PREFIX_BYTES = 32


def _words(prefix: bytes) -> list[int]:
    count = len(prefix) // 4
    return list(struct.unpack("<" + "I" * count, prefix[: count * 4]))


def _known_invariants(extension: str, size: int, prefix: bytes) -> dict[str, bool]:
    words = _words(prefix)
    if extension == ".anm" and len(words) >= 3:
        return {
            "magic_is_MNA_nul": words[0] == 0x00414E4D,
            "word_2_is_file_size": words[2] == size,
        }
    if extension == ".gms" and len(words) >= 2:
        return {"word_1_is_file_size": words[1] == size}
    if extension == ".prm" and len(words) >= 3:
        return {
            "word_0_is_76476": words[0] == 76476,
            "words_1_and_2_match": words[1] == words[2],
        }
    if extension == ".snd" and len(words) >= 4:
        return {
            "word_0_is_file_size_minus_48": words[0] == size - 48,
            "word_1_is_file_size": words[1] == size,
            "words_2_and_3_are_3_4": words[2:4] == [3, 4],
        }
    if extension == ".sup" and len(words) >= 3:
        invariants = {
            "word_0_is_zero": words[0] == 0,
            "word_1_is_flagged_file_size": words[1] == (size | 0x80000000),
            "word_2_is_file_size": words[2] == size,
        }
        if len(words) >= 6:
            invariants.update(
                {
                    "word_3_is_one": words[3] == 1,
                    "word_4_is_DLCF": words[4] == 0x46434C44,
                    "DLCF_descriptor_size_matches":
                        (words[5] & 0x3FFFFFFF) == size - 16,
                    "DLCF_layout_is_scalar_or_array":
                        (words[5] & 0xC0000000) in (0, 0x40000000),
                }
            )
        return invariants
    if extension == ".tex" and len(words) >= 4:
        return {
            "word_0_is_file_size_minus_16384": words[0] == size - 16384,
            "word_1_is_normal_or_empty_variant": words[1] in (words[0], words[0] + 8192),
            "words_2_and_3_are_3_4": words[2:4] == [3, 4],
        }
    if extension == ".zgf" and len(words) >= 2:
        return {"word_1_is_file_size": words[1] == size}
    return {}


def common_prefix(values: list[bytes]) -> bytes:
    if not values:
        return b""
    result = bytearray()
    for column in zip(*values):
        if len(set(column)) != 1:
            break
        result.append(column[0])
    return bytes(result)


def census(root: pathlib.Path) -> dict[str, object]:
    archives = sorted(root.rglob("*.zip")) + sorted(root.rglob("*.ZIP"))
    # Case-sensitive filesystems may return a path twice only when it somehow
    # matches both patterns; preserve deterministic order while de-duplicating.
    archives = list(dict.fromkeys(archives))
    formats: dict[str, list[tuple[int, bytes, int]]] = collections.defaultdict(list)
    invalid_archives = 0
    for archive_path in archives:
        try:
            with zipfile.ZipFile(archive_path) as archive:
                for member in archive.infolist():
                    if member.is_dir():
                        continue
                    extension = pathlib.PurePosixPath(member.filename).suffix.lower() or "[none]"
                    with archive.open(member, "r") as handle:
                        prefix = handle.read(PREFIX_BYTES)
                    formats[extension].append(
                        (member.file_size, prefix, member.compress_type)
                    )
        except (OSError, zipfile.BadZipFile):
            invalid_archives += 1

    result_formats: dict[str, object] = {}
    for extension, rows in sorted(formats.items()):
        sizes = [row[0] for row in rows]
        prefixes = [row[1] for row in rows]
        first_u32 = collections.Counter(
            struct.unpack_from("<I", prefix)[0]
            for prefix in prefixes
            if len(prefix) >= 4
        )
        compression = collections.Counter(row[2] for row in rows)
        invariant_counts: dict[str, collections.Counter[bool]] = collections.defaultdict(
            collections.Counter
        )
        for size, prefix, _ in rows:
            for name, passed in _known_invariants(extension, size, prefix).items():
                invariant_counts[name][passed] += 1
        result_formats[extension] = {
            "count": len(rows),
            "size": {
                "min": min(sizes),
                "median": int(statistics.median(sizes)),
                "max": max(sizes),
            },
            "common_prefix_hex": common_prefix(prefixes).hex(),
            "unique_prefixes_16": len({prefix[:16] for prefix in prefixes}),
            "first_u32_le_top": [
                {"value": value, "count": count}
                for value, count in first_u32.most_common(8)
            ],
            "zip_compression_methods": {
                str(method): count for method, count in sorted(compression.items())
            },
            "known_invariants": {
                name: {"passed": counts[True], "failed": counts[False]}
                for name, counts in sorted(invariant_counts.items())
            },
        }

    return {
        "schema": 1,
        "prefix_bytes_read_per_member": PREFIX_BYTES,
        "archive_count": len(archives),
        "invalid_archive_count": invalid_archives,
        "member_count": sum(len(rows) for rows in formats.values()),
        "formats": result_formats,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("install", type=pathlib.Path)
    args = parser.parse_args()
    try:
        result = census(args.install.expanduser())
    except OSError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    json.dump(result, sys.stdout, indent=2, sort_keys=True)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
