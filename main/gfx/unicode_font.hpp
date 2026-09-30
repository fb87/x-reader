#pragma once

#include <stddef.h>
#include <stdint.h>

// 24 rows, 4 bytes/row (28 significant columns), MSB first per byte -- the
// same packing convention icon_font.hpp uses.  Regenerate with
// tools/generate_unicode_font.py if these change.
static constexpr uint8_t unicode_glyph_height = 24;
static constexpr uint8_t unicode_glyph_bytes_per_row = 4;

struct unicode_glyph_t
{
    uint32_t codepoint;
    uint8_t width;
    uint8_t advance;
    uint8_t bitmap[unicode_glyph_height * unicode_glyph_bytes_per_row];
};

struct unicode_composition_t
{
    uint32_t first;
    uint32_t second;
    uint32_t third;
    uint32_t composed;
    uint8_t length;
};

extern const unicode_glyph_t unicode_glyphs[];
extern const size_t unicode_glyph_count;
extern const unicode_composition_t unicode_compositions[];
extern const size_t unicode_composition_count;
