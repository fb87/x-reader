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
size_t decode_utf8(const char* text, uint32_t* codepoint);
bool compose_unicode(uint32_t first, uint32_t second, uint32_t third, uint32_t* composed,
                     size_t* consumed_codepoints);

} // namespace gfx
} // namespace xreader
