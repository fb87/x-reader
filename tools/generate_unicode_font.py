#!/usr/bin/env python3
"""Generate a bounded 16px Vietnamese glyph table from DejaVu Sans."""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def codepoints():
    values = set(range(0x00C0, 0x0100))
    values.update((0x0102, 0x0110, 0x0128, 0x0168, 0x01A0, 0x01AF))
    values.update(range(0x1EA0, 0x1EFA))
    return sorted(values)


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
    return min(24, right), pixels


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
        width, pixels = render(font, codepoint)
        values = ", ".join(f"0x{value:02x}" for value in pixels)
        lines.append(f"    {{0x{codepoint:04x}U, {width}, {{{values}}}}},")
    lines.extend(
        [
            "};",
            "",
            "const size_t unicode_glyph_count = sizeof(unicode_glyphs) / sizeof(unicode_glyphs[0]);",
            "",
        ]
    )
    output.write_text("\n".join(lines), encoding="ascii")


if __name__ == "__main__":
    main()
