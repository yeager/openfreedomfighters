#!/usr/bin/env python3
"""Compile one private ID-only retail translation pack outside this repository."""
from __future__ import annotations
import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any, Mapping

REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
BINDING_FORMAT = "off.reviewed-retail-lookup-binding/v1"
SOURCE_FORMAT = "off.private-retail-translation-source/v1"
MAGIC = b"OFF-PRIVATE-L10N\0\1"
MAX_FILE_BYTES = 128 * 1024 * 1024
MAX_ENTRIES = 1_000_000
MAX_TEXT_BYTES = 1 * 1024 * 1024
IDENTIFIER = frozenset("abcdefghijklmnopqrstuvwxyz0123456789._-")
LOCALES = frozenset(("en", "sv", "da", "nb", "fi", "de", "fr", "es", "it", "pt-BR", "pl", "cs", "hu", "ro", "tr", "ru", "uk", "ja", "ko", "zh-Hans"))

def _object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result: raise ValueError("JSON object has a duplicate field")
        result[key] = value
    return result

def _exact(record: Mapping[str, Any], fields: frozenset[str], label: str) -> None:
    if frozenset(record) != fields: raise ValueError(f"{label} has an unsupported field")

def _natural(value: Any, label: str) -> int:
    if type(value) is not int or value < 0 or value > (1 << 63) - 1:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value

def _identifier(value: Any, label: str, maximum: int) -> str:
    if not isinstance(value, str) or not value or len(value) > maximum or any(c not in IDENTIFIER for c in value):
        raise ValueError(f"{label} must be a bounded opaque identifier")
    return value

def binding(record: Any) -> tuple[str, str, int, int]:
    if not isinstance(record, dict): raise ValueError("binding manifest must be a JSON object")
    _exact(record, frozenset(("format", "parser_identity", "source_set", "first_ordinal", "ordinal_count")), "binding manifest")
    if record["format"] != BINDING_FORMAT: raise ValueError("unrecognized binding manifest format")
    parser = _identifier(record["parser_identity"], "parser_identity", 128)
    source_set = _identifier(record["source_set"], "source_set", 96)
    first, count = _natural(record["first_ordinal"], "first_ordinal"), _natural(record["ordinal_count"], "ordinal_count")
    if first != 0 or count == 0 or count > MAX_ENTRIES: raise ValueError("binding manifest has an unsupported ordinal span")
    return parser, source_set, first, count

def source(record: Any, source_set: str, first: int, count: int) -> tuple[str, bool, list[tuple[str, str]]]:
    if not isinstance(record, dict): raise ValueError("translation source must be a JSON object")
    _exact(record, frozenset(("format", "locale", "complete", "entries")), "translation source")
    if record["format"] != SOURCE_FORMAT: raise ValueError("unrecognized translation source format")
    locale, complete, entries = record["locale"], record["complete"], record["entries"]
    if not isinstance(locale, str) or locale not in LOCALES: raise ValueError("translation source has an unsupported locale")
    if type(complete) is not bool: raise ValueError("translation source complete must be boolean")
    if not isinstance(entries, list) or not entries or len(entries) > MAX_ENTRIES: raise ValueError("translation source must contain bounded entries")
    prefix = f"off.retail.{source_set}."
    accepted: list[tuple[str, str]] = []
    ids: set[str] = set()
    for entry in entries:
        if not isinstance(entry, dict): raise ValueError("translation entry must be a JSON object")
        _exact(entry, frozenset(("id", "text")), "translation entry")
        identifier, text = entry["id"], entry["text"]
        if not isinstance(identifier, str) or not identifier.startswith(prefix): raise ValueError("translation entry has an ID outside its source set")
        tail = identifier[len(prefix):]
        if not tail or (len(tail) > 1 and tail[0] == "0") or not tail.isascii() or not tail.isdecimal(): raise ValueError("translation entry ID is not canonical")
        ordinal = int(tail)
        if ordinal < first or ordinal - first >= count or identifier in ids: raise ValueError("translation entry ID is duplicate or outside its source span")
        if not isinstance(text, str) or not text or "\0" in text or len(text.encode("utf-8")) > MAX_TEXT_BYTES: raise ValueError("translation entry text is invalid or too large")
        ids.add(identifier); accepted.append((identifier, text))
    if complete and len(accepted) != count: raise ValueError("complete translation source does not cover its source span")
    return locale, complete, sorted(accepted)

