#pragma once

#include "epub/book.hpp"
#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{

struct reader_settings_t
{
    uint8_t text_scale;
    uint8_t line_spacing;
    uint8_t refresh_mode;
};

void draw_reader(gfx::framebuffer_t* framebuffer, const epub::book_t* book,
                 const epub::document_t* document, uint8_t page, uint8_t page_count,
                 const reader_settings_t* settings);
uint8_t page_count(const epub::document_t* document, const reader_settings_t* settings);

} // namespace ui
} // namespace xreader
