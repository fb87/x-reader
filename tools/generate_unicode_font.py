#!/usr/bin/env python3
"""Generate the bounded Smooch Sans Latin glyph table."""

import sys
import unicodedata
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# +20% over the original 20px render (matches the same "a bit bigger" bump
# applied to the footer height elsewhere): both the UI chrome text and the
# reader's "Small" text-size option share this one table, so raising it here
# raises both at once. Byte-aligned per-row (MSB first per byte), the same
# packing convention icon_font.hpp already uses.
GLYPH_HEIGHT = 24
GLYPH_WIDTH = 28
BYTES_PER_ROW = (GLYPH_WIDTH + 7) // 8


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


def render(font, codepoint):
    image = Image.new("L", (GLYPH_WIDTH, GLYPH_HEIGHT), 0)
    draw = ImageDraw.Draw(image)
    # Baseline kept at the same 80% line-height fraction the 20px table used
    # (16/20), so descenders get proportionally the same headroom.
    baseline = round(GLYPH_HEIGHT * 16 / 20)
    draw.text((0, baseline), chr(codepoint), font=font, fill=255, anchor="ls", stroke_width=0)
    rows = []
    right = 1
    for y in range(GLYPH_HEIGHT):
        row_bytes = [0] * BYTES_PER_ROW
        for x in range(GLYPH_WIDTH):
            if image.getpixel((x, y)) >= 128:
                row_bytes[x // 8] |= 1 << (7 - (x % 8))
                right = max(right, x + 1)
        rows.extend(row_bytes)
    advance = round(font.getlength(chr(codepoint)))
    if chr(codepoint) in "ilrt":
        # Tighten narrow-stem glyphs, but never past their own ink -- doing so
        # made the next glyph overlap "i"/"t"'s rightmost column.
        advance = max(advance - 1, right)
    return min(GLYPH_WIDTH, right), max(1, advance), rows


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_unicode_font.py FONT OUTPUT")
    font = ImageFont.truetype(sys.argv[1], GLYPH_HEIGHT)
    try:
        font.set_variation_by_name("Bold")
    except AttributeError:
        pass
    output = Path(sys.argv[2])
    lines = [
        "#include <stdint.h>",
        "#include \"unicode_font.hpp\"",
        "",
        "const unicode_glyph_t unicode_glyphs[] = {",
    ]
    for codepoint in codepoints():
        width, advance, pixels = render(font, codepoint)
        values = ", ".join(f"0x{value:02x}" for value in pixels)
        lines.append(f"    {{0x{codepoint:04x}U, {width}, {advance}, {{{values}}}}},")
    lines.extend(
        [
            "};",
            "",
            "const size_t unicode_glyph_count = sizeof(unicode_glyphs) / sizeof(unicode_glyphs[0]);",
            "",
            "const unicode_composition_t unicode_compositions[] = {",
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
            "const size_t unicode_composition_count = "
            "sizeof(unicode_compositions) / sizeof(unicode_compositions[0]);",
            "",
        ]
    )
    output.write_text("\n".join(lines), encoding="ascii")


if __name__ == "__main__":
    main()
