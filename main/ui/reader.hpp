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
    uint8_t margin_mode;
    uint8_t paragraph_spacing;
    uint8_t text_alignment;
};

void draw_reader(gfx::framebuffer_t* framebuffer, const char* book_path, const epub::book_t* book,
                 const epub::document_t* document, uint8_t page, uint8_t page_count,
                 const reader_settings_t* settings);
uint8_t page_count(const epub::document_t* document, const reader_settings_t* settings,
                   uint16_t display_width, uint16_t display_height);
// Finds a UTF-8 byte substring and maps its byte offset to the cached page.
// start_offset allows callers to continue searching after a previous match.
bool find_page(const epub::document_t* document, const char* query,
               const reader_settings_t* settings, uint16_t display_width,
               uint16_t display_height, size_t start_offset, uint8_t* page,
               size_t* match_offset);

} // namespace ui
} // namespace xreader
