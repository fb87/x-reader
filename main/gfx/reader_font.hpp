#pragma once

#include <stddef.h>
#include <stdint.h>

// Alegreya Regular. Sized independently from unicode_font.hpp's shared UI
// table (28 rows vs 24) for a more comfortable reading size. Regenerate with:
//   tools/generate_unicode_font.py main/gfx/Alegreya.ttf main/gfx/reader_font.cpp reader Regular 28
static constexpr uint8_t reader_glyph_height = 28;
static constexpr uint8_t reader_glyph_bytes_per_row = 4;

struct reader_glyph_t
{
    uint32_t codepoint;
    uint8_t width;
    uint8_t advance;
    uint8_t bitmap[reader_glyph_height * reader_glyph_bytes_per_row];
};

struct reader_composition_t
{
    uint32_t first;
    uint32_t second;
    uint32_t third;
    uint32_t composed;
    uint8_t length;
};

extern const reader_glyph_t reader_glyphs[];
extern const size_t reader_glyph_count;
extern const reader_composition_t reader_compositions[];
extern const size_t reader_composition_count;