def _u64(output: bytearray, value: int) -> None: output.extend(value.to_bytes(8, "little"))
def _blob(output: bytearray, value: str) -> None:
    encoded = value.encode("utf-8"); _u64(output, len(encoded)); output.extend(encoded)

def build_pack(binding_record: Any, source_record: Any) -> tuple[str, bytes]:
    parser, source_set, first, count = binding(binding_record)
    locale, complete, entries = source(source_record, source_set, first, count)
    output = bytearray(MAGIC)
    for value in (parser, source_set, locale): _blob(output, value)
    _u64(output, first); _u64(output, count); output.append(1 if complete else 0); _u64(output, len(entries))
    for identifier, text in entries: _blob(output, identifier); _blob(output, text)
    if len(output) > MAX_FILE_BYTES: raise ValueError("translation package exceeds the runtime limit")
    return f"{locale}.offl10n", bytes(output)

def _outside(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink(): raise ValueError(f"{label} must not be a symlink")
    current = pathlib.Path(path.anchor)
    for component in path.parts[1:]:
        current /= component
        if current.exists() and current.is_symlink():
            raise ValueError(f"{label} must not traverse a symlink")
    result = path.resolve()
    if result.is_relative_to(REPOSITORY_ROOT): raise ValueError(f"{label} must be outside the repository")
    return result

def _parent(path: pathlib.Path, label: str) -> tuple[int, str]:
    directory, no_follow = getattr(os, "O_DIRECTORY", None), getattr(os, "O_NOFOLLOW", None)
    if directory is None or no_follow is None:
        raise ValueError(f"platform cannot safely open {label}")
    flags = directory | no_follow | os.O_RDONLY
    try: descriptor = os.open(path.parent, flags)
    except OSError as error: raise ValueError(f"{label} parent must be an existing real directory") from error
    if not stat.S_ISDIR(os.fstat(descriptor).st_mode): os.close(descriptor); raise ValueError(f"{label} parent must be an existing real directory")
    return descriptor, path.name

def _read(path: pathlib.Path, label: str) -> Any:
    directory, name = _parent(path, label)
    try:
        try: descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW, dir_fd=directory)
        except OSError as error: raise ValueError(f"{label} must be a regular private file") from error
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_FILE_BYTES: raise ValueError(f"{label} must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream: return json.load(stream, object_pairs_hook=_object)
        finally: os.close(descriptor)
    finally: os.close(directory)

def _write(path: pathlib.Path, contents: bytes) -> None:
    directory, name = _parent(path, "output")
    try:
        try: descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=directory)
        except OSError as error: raise ValueError("refusing to overwrite output") from error
        try:
            with os.fdopen(descriptor, "wb", closefd=False) as stream: stream.write(contents); stream.flush()
        finally: os.close(descriptor)
    finally: os.close(directory)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binding", type=pathlib.Path); parser.add_argument("source", type=pathlib.Path); parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        binding_path, source_path, output_path = _outside(args.binding, "binding"), _outside(args.source, "source"), _outside(args.output, "output")
        filename, package = build_pack(_read(binding_path, "binding"), _read(source_path, "source"))
        if output_path.name != filename: raise ValueError("output filename must match the canonical pack locale")
        _write(output_path, package)
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr); return 1
    print(f"wrote private {filename} translation package"); return 0

if __name__ == "__main__": raise SystemExit(main())
