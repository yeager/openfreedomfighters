#!/usr/bin/env python3
"""Import reviewed source-free soundtrack bindings into a private receipt.

The receipt is deliberately not consumed by the native runtime yet.  This is
the offline review boundary between repeatable comparison evidence and later
audio-lifecycle work.  Hashes come only from a separate private catalog
manifest; no paths, names, samples, or retail text are accepted.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

import soundtrack_cue_evidence_bundle as bundle

REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
OUTPUT_NAME = "reviewed-soundtrack-cue-bindings.json"
OUTPUT_FORMAT = "off.reviewed-soundtrack-cue-bindings/v1"
CATALOG_FORMAT = "off.private-soundtrack-catalog-binding/v1"
MAX_FILE_BYTES = 256 * 1024


def _sha(value: Any) -> str:
    if not isinstance(value, str) or len(value) != 64 or any(c not in "0123456789abcdef" for c in value):
        raise ValueError("catalog digest must be a lowercase SHA-256")
    return value


def _catalog(value: Any) -> dict[tuple[int, str], str]:
    if not isinstance(value, dict) or frozenset(value) != frozenset(("format", "editions")):
        raise ValueError("catalog manifest has an unsupported field")
    if value["format"] != CATALOG_FORMAT or not isinstance(value["editions"], list):
        raise ValueError("catalog manifest has an unsupported format")
    result: dict[tuple[int, str], str] = {}
    for item in value["editions"]:
        if not isinstance(item, dict) or frozenset(item) != frozenset(("album_ordinal", "format", "sha256")):
            raise ValueError("catalog edition has an unsupported field")
        ordinal, format_name = item["album_ordinal"], item["format"]
        if type(ordinal) is not int or not 1 <= ordinal <= 255 or format_name not in ("flac", "mp3"):
            raise ValueError("catalog edition identity is invalid")
        key = (ordinal, format_name)
        if key in result:
            raise ValueError("catalog has a duplicate edition")
        result[key] = _sha(item["sha256"])
    return result


def build_receipt(evidence: Any, catalog_manifest: Any) -> dict[str, Any]:
    """Return a private hash-bound receipt from reviewed source-free evidence."""
    if not isinstance(evidence, dict) or evidence.get("format") != bundle.OUTPUT_FORMAT:
        raise ValueError("evidence must be a soundtrack cue review bundle")
    required = frozenset(("format", "method_version", "verified_data_manifest_fingerprint", "platform",
                          "architecture", "positive_repeat_count", "negative_run_count", "bindings",
                          "negative_candidate_count"))
    if frozenset(evidence) != required or evidence["positive_repeat_count"] != 2 or evidence["negative_run_count"] != 1:
        raise ValueError("evidence has an unsupported review shape")
    catalog = _catalog(catalog_manifest)
    entries: list[dict[str, Any]] = []
    tokens: set[int] = set()
    for item in evidence["bindings"]:
        if not isinstance(item, dict) or frozenset(item) != frozenset(("cue_token", "album_ordinal", "format")):
            raise ValueError("evidence binding has an unsupported field")
        token, ordinal, format_name = item["cue_token"], item["album_ordinal"], item["format"]
        if type(token) is not int or not 1 <= token < 1 << 64 or token in tokens:
            raise ValueError("evidence cue token is invalid")
        if (ordinal, format_name) not in catalog:
            raise ValueError("evidence binding is absent from the reviewed catalog")
        tokens.add(token)
        entries.append({"cue_token": token, "album_ordinal": ordinal, "format": format_name,
                        "expected_sha256": catalog[(ordinal, format_name)]})
    if not entries:
        raise ValueError("evidence contains no reviewed bindings")
    return {"format": OUTPUT_FORMAT, "verified_data_manifest_fingerprint": evidence["verified_data_manifest_fingerprint"],
            "bindings": entries}


def _outside(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _read(path: pathlib.Path, label: str) -> Any:
    try:
        directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        descriptor = os.open(path.name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=directory)
    except OSError as error:
        raise ValueError(f"{label} must be a regular private file") from error
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode) or os.fstat(descriptor).st_size > MAX_FILE_BYTES:
            raise ValueError(f"{label} must be a bounded regular private file")
        with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
            return json.load(stream)
    finally:
        os.close(descriptor)
        os.close(directory)


def _write(path: pathlib.Path, value: dict[str, Any]) -> None:
    directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        descriptor = os.open(path.name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600, dir_fd=directory)
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
                json.dump(value, stream, indent=2)
                stream.write("\n")
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=pathlib.Path)
    parser.add_argument("catalog", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        evidence_path, catalog_path, output_path = (_outside(args.evidence, "evidence"), _outside(args.catalog, "catalog"), _outside(args.output, "output"))
        if len({evidence_path, catalog_path, output_path}) != 3 or output_path.exists() or output_path.name != OUTPUT_NAME:
            raise ValueError("private inputs and new named output must be distinct")
        receipt = build_receipt(_read(evidence_path, "evidence"), _read(catalog_path, "catalog"))
        _write(output_path, receipt)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(receipt['bindings'])} private hash-bound soundtrack cue bindings")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
