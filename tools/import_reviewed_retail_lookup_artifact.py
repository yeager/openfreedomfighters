#!/usr/bin/env python3
"""Create one private reviewed-retail lookup artifact from a sanitized trace.

This operator-side tool accepts only the source-free JSON produced by
``retail_localization_lookup_trace.py``.  It never opens game data, binaries,
screenshots, logs, or dumps.  The output is the exact bounded binary format
loaded by OpenFreedomFighters, and must remain outside this repository.

Each imported event must be a distinct resolved, unformatted catalog lookup.
The observer-assigned lookup site and opaque catalog ordinal are both unique.
The parser identity, source-set identity, and ordinal span are supplied
explicitly or read from a separate source-free binding manifest.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any, Mapping


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
TRACE_FORMAT = "off.retail-localization-lookup/v1"
BINDING_FORMAT = "off.reviewed-retail-lookup-binding/v1"
MAGIC = b"OFF-REVIEWED-RETAIL-LOOKUP\0\1"
OUTPUT_FILENAME = "reviewed-retail-lookup.offlookup"
MAX_FILE_BYTES = 512 * 1024
MAX_EVENTS = 4096
MAX_ORDINAL = (1 << 63) - 1

_IDENTIFIER_CHARACTERS = frozenset("abcdefghijklmnopqrstuvwxyz0123456789._-")
_TRACE_FIELDS = frozenset(("format", "events"))
_EVENT_FIELDS = frozenset((
    "observation_order", "call_ordinal", "lookup_site", "lookup_key_relation",
    "catalog_ordinal", "lookup_outcome", "result_kind",
    "format_argument_kinds",
))
_BINDING_FIELDS = frozenset((
    "format", "parser_identity", "source_set", "first_ordinal", "ordinal_count",
))


def _json_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("JSON object has a duplicate field")
        result[key] = value
    return result


def _exact(record: Mapping[str, Any], expected: frozenset[str], label: str) -> None:
    if frozenset(record) != expected:
        raise ValueError(f"{label} has an unsupported field")


def _natural(value: Any, label: str, maximum: int = MAX_ORDINAL) -> int:
    if type(value) is not int or value < 0 or value > maximum:
        raise ValueError(f"{label} must be an unsigned bounded integer")
    return value


def _identifier(value: Any, label: str, maximum: int) -> str:
    if (not isinstance(value, str) or not value or len(value) > maximum or
            any(character not in _IDENTIFIER_CHARACTERS for character in value)):
        raise ValueError(f"{label} must be a bounded opaque identifier")
    return value


def _binding(record: Mapping[str, Any]) -> tuple[str, str, int, int]:
    _exact(record, _BINDING_FIELDS, "binding manifest")
    if record["format"] != BINDING_FORMAT:
        raise ValueError("unrecognized binding manifest format")
    parser = _identifier(record["parser_identity"], "parser_identity", 128)
    source_set = _identifier(record["source_set"], "source_set", 96)
    first = _natural(record["first_ordinal"], "first_ordinal")
    count = _natural(record["ordinal_count"], "ordinal_count")
    if first != 0 or count == 0 or count > MAX_ORDINAL + 1:
        raise ValueError("binding manifest has an unsupported ordinal span")
    return parser, source_set, first, count


def binding_from_values(parser: Any, source_set: Any, ordinal_count: Any) -> tuple[str, str, int, int]:
    """Validate explicit source-free binding values for the runtime loader."""
    return _binding({"format": BINDING_FORMAT, "parser_identity": parser,
                     "source_set": source_set, "first_ordinal": 0,
                     "ordinal_count": ordinal_count})


def _trace_observations(trace: Any, binding: tuple[str, str, int, int]) -> list[tuple[str, int]]:
    if not isinstance(trace, dict):
        raise ValueError("trace must be a JSON object")
    _exact(trace, _TRACE_FIELDS, "trace")
    if trace["format"] != TRACE_FORMAT:
        raise ValueError("unrecognized sanitized trace format")
    events = trace["events"]
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("trace must contain between one and 4096 events")
    _, _, first, count = binding
    sites: set[str] = set()
    ordinals: set[int] = set()
    observations: list[tuple[str, int]] = []
    prior_order = -1
    prior_call = -1
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("trace event must be a JSON object")
        _exact(event, _EVENT_FIELDS, "trace event")
        order = _natural(event["observation_order"], "observation_order", MAX_EVENTS)
        call = _natural(event["call_ordinal"], "call_ordinal", MAX_EVENTS)
        if order <= prior_order or call <= prior_call:
            raise ValueError("trace observation and call ordinals must be strictly increasing")
        prior_order, prior_call = order, call
        if (event["lookup_outcome"] != "resolved" or
                event["result_kind"] != "catalog-value" or
                event["format_argument_kinds"] != []):
            raise ValueError("trace contains an unresolved or formatted lookup")
        site = _identifier(event["lookup_site"], "lookup_site", 128)
        ordinal = _natural(event["catalog_ordinal"], "catalog_ordinal")
        if ordinal < first or ordinal - first >= count:
            raise ValueError("trace catalog ordinal is outside the bound source span")
        if site in sites:
            raise ValueError("trace contains a duplicate lookup site")
        if ordinal in ordinals:
            raise ValueError("trace contains a duplicate catalog ordinal")
        sites.add(site)
        ordinals.add(ordinal)
        observations.append((site, ordinal))
    return observations


def _append_u64(output: bytearray, value: int) -> None:
    output.extend(value.to_bytes(8, "little", signed=False))


def _append_blob(output: bytearray, value: str) -> None:
    encoded = value.encode("ascii")
    _append_u64(output, len(encoded))
    output.extend(encoded)


def build_artifact(trace: Any, binding: tuple[str, str, int, int]) -> bytes:
    """Validate source-free input and return the runtime artifact bytes."""
    parser, source_set, first, count = binding
    observations = _trace_observations(trace, binding)
    output = bytearray(MAGIC)
    _append_blob(output, parser)
    _append_blob(output, source_set)
    _append_u64(output, first)
    _append_u64(output, count)
    _append_u64(output, len(observations))
    for site, ordinal in observations:
        _append_blob(output, site)
        _append_u64(output, ordinal)
    return bytes(output)


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _real_path_components(path: pathlib.Path, label: str) -> None:
    current = pathlib.Path(path.anchor)
    for component in path.parts[1:]:
        current /= component
        if current.exists() and current.is_symlink():
            raise ValueError(f"{label} must not traverse a symlink")


def _open_parent_no_follow(path: pathlib.Path, label: str) -> tuple[int, str]:
    _real_path_components(path.parent, label)
    no_follow = getattr(os, "O_NOFOLLOW", None)
    directory = getattr(os, "O_DIRECTORY", None)
    if no_follow is None or directory is None:
        raise ValueError(f"platform cannot safely open {label}")
    try:
        descriptor = os.open(path.parent, os.O_RDONLY | directory | no_follow)
    except OSError as error:
        raise ValueError(f"{label} parent must be an existing real directory") from error
    if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise ValueError(f"{label} parent must be an existing real directory")
    return descriptor, path.name


def _read_private_json(path: pathlib.Path, label: str) -> object:
    directory, name = _open_parent_no_follow(path, label)
    try:
        try:
            descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW,
                                 dir_fd=directory)
        except OSError as error:
            raise ValueError(f"{label} must be a regular private file") from error
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_FILE_BYTES:
                raise ValueError(f"{label} must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
                return json.load(stream, object_pairs_hook=_json_object)
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def _write_new_private_binary(path: pathlib.Path, artifact: bytes) -> None:
    directory, name = _open_parent_no_follow(path, "output")
    try:
        try:
            descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                 0o600, dir_fd=directory)
        except OSError as error:
            raise ValueError("refusing to overwrite output") from error
        try:
            with os.fdopen(descriptor, "wb", closefd=False) as stream:
                stream.write(artifact)
                stream.flush()
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=pathlib.Path, help="sanitized private source-free trace JSON")
    parser.add_argument("output", type=pathlib.Path, help="new private .offlookup file")
    binding_group = parser.add_mutually_exclusive_group(required=True)
    binding_group.add_argument("--binding-manifest", type=pathlib.Path,
                               help="source-free private binding JSON")
    binding_group.add_argument("--parser-identity", help="reviewed parser identity")
    parser.add_argument("--source-set", help="reviewed source-set identity (with --parser-identity)")
    parser.add_argument("--ordinal-count", type=int,
                        help="private catalog ordinal count (with --parser-identity)")
    args = parser.parse_args()
    try:
        trace_path = _outside_repository(args.trace, "trace")
        output_path = _outside_repository(args.output, "output")
        if trace_path == output_path:
            raise ValueError("trace and output paths must differ")
        if output_path.name != OUTPUT_FILENAME:
            raise ValueError(f"output must be named {OUTPUT_FILENAME}")
        trace = _read_private_json(trace_path, "trace")
        if args.binding_manifest is not None:
            manifest_path = _outside_repository(args.binding_manifest, "binding manifest")
            if manifest_path in (trace_path, output_path):
                raise ValueError("binding manifest must be a distinct path")
            binding = _binding(_read_private_json(manifest_path, "binding manifest"))
        else:
            if args.source_set is None or args.ordinal_count is None:
                raise ValueError("explicit binding requires --source-set and --ordinal-count")
            binding = binding_from_values(args.parser_identity, args.source_set, args.ordinal_count)
        artifact = build_artifact(trace, binding)
        _write_new_private_binary(output_path, artifact)
    except (OSError, ValueError, UnicodeError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {len(artifact)} source-free reviewed retail lookup bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
