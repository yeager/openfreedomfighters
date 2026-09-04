#!/usr/bin/env python3
"""Measure PE call boundaries without emitting instructions or code bytes."""

from __future__ import annotations

import argparse
import collections
import json
import pathlib
import re
import sys

try:
    from capstone import CS_ARCH_X86, CS_MODE_32, CS_OP_IMM, CS_OP_MEM, Cs
except ImportError:  # pragma: no cover - depends on optional analysis environment
    print(
        "error: capstone is required; install requirements-analysis.txt",
        file=sys.stderr,
    )
    raise SystemExit(2)

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from inspect_install import _rva_to_offset, pe_summary


PRINTABLE_API = re.compile(r"^[A-Za-z_?][A-Za-z0-9_?$@.-]{1,127}$")


def _mapped_ascii(
    image: bytes,
    value: int,
    image_base: int,
    sections: list[dict[str, object]],
) -> str | None:
    if value < image_base:
        return None
    try:
        offset = _rva_to_offset(value - image_base, sections)
    except ValueError:
        return None
    if offset < 0 or offset >= len(image):
        return None
    end = image.find(b"\0", offset, min(offset + 129, len(image)))
    if end < 0 or end == offset:
        return None
    try:
        value_text = image[offset:end].decode("ascii")
    except UnicodeDecodeError:
        return None
    return value_text if PRINTABLE_API.fullmatch(value_text) else None


def analyze(path: pathlib.Path) -> dict[str, object]:
    pe = pe_summary(path)
    if pe["bitness"] != 32 or pe["machine"] != "0x014c":
        raise ValueError("only PE32/i386 images are supported")
    image = path.read_bytes()
    sections = pe["sections"]
    text_section = next(
        (section for section in sections if section["name"] == ".text"), None
    )
    if text_section is None:
        raise ValueError("PE image has no .text section")

    image_base = int(pe["image_base"])
    text_offset = int(text_section["raw_offset"])
    text_size = int(text_section["raw_size"])
    text_rva = int(text_section["virtual_address"])
    text = image[text_offset : text_offset + text_size]
    if len(text) != text_size:
        raise ValueError("truncated .text section")

    iat: dict[int, str] = {}
    for library in pe["imports"]:
        dll = str(library["dll"])
        for symbol in library["symbols"]:
            name = symbol.get("name") or symbol.get("resolved_name")
            if name is None:
                name = f"ordinal_{symbol['ordinal']}"
            iat[image_base + int(symbol["iat_rva"])] = f"{dll}!{name}"

    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    decoder.skipdata = True
    import_references: collections.Counter[str] = collections.Counter()
    direct_call_targets: collections.Counter[int] = collections.Counter()
    dynamic_candidates: dict[str, collections.Counter[str]] = collections.defaultdict(
        collections.Counter
    )
    recent = collections.deque(maxlen=12)
    instruction_count = 0
    undecodable_bytes = 0
    for instruction in decoder.disasm(text, image_base + text_rva):
        if instruction.id == 0:
            undecodable_bytes += instruction.size
            recent.clear()
            continue
        instruction_count += 1
        referenced_imports = []
        for operand in instruction.operands:
            if operand.type == CS_OP_MEM and operand.mem.base == 0 and operand.mem.index == 0:
                absolute = operand.mem.disp & 0xFFFFFFFF
                imported = iat.get(absolute)
                if imported is not None:
                    import_references[imported] += 1
                    referenced_imports.append(imported)
        if instruction.mnemonic == "call" and instruction.operands:
            operand = instruction.operands[0]
            if operand.type == CS_OP_IMM:
                direct_call_targets[operand.imm & 0xFFFFFFFF] += 1

        control_transfer_imports = (
            referenced_imports if instruction.mnemonic in ("call", "jmp") else []
        )
        for imported in control_transfer_imports:
            api = imported.rsplit("!", 1)[-1]
            if api not in ("LoadLibraryA", "LoadLibraryW", "GetProcAddress"):
                continue
            for previous in reversed(recent):
                if previous.mnemonic != "push" or not previous.operands:
                    continue
                operand = previous.operands[0]
                if operand.type != CS_OP_IMM:
                    continue
                candidate = _mapped_ascii(
                    image,
                    operand.imm & 0xFFFFFFFF,
                    image_base,
                    sections,
                )
                if candidate is not None:
                    dynamic_candidates[api][candidate] += 1
        recent.append(instruction)

    text_start = image_base + text_rva
    text_end = text_start + int(text_section["virtual_size"])
    internal_targets = {
        address: count
        for address, count in direct_call_targets.items()
        if text_start <= address < text_end
    }
    return {
        "schema": 1,
        "method": "linear_sweep_x86_32",
        "instruction_count": instruction_count,
        "undecodable_bytes": undecodable_bytes,
        "direct_internal_call_sites": sum(internal_targets.values()),
        "unique_direct_internal_targets": len(internal_targets),
        "import_references": dict(sorted(import_references.items())),
        "dynamic_api_nearby_string_candidates": {
            api: dict(sorted(candidates.items()))
            for api, candidates in sorted(dynamic_candidates.items())
        },
        "caveats": [
            "Linear sweep may decode embedded data as instructions.",
            "Import references include non-call reads and jumps.",
            "Nearby strings are candidates, not proven call arguments.",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    args = parser.parse_args()
    try:
        result = analyze(args.executable.expanduser())
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    json.dump(result, sys.stdout, indent=2, sort_keys=True)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
