#pragma once

#include "epub/book.hpp"
#include "gfx/framebuffer.hpp"

namespace xreader::ui
{
void draw_book_info(gfx::framebuffer_t* framebuffer, const epub::book_t* book, uint8_t spine_index,
                    uint8_t spine_count);
bool book_info_back_hit(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y);
} // namespace xreader::ui
