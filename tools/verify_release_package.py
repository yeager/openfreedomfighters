#!/usr/bin/env python3
"""Validate a source-free OpenFreedomFighters release archive without extracting it."""

from __future__ import annotations

import argparse
import pathlib
import sys
import tarfile
import zipfile


RETAIL_SUFFIXES = frozenset({
    ".exe", ".dll", ".zip", ".wav", ".ogg", ".whd", ".loc", ".tex",
    ".prm", ".zgf", ".gms", ".oct", ".anm", ".buf", ".rmc", ".rmi",
    ".sgp", ".snd", ".sup",
})

COMMON_FILES = frozenset({
    "README.md",
    "LICENSE",
    "THIRD_PARTY.md",
    "bin/assets/openfreedomfighters-splash.bmp",
    "bin/assets/Rajdhani-SemiBold.ttf",
})


def _valid_path(name: str) -> bool:
    path = pathlib.PurePosixPath(name)
    return bool(name) and not path.is_absolute() and ".." not in path.parts and "." not in path.parts


def _members(archive: pathlib.Path) -> tuple[set[str], set[str]]:
    """Return paths and executable paths, rejecting unsafe archive members."""
    names: set[str] = set()
    executable: set[str] = set()

    def add(name: str, is_file: bool, mode: int = 0) -> None:
        normalized = name.rstrip("/")
        # `tar -C package .` conventionally records the package root as `./`.
        # It is not a payload member and must not make a valid release fail.
        if normalized in ("", "."):
            return
        if not _valid_path(normalized):
            raise ValueError(f"unsafe archive path: {name!r}")
        if normalized in names:
            raise ValueError(f"duplicate archive path: {normalized}")
        names.add(normalized)
        if is_file and mode & 0o111:
            executable.add(normalized)

    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as package:
            for member in package.infolist():
                mode = member.external_attr >> 16
                if mode & 0o170000 and (mode & 0o170000) not in (0o100000, 0o040000):
                    raise ValueError(f"non-regular ZIP member: {member.filename!r}")
                add(member.filename, not member.is_dir(), mode)
    else:
        with tarfile.open(archive, "r:gz") as package:
            for member in package.getmembers():
                if not (member.isfile() or member.isdir()):
                    raise ValueError(f"non-regular tar member: {member.name!r}")
                add(member.name, member.isfile(), member.mode)
    return names, executable


def verify(archive: pathlib.Path, platform: str) -> None:
    names, executable = _members(archive)
    required = set(COMMON_FILES)
    executable_name = "bin/openfreedomfighters.exe" if platform == "windows" else "bin/openfreedomfighters"
    required.add(executable_name)
    if platform == "linux":
        required.add("share/applications/openfreedomfighters.desktop")

    missing = sorted(required - names)
    if missing:
        raise ValueError("missing required package files: " + ", ".join(missing))
    if platform != "windows" and executable_name not in executable:
        raise ValueError(f"installed executable is not marked executable: {executable_name}")

    allowed_suffix_paths = {executable_name}
    if platform == "windows":
        # The Windows package may contain copied third-party runtime DLLs.
        allowed_suffix_paths.update(name for name in names if name.startswith("bin/") and name.lower().endswith(".dll"))
    forbidden = sorted(name for name in names if pathlib.PurePosixPath(name).suffix.lower() in RETAIL_SUFFIXES
                       and name not in allowed_suffix_paths)
    if forbidden:
        raise ValueError("package contains prohibited retail-data extension: " + ", ".join(forbidden))

    if platform == "linux":
        with tarfile.open(archive, "r:gz") as package:
            desktop = package.extractfile("share/applications/openfreedomfighters.desktop")
            assert desktop is not None
            content = desktop.read().decode("utf-8")
        if "Exec=openfreedomfighters\n" not in content:
            raise ValueError("Linux desktop entry does not launch openfreedomfighters")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("linux", "macos", "windows"))
    parser.add_argument("archive", type=pathlib.Path)
    args = parser.parse_args()
    try:
        verify(args.archive, args.platform)
    except (OSError, ValueError, zipfile.BadZipFile, tarfile.TarError) as error:
        print(f"release package validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
