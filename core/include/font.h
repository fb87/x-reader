#ifndef READER_FONT_H
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
