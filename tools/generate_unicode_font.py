#!/usr/bin/env python3
"""Generate the bounded Smooch Sans Latin glyph table."""

import sys
import unicodedata
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


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
    image = Image.new("L", (24, 20), 0)
    draw = ImageDraw.Draw(image)
    draw.text((0, 16), chr(codepoint), font=font, fill=255, anchor="ls", stroke_width=0)
    pixels = []
    right = 1
    for y in range(20):
        row = 0
        for x in range(24):
            if image.getpixel((x, y)) >= 128:
                row |= 1 << (23 - x)
                right = max(right, x + 1)
        pixels.extend((row >> 16, (row >> 8) & 0xFF, row & 0xFF))
    advance = round(font.getlength(chr(codepoint)))
    if chr(codepoint) in "ilrt":
        advance -= 1
    return min(24, right), max(1, advance), pixels


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_unicode_font.py FONT OUTPUT")
    font = ImageFont.truetype(sys.argv[1], 20)
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
        compositions.append((values[0], values[1], values[2], len(decomposition), codepoint))
    for first, second, third, length, codepoint in sorted(compositions):
        lines.append(f"    {{{first}U, {second}U, {third}U, {codepoint}U, {length}}},")
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
