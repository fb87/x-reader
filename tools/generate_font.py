#!/usr/bin/env python3
"""Generate dedicated small, medium, and large 8-bit-alpha font tables."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_SIZES = (("small", 19), ("medium", 22), ("large", 26))
UNICODE_RANGES = ((0x0020, 0x007E), (0x00A0, 0x00FF), (0x0100, 0x024F),
                  (0x0300, 0x036F), (0x1E00, 0x1EFF))
ICONS = {
    0xE000: [0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x24, 0x00],
    0xE001: [0x3C, 0x42, 0x99, 0xA5, 0xA5, 0x99, 0x42, 0x3C],
    0xE002: [0x18, 0x18, 0x7E, 0xDB, 0x18, 0x18, 0x3C, 0x00],
    0xE003: [0x18, 0x18, 0xFF, 0x18, 0x18, 0x00, 0x00, 0x00],
    0xE004: [0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00],
    0xE005: [0x3C, 0x42, 0x81, 0x81, 0x81, 0x42, 0x3C, 0x00],
}


def make_font(input_path, size):
    font = ImageFont.truetype(input_path, size)
    baseline = font.getmetrics()[0]
    glyphs = []
    bitmap = bytearray()
    codepoints = [code for first, last in UNICODE_RANGES for code in range(first, last + 1)]
    for codepoint in codepoints:
        character = chr(codepoint)
        left, top, right, bottom = font.getbbox(character, anchor="ls")
        width = max(0, right - left)
        height = max(0, bottom - top)
        offset = len(bitmap)
        if width and height:
            image = Image.new("L", (width, height), 0)
            ImageDraw.Draw(image).text((-left, -top), character, font=font, fill=255, anchor="ls")
            bitmap.extend(image.tobytes())
        glyphs.append((codepoint, offset, width, height, round(font.getlength(character)), left, top))

    for codepoint, rows in ICONS.items():
        offset = len(bitmap)
        glyphs.append((codepoint, offset, 8, 8, 9, 0, -baseline))
        for row in rows:
            bitmap.extend(255 if row & (1 << (7 - column)) else 0 for column in range(8))
    return baseline, glyphs, bitmap


def write_header(path):
    path.write_text(
        """#ifndef READER_FONT_H
#define READER_FONT_H

#include <stddef.h>
#include <stdint.h>

struct font_glyph {
    uint32_t codepoint;
    uint32_t bitmap_offset;
    uint16_t width;
    uint16_t height;
    uint16_t advance;
    int16_t bearing_x;
    int16_t bearing_y;
};

struct font {
    const struct font_glyph* glyphs;
    size_t glyph_count;
    const uint8_t* bitmap;
    int16_t baseline;
};

const struct font* font_default(void);
const struct font* font_for_size(unsigned size);
const struct font_glyph* font_lookup(const struct font* font, uint32_t codepoint);

#endif
""",
        encoding="ascii",
    )


def write_source(path, input_path):
    generated = [(name, make_font(input_path, size)) for name, size in FONT_SIZES]
    with path.open("w", encoding="ascii") as output:
        output.write('#include "font.h"\n\n')
        for name, (baseline, glyphs, bitmap) in generated:
            output.write(f"static const uint8_t bitmap_{name}[] = {{\n")
            for offset in range(0, len(bitmap), 16):
                values = ", ".join(f"0x{value:02x}U" for value in bitmap[offset : offset + 16])
                output.write(f"    {values},\n")
            output.write("};\n\n")
            output.write(f"static const struct font_glyph glyphs_{name}[] = {{\n")
            for glyph in glyphs:
                codepoint, offset, width, height, advance, bearing_x, bearing_y = glyph
                output.write(
                    f"    {{{codepoint}U, {offset}U, {width}U, {height}U, {advance}U, "
                    f"{bearing_x}, {bearing_y}}},\n"
                )
            output.write("};\n\n")
            output.write(
                f"static const struct font font_{name} = {{\n"
                f"    .glyphs = glyphs_{name},\n"
                f"    .glyph_count = sizeof(glyphs_{name}) / sizeof(glyphs_{name}[0]),\n"
                f"    .bitmap = bitmap_{name},\n"
                f"    .baseline = {baseline},\n"
                "};\n\n"
            )
        output.write(
            "const struct font* font_default(void) { return &font_medium; }\n\n"
            "const struct font* font_for_size(unsigned size) {\n"
            "    if (size == 1U) return &font_small;\n"
            "    if (size >= 3U) return &font_large;\n"
            "    return &font_medium;\n"
            "}\n\n"
            "const struct font_glyph* font_lookup(const struct font* font, uint32_t codepoint) {\n"
            "    size_t first = 0U;\n"
            "    size_t last = font->glyph_count;\n"
            "    while (first < last) {\n"
            "        const size_t middle = first + (last - first) / 2U;\n"
            "        if (font->glyphs[middle].codepoint == codepoint) return &font->glyphs[middle];\n"
            "        if (font->glyphs[middle].codepoint < codepoint) first = middle + 1U;\n"
            "        else last = middle;\n"
            "    }\n"
            "    return NULL;\n"
            "}\n"
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("header", type=Path)
    parser.add_argument("source", type=Path)
    arguments = parser.parse_args()
    write_header(arguments.header)
    write_source(arguments.source, arguments.input)


if __name__ == "__main__":
    main()
