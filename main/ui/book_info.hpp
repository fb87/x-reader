#pragma once

#include "epub/book.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/layout/layout.hpp"

namespace xreader::ui {
void draw_book_info(gfx::framebuffer_t* framebuffer, const epub::book_t* book,
                    uint8_t spine_index, uint8_t spine_count);
bool book_info_back_hit(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y);
layout::rect_t book_info_open_bounds(layout::viewport_t viewport);
bool book_info_open_hit(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y);
} // namespace xreader::ui
