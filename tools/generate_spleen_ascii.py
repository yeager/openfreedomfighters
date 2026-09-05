#!/usr/bin/env python3
"""Generate the diagnostic U+0020..U+007E 8x16 row table from Spleen BDF."""

import argparse
from pathlib import Path


def parse(path: Path) -> dict[int, list[int]]:
    glyphs: dict[int, list[int]] = {}
    encoding: int | None = None
    bitmap: list[int] | None = None
    for line in path.read_text(encoding="ascii").splitlines():
        if line.startswith("ENCODING "):
            encoding = int(line.split()[1])
        elif line == "BITMAP":
            bitmap = []
        elif line == "ENDCHAR":
            if encoding is not None and bitmap is not None:
                glyphs[encoding] = bitmap
            encoding = None
            bitmap = None
        elif bitmap is not None:
            bitmap.append(int(line, 16))
    return glyphs


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("bdf", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    glyphs = parse(args.bdf)
    rows = []
    for codepoint in range(32, 127):
        glyph = glyphs[codepoint]
        if len(glyph) != 16 or any(value > 255 for value in glyph):
            raise ValueError(f"U+{codepoint:04X} is not an 8x16 glyph")
        rows.append("  {" + ", ".join(f"0x{x:02x}" for x in glyph) + "}")
    text = """// Generated from Spleen 2.2.0 spleen-8x16.bdf (BSD-2-Clause).
// Source BDF SHA-256: 4a3d97ee61a8c86a7525d8c723cb8a14081f395cd2feb4227ba5e3baf0629bae
#pragma once
#include <array>
#include <cstdint>
namespace off::ui::detail {
inline constexpr std::array<std::array<std::uint8_t, 16>, 95> spleen_ascii_rows{{
""" + ",\n".join(rows) + "\n}};\n} // namespace off::ui::detail\n"
    args.output.write_text(text, encoding="ascii")


if __name__ == "__main__":
    main()
