#pragma once

#include "epub/book.hpp"
#include "gfx/framebuffer.hpp"

namespace xreader::ui
{
void draw_contents(gfx::framebuffer_t* framebuffer, const epub::book_t* book, uint8_t focus);
bool contents_touch_index(uint16_t width, uint16_t height, uint16_t x, uint16_t y, uint8_t count,
                          uint8_t* index);
} // namespace xreader::ui
