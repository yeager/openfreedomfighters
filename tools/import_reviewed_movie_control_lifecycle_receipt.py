#!/usr/bin/env python3
"""Import one independently reviewed, source-free MovieControl receipt.

The input must already be the final output of
``movie_control_cutscene_lifecycle_contract_bundle.py`` from fresh external
observations.  This tool never launches, attaches to, locates, or opens a
game process, executable, archive, game-data file, log, dump, or screenshot.
It revalidates the complete categorical receipt and creates the one owner-only
file that the native inert admission loader recognizes.  ``--reviewed`` is an
explicit operator acknowledgement; it does not enable runtime playback.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

import movie_control_cutscene_lifecycle_contract_bundle as bundle


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
OUTPUT_NAME = "reviewed-movie-control-lifecycle.json"
MAX_FILE_BYTES = bundle.MAX_PRIVATE_RECORD_BYTES


def _json_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("JSON object has a duplicate field")
        result[key] = value
    return result


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    current = pathlib.Path(path.anchor)
    for component in path.parts[1:]:
        current /= component
        if current.exists() and current.is_symlink():
            raise ValueError(f"{label} must not traverse a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _open_private_parent(path: pathlib.Path, label: str) -> tuple[int, str]:
    parent = path.parent
    if parent.is_symlink() or not parent.is_dir():
        raise ValueError(f"{label} parent must be an existing private directory")
    try:
        descriptor = os.open(parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    except OSError as error:
        raise ValueError(f"{label} parent must be an existing private directory") from error
    if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise ValueError(f"{label} parent must be an existing private directory")
    return descriptor, path.name


def _read_private_json(path: pathlib.Path) -> Any:
    directory, name = _open_private_parent(path, "receipt")
    try:
        try:
            descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW,
                                 dir_fd=directory)
        except OSError as error:
            raise ValueError("receipt must be a bounded regular private file") from error
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size == 0 or metadata.st_size > MAX_FILE_BYTES:
                raise ValueError("receipt must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
                return json.load(stream, object_pairs_hook=_json_object)
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def _write_new_receipt(path: pathlib.Path, receipt: dict[str, Any]) -> None:
    directory, name = _open_private_parent(path, "output")
    try:
        try:
            descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                 0o600, dir_fd=directory)
        except OSError as error:
            raise ValueError("refusing to overwrite reviewed lifecycle receipt") from error
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
                json.dump(receipt, stream, indent=2)
                stream.write("\n")
                stream.flush()
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def import_receipt(receipt_path: pathlib.Path, output_path: pathlib.Path) -> None:
    """Validate and create one exact native-admission receipt."""
    source = _outside_repository(receipt_path, "receipt")
    output = _outside_repository(output_path, "output")
    if source == output or output.name != OUTPUT_NAME or output.exists():
        raise ValueError("receipt and a new correctly named output must be distinct")
    _write_new_receipt(output, bundle.validate_contract_receipt(_read_private_json(source)))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reviewed", action="store_true",
                        help="confirm independent review of the source-free receipt")
    parser.add_argument("receipt", type=pathlib.Path,
                        help="private final lifecycle contract bundle")
    parser.add_argument("output", type=pathlib.Path,
                        help="new reviewed-movie-control-lifecycle.json under private preferences")
    args = parser.parse_args()
    if not args.reviewed:
        parser.error("refusing to install a receipt without --reviewed")
    try:
        import_receipt(args.receipt, args.output)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("installed one inert reviewed MovieControl lifecycle receipt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
