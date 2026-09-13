"""Race-resistant JSON I/O for source-free private observation tools.

The observation tools must not accidentally follow a symlink into the source
tree or accept an unbounded special file. Callers validate the small JSON
schema separately; this module only binds a checked private pathname to an
existing directory entry and performs bounded regular-file I/O.
"""

from __future__ import annotations

import json
import os
import pathlib
import stat
from typing import Any


MAX_PRIVATE_JSON_BYTES = 4 * 1024 * 1024


def outside_repository(path: pathlib.Path, repository_root: pathlib.Path,
                       label: str) -> pathlib.Path:
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symlink")
    resolved = path.resolve()
    if resolved.is_relative_to(repository_root):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _open_parent_no_follow(path: pathlib.Path, label: str) -> tuple[int, str]:
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


def read_json(path: pathlib.Path, label: str) -> Any:
    """Read a bounded regular JSON file without following its final entry."""
    directory, name = _open_parent_no_follow(path, label)
    no_follow = getattr(os, "O_NOFOLLOW")
    try:
        try:
            descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | no_follow,
                                 dir_fd=directory)
        except OSError as error:
            raise ValueError(f"{label} must be a bounded regular private file") from error
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_PRIVATE_JSON_BYTES:
                raise ValueError(f"{label} must be a bounded regular private file")
            with os.fdopen(descriptor, "r", encoding="utf-8", closefd=False) as stream:
                return json.load(stream)
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def write_new_json(path: pathlib.Path, record: Any, label: str) -> None:
    """Create a private JSON file once, bound to its already-checked parent."""
    directory, name = _open_parent_no_follow(path, label)
    no_follow = getattr(os, "O_NOFOLLOW")
    try:
        try:
            descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | no_follow,
                                 0o600, dir_fd=directory)
        except OSError as error:
            raise ValueError(f"refusing to overwrite {label}") from error
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
                json.dump(record, stream, indent=2)
                stream.write("\n")
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)
