#include "reader.hpp"

#include <stdio.h>
#include <string.h>

#include "gfx/font.hpp"

namespace xreader
{
namespace ui
{

void draw_reader(gfx::framebuffer_t* framebuffer, const epub::book_t* book,
                 const epub::document_t* document, uint8_t page, uint8_t page_count)
{
    if (framebuffer == nullptr || book == nullptr || document == nullptr)
        return;
    gfx::clear(framebuffer, 0x0f);
    gfx::draw_text(framebuffer, 24, 18, book->title, 2, 0x00);
    gfx::fill_rect(framebuffer, 24, 52, framebuffer->width - 48, 2, 0x00);
    const size_t page_size = 900;
    const size_t start = static_cast<size_t>(page) * page_size;
    size_t length = document->length > start ? document->length - start : 0;
    if (length > page_size)
        length = page_size;
    char text[page_size + 1] = {};
    memcpy(text, document->text + start, length);
    gfx::draw_text(framebuffer, 28, 76, text, 2, 0x00);
    char footer[32] = {};
    snprintf(footer, sizeof(footer), "%u / %u", static_cast<unsigned>(page + 1),
             static_cast<unsigned>(page_count));
    gfx::draw_text(framebuffer, framebuffer->width - 130, framebuffer->height - 28, footer, 1,
                   0x00);
}

} // namespace ui
} // namespace xreader
