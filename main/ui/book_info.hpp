#pragma once

#include "epub/book.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/layout/layout.hpp"

namespace xreader::ui
{
// cover_pixels is a decoded 4-bpp packed grayscale buffer (2 px/byte, the same
// layout the framebuffer itself uses) at cover_pixel_width x cover_pixel_height,
// or nullptr to draw the placeholder.  The caller owns and decodes the cover
// (main.cpp, alongside the other book-loading I/O); this stays a pure drawing
// function like every other screen.
void draw_book_info(gfx::framebuffer_t* framebuffer, const epub::book_t* book, uint8_t spine_index,
                    uint8_t spine_count, const uint8_t* cover_pixels, uint16_t cover_pixel_width,
                    uint16_t cover_pixel_height, int8_t footer_focus);
bool book_info_back_hit(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y);
layout::rect_t book_info_open_bounds(layout::viewport_t viewport);
bool book_info_open_hit(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y);
} // namespace xreader::ui
