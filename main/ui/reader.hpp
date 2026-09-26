#pragma once

#include "epub/book.hpp"
#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{

void draw_reader(gfx::framebuffer_t* framebuffer, const epub::book_t* book,
                 const epub::document_t* document, uint8_t page, uint8_t page_count);

} // namespace ui
} // namespace xreader
