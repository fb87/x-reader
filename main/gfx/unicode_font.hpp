#pragma once

#include <stddef.h>
#include <stdint.h>

struct unicode_glyph_t
{
    uint32_t codepoint;
    uint8_t width;
    uint8_t bitmap[32];
};

extern const unicode_glyph_t unicode_glyphs[];
extern const size_t unicode_glyph_count;
