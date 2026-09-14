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


def strict_json_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    """Build one JSON object without silently accepting duplicate fields.

    Structural observation schemas use exact field sets.  The default JSON
    decoder keeps only the last occurrence of a duplicate key, which could
    make an ambiguous observer record appear to satisfy such a schema.
    """
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("JSON object has a duplicate field")
        result[key] = value
    return result


def outside_repository(path: pathlib.Path, repository_root: pathlib.Path,
                       label: str) -> pathlib.Path:
    """Resolve a private path only after rejecting every symlink component.

    Checking just the final entry is insufficient: an observer-result path can
    cross a symlinked parent before the final ``O_NOFOLLOW`` open happens.
    Rejecting ``..`` also keeps the pre-open component walk and the later
    resolved path equivalent.
    """
    absolute = path if path.is_absolute() else pathlib.Path.cwd() / path
    if any(component == ".." for component in absolute.parts):
        raise ValueError(f"{label} must not contain parent traversal")
    if len(absolute.parts) < 2:
        raise ValueError(f"{label} must name a private file")
    # POSIX leaves the meaning of an initial ``//`` implementation-defined;
    # Linux currently treats it as ``/`` while pathlib retains ``//`` as a
    # distinct lexical anchor.  Reject it rather than allowing that mismatch
    # to bypass the lexical repository-boundary check below.
    if os.name == "posix" and absolute.anchor != "/":
        raise ValueError(f"{label} must use a standard absolute path")
    current = pathlib.Path(absolute.anchor)
    for component in absolute.parts[1:]:
        current /= component
        try:
            metadata = os.lstat(current)
        except FileNotFoundError:
            # A new final output has no entry yet. Its parent is still bound
            # descriptor-by-descriptor before O_EXCL creation below.
            continue
        if stat.S_ISLNK(metadata.st_mode):
            raise ValueError(f"{label} must not traverse a symlink")
    # Do not canonicalize the caller's path: doing so would follow a symlink
    # before the descriptor-bound open below has rejected it.  With `..`
    # forbidden, this lexical check has the same repository boundary as the
    # no-follow component walk.
    repository = repository_root.resolve()
    if absolute.is_relative_to(repository):
        raise ValueError(f"{label} must be outside the repository")
    return absolute


def _open_parent_no_follow(path: pathlib.Path, label: str) -> tuple[int, str]:
    no_follow = getattr(os, "O_NOFOLLOW", None)
    directory = getattr(os, "O_DIRECTORY", None)
    if no_follow is None or directory is None:
        raise ValueError(f"platform cannot safely open {label}")
    if not path.is_absolute() or not path.name:
        raise ValueError(f"{label} must name a private file")
    descriptor = os.open(path.anchor, os.O_RDONLY | directory | no_follow)
    try:
        # Opening only the final parent with O_NOFOLLOW leaves every earlier
        # component vulnerable to a symlink swap. Walk one directory FD at a
        # time instead, retaining no path-based trust after this point.
        for component in path.parts[1:-1]:
            try:
                child = os.open(component, os.O_RDONLY | directory | no_follow,
                                dir_fd=descriptor)
            except OSError as error:
                raise ValueError(
                    f"{label} parent must be an existing real directory") from error
            os.close(descriptor)
            descriptor = child
        if not stat.S_ISDIR(os.fstat(descriptor).st_mode):
            raise ValueError(f"{label} parent must be an existing real directory")
        return descriptor, path.name
    except Exception:
        os.close(descriptor)
        raise


def read_bytes(path: pathlib.Path, label: str) -> bytes:
    """Read a bounded regular private file without following any component."""
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
            payload = bytearray()
            while len(payload) <= MAX_PRIVATE_JSON_BYTES:
                chunk = os.read(descriptor, min(65536, MAX_PRIVATE_JSON_BYTES + 1 - len(payload)))
                if not chunk:
                    break
                payload.extend(chunk)
            if len(payload) > MAX_PRIVATE_JSON_BYTES:
                raise ValueError(f"{label} must be a bounded regular private file")
            return bytes(payload)
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)


def read_json(path: pathlib.Path, label: str) -> Any:
    """Read bounded UTF-8 JSON without following its final entry."""
    return json.loads(read_bytes(path, label).decode("utf-8"),
                      object_pairs_hook=strict_json_object)


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
            payload = (json.dumps(record, indent=2) + "\n").encode("utf-8")
            view = memoryview(payload)
            while view:
                view = view[os.write(descriptor, view):]
            os.fsync(descriptor)
        finally:
            os.close(descriptor)
    finally:
        os.close(directory)
