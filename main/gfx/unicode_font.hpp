#pragma once

#include <stddef.h>
#include <stdint.h>

struct unicode_glyph_t
{
    uint32_t codepoint;
    uint8_t width;
    uint8_t advance;
    uint8_t bitmap[60];
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
