#pragma once

#include <stdint.h>

#include "framebuffer.hpp"

namespace xreader
{
namespace gfx
{

uint16_t draw_text(framebuffer_t* framebuffer, uint16_t x, uint16_t y, const char* text,
                   uint8_t scale, uint8_t value);

} // namespace gfx
} // namespace xreader
