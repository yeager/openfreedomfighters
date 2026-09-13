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

# The release job must not be able to publish a correctly named archive that
# contains a binary for a different platform. Keep this parser deliberately
# small: it reads only public executable container headers, never executes or
# extracts the program. The supported matrix is intentionally narrow.
PLATFORM_ARCHITECTURES = {
    "linux": frozenset({"x86_64"}),
    "macos": frozenset({"x86_64", "arm64"}),
    "windows": frozenset({"x86_64"}),
}

_ELF_MACHINE_ARCHITECTURES = {0x003E: "x86_64", 0x00B7: "arm64"}
_PE_MACHINE_ARCHITECTURES = {0x8664: "x86_64", 0xAA64: "arm64"}
_MACH_CPU_ARCHITECTURES = {0x01000007: "x86_64", 0x0100000C: "arm64"}


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


def _executable_bytes(archive: pathlib.Path, executable_name: str) -> bytes:
    """Read only the executable member needed for a container-header check."""
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as package:
            with package.open(executable_name) as member:
                return member.read(4096)
    with tarfile.open(archive, "r:gz") as package:
        member = package.extractfile(executable_name)
        if member is None:
            raise ValueError("release executable is not a regular archive member")
        with member:
            return member.read(4096)


def _u16_le(data: bytes, offset: int) -> int | None:
    if len(data) < offset + 2:
        return None
    return int.from_bytes(data[offset:offset + 2], "little")


def _u32(data: bytes, offset: int, byteorder: str) -> int | None:
    if len(data) < offset + 4:
        return None
    return int.from_bytes(data[offset:offset + 4], byteorder)


def _binary_architectures(binary: bytes, platform: str) -> frozenset[str]:
    """Return declared CPU architectures from a native executable header."""
    if platform == "linux":
        if len(binary) < 20 or binary[:4] != b"\x7fELF" or binary[4] != 2:
            raise ValueError("Linux release executable is not a 64-bit ELF binary")
        byteorder = {1: "little", 2: "big"}.get(binary[5])
        if byteorder is None:
            raise ValueError("Linux release executable has an invalid ELF byte order")
        machine = int.from_bytes(binary[18:20], byteorder)
        architecture = _ELF_MACHINE_ARCHITECTURES.get(machine)
        if architecture is None:
            raise ValueError("Linux release executable has an unsupported ELF machine")
        return frozenset({architecture})

    if platform == "windows":
        pe_offset = _u32(binary, 0x3C, "little")
        if (pe_offset is None or len(binary) < pe_offset + 6 or binary[:2] != b"MZ" or
                binary[pe_offset:pe_offset + 4] != b"PE\0\0"):
            raise ValueError("Windows release executable is not a PE binary")
        architecture = _PE_MACHINE_ARCHITECTURES.get(_u16_le(binary, pe_offset + 4))
        if architecture is None:
            raise ValueError("Windows release executable has an unsupported PE machine")
        return frozenset({architecture})

    # Thin Mach-O 64 headers use host byte order for cputype. Fat Mach-O
    # headers are always big-endian and enumerate each contained architecture.
    if len(binary) < 8:
        raise ValueError("macOS release executable is not a Mach-O binary")
    magic = binary[:4]
    if magic in (b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf"):
        byteorder = "little" if magic == b"\xcf\xfa\xed\xfe" else "big"
        architecture = _MACH_CPU_ARCHITECTURES.get(_u32(binary, 4, byteorder))
        if architecture is None:
            raise ValueError("macOS release executable has an unsupported Mach-O CPU")
        return frozenset({architecture})
    if magic not in (b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf"):
        raise ValueError("macOS release executable is not a Mach-O binary")
    count = _u32(binary, 4, "big")
    entry_size = 20 if magic == b"\xca\xfe\xba\xbe" else 32
    if count is None or count == 0 or len(binary) < 8 + count * entry_size:
        raise ValueError("macOS release executable has an invalid fat Mach-O header")
    architectures = {
        _MACH_CPU_ARCHITECTURES[cpu]
        for index in range(count)
        if (cpu := _u32(binary, 8 + index * entry_size, "big")) in _MACH_CPU_ARCHITECTURES
    }
    if not architectures:
        raise ValueError("macOS release executable has no supported Mach-O CPU")
    return frozenset(architectures)


def verify(archive: pathlib.Path, platform: str, architecture: str | None = None) -> None:
    if platform not in PLATFORM_ARCHITECTURES:
        raise ValueError(f"unsupported release platform: {platform}")
    if architecture is not None and architecture not in PLATFORM_ARCHITECTURES[platform]:
        raise ValueError(f"unsupported {platform} release architecture: {architecture}")
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

    binary_architectures = _binary_architectures(
        _executable_bytes(archive, executable_name), platform)
    expected_architectures = frozenset({architecture}) if architecture else PLATFORM_ARCHITECTURES[platform]
    if not binary_architectures.intersection(expected_architectures):
        expected = ", ".join(sorted(expected_architectures))
        actual = ", ".join(sorted(binary_architectures))
        raise ValueError(
            f"release executable architecture mismatch: expected {expected}; got {actual}")

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
    parser.add_argument("--architecture", choices=("x86_64", "arm64"),
                        help="require one supported native architecture in the executable")
    args = parser.parse_args()
    try:
        verify(args.archive, args.platform, args.architecture)
    except (OSError, ValueError, zipfile.BadZipFile, tarfile.TarError) as error:
        print(f"release package validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
