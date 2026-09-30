#!/usr/bin/env python3
"""Generate the bounded Smooch Sans Latin glyph table."""

import sys
import unicodedata
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# Byte-aligned per-row (MSB first per byte), the same packing convention
# icon_font.hpp already uses. Height is parametric (see main()) so the reader
# table can use a larger, independent size from the shared UI table.


def codepoints():
    ranges = (
        (0x0020, 0x007E),
        (0x00A0, 0x00FF),
        (0x0100, 0x017F),
        (0x0180, 0x024F),
        (0x0300, 0x036F),
        (0x1E00, 0x1EFF),
    )
    return [codepoint for first, last in ranges for codepoint in range(first, last + 1)]


def render(font, codepoint, glyph_height, glyph_width, bytes_per_row):
    image = Image.new("L", (glyph_width, glyph_height), 0)
    draw = ImageDraw.Draw(image)
    # Baseline kept at the same 80% line-height fraction earlier tables used
    # (16/20), so descenders get proportionally the same headroom.
    baseline = round(glyph_height * 16 / 20)
    draw.text((0, baseline), chr(codepoint), font=font, fill=255, anchor="ls", stroke_width=0)
    rows = []
    right = 1
    for y in range(glyph_height):
        row_bytes = [0] * bytes_per_row
        for x in range(glyph_width):
            if image.getpixel((x, y)) >= 128:
                row_bytes[x // 8] |= 1 << (7 - (x % 8))
                right = max(right, x + 1)
        rows.extend(row_bytes)
    advance = round(font.getlength(chr(codepoint)))
    if chr(codepoint) in "ilrt":
        # Tighten narrow-stem glyphs, but never past their own ink -- doing so
        # made the next glyph overlap "i"/"t"'s rightmost column.
        advance = max(advance - 1, right)
    return min(glyph_width, right), max(1, advance), rows


def main():
    if len(sys.argv) < 3 or len(sys.argv) > 6:
        raise SystemExit(
            "usage: generate_unicode_font.py FONT OUTPUT [PREFIX] [WEIGHT] [HEIGHT]"
        )
    # PREFIX names the emitted C arrays (default "unicode", matching the shared
    # UI table). A second table -- e.g. "reader" for the reader body font --
    # gets its own struct type and its own glyph HEIGHT (default 24, same as
    # the UI table), so the reader can use a larger, independent glyph size.
    prefix = sys.argv[3] if len(sys.argv) > 3 else "unicode"
    weight = sys.argv[4] if len(sys.argv) > 4 else "Bold"
    glyph_height = int(sys.argv[5]) if len(sys.argv) > 5 else 24
    glyph_width = glyph_height + 4
    bytes_per_row = (glyph_width + 7) // 8
    font = ImageFont.truetype(sys.argv[1], glyph_height)
    try:
        font.set_variation_by_name(weight)
    except AttributeError:
        pass
    output = Path(sys.argv[2])
    glyph_type = "unicode_glyph_t" if prefix == "unicode" else f"{prefix}_glyph_t"
    composition_type = (
        "unicode_composition_t" if prefix == "unicode" else f"{prefix}_composition_t"
    )
    lines = [
        "#include <stdint.h>",
        f"#include \"{prefix}_font.hpp\"",
        "",
        f"const {glyph_type} {prefix}_glyphs[] = {{",
    ]
    for codepoint in codepoints():
        width, advance, pixels = render(font, codepoint, glyph_height, glyph_width, bytes_per_row)
        values = ", ".join(f"0x{value:02x}" for value in pixels)
        lines.append(f"    {{0x{codepoint:04x}U, {width}, {advance}, {{{values}}}}},")
    lines.extend(
        [
            "};",
            "",
            f"const size_t {prefix}_glyph_count = sizeof({prefix}_glyphs) / sizeof({prefix}_glyphs[0]);",
            "",
            f"const {composition_type} {prefix}_compositions[] = {{",
        ]
    )
    compositions = []
    for codepoint in codepoints():
        decomposition = unicodedata.normalize("NFD", chr(codepoint))
        if len(decomposition) not in (2, 3):
            continue
        values = [ord(value) for value in decomposition]
        values.append(0)
        compositions.append(
            (values[0], values[1], values[2], len(decomposition), codepoint)
        )
    # Runtime lookup uses binary search, so keep the table sorted by its lookup key.
    compositions.sort(key=lambda item: item[:4])
    for first, second, third, length, codepoint in compositions:
        lines.append(
            f"    {{{first}U, {second}U, {third}U, {codepoint}U, {length}}},"
        )
    lines.extend(
        [
            "};",
            "",
            f"const size_t {prefix}_composition_count = "
            f"sizeof({prefix}_compositions) / sizeof({prefix}_compositions[0]);",
            "",
        ]
    )
    output.write_text("\n".join(lines), encoding="ascii")


if __name__ == "__main__":
    main()
