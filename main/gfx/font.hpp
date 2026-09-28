#pragma once

#include <stdint.h>

#include "framebuffer.hpp"

namespace xreader
{
namespace gfx
{

uint16_t draw_text(framebuffer_t* framebuffer, uint16_t x, uint16_t y, const char* text,
                   uint8_t scale, uint8_t value);
uint16_t measure_text(const char* text, uint8_t scale);
uint16_t glyph_advance(uint32_t codepoint, uint8_t scale);
void draw_codepoint(framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint32_t codepoint,
                    uint8_t scale, uint8_t value);
size_t decode_utf8(const char* text, uint32_t* codepoint);
bool compose_unicode(uint32_t first, uint32_t second, uint32_t third, uint32_t* composed,
                     size_t* consumed_codepoints);

} // namespace gfx
} // namespace xreader
