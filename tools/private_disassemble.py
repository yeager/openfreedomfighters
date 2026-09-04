#!/usr/bin/env python3
"""Create a complete private linear disassembly outside the repository."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import sys

try:
    from capstone import CS_ARCH_X86, CS_MODE_32, Cs
except ImportError:  # pragma: no cover
    print(
        "error: capstone is required; install requirements-analysis.txt",
        file=sys.stderr,
    )
    raise SystemExit(2)

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from inspect_install import pe_summary


REPOSITORY_ROOT = SCRIPT_DIR.parent.resolve()


def disassemble(executable: pathlib.Path, output: pathlib.Path) -> dict[str, int]:
    executable = executable.resolve()
    output = output.resolve()
    if output.is_relative_to(REPOSITORY_ROOT):
        raise ValueError("private disassembly output must be outside the repository")
    if executable == output:
        raise ValueError("input and output paths must differ")
    output.parent.mkdir(parents=True, exist_ok=True)

    image = executable.read_bytes()
    pe = pe_summary(executable)
    if pe["bitness"] != 32 or pe["machine"] != "0x014c":
        raise ValueError("only PE32/i386 images are supported")
    section = next(
        (item for item in pe["sections"] if item["name"] == ".text"), None
    )
    if section is None:
        raise ValueError("PE image has no .text section")
    raw_offset = int(section["raw_offset"])
    raw_size = int(section["raw_size"])
    code = image[raw_offset : raw_offset + raw_size]
    if len(code) != raw_size:
        raise ValueError("truncated .text section")

    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.skipdata = True
    base = int(pe["image_base"]) + int(section["virtual_address"])
    covered = 0
    instructions = 0
    data_records = 0
    digest = hashlib.sha256(image).hexdigest()
    with output.open("w", encoding="utf-8", newline="\n") as listing:
        listing.write("; PRIVATE CLEAN-ROOM RESEARCH ARTIFACT - DO NOT COMMIT OR DISTRIBUTE\n")
        listing.write(f"; input_sha256 {digest}\n")
        listing.write(f"; text_va 0x{base:08x} raw_size {raw_size}\n")
        for instruction in decoder.disasm(code, base):
            raw = instruction.bytes.hex()
            listing.write(
                f"{instruction.address:08x}  {raw:<30}  "
                f"{instruction.mnemonic:<10} {instruction.op_str}\n"
            )
            covered += instruction.size
            if instruction.id == 0:
                data_records += 1
            else:
                instructions += 1
        listing.write(
            f"; coverage {covered}/{raw_size} instructions {instructions} "
            f"data_records {data_records}\n"
        )
    if covered != raw_size:
        raise ValueError(f"disassembly coverage incomplete: {covered}/{raw_size} bytes")
    return {
        "covered_bytes": covered,
        "text_bytes": raw_size,
        "instructions": instructions,
        "data_records": data_records,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        result = disassemble(args.executable, args.output)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(
        f"complete: {result['covered_bytes']}/{result['text_bytes']} bytes, "
        f"{result['instructions']} instructions, {result['data_records']} data records"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

