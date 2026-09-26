#include "reader.hpp"

#include <stdio.h>
#include <string.h>

#include "gfx/font.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static constexpr size_t characters_per_line = 62;
static constexpr size_t lines_per_page = 11;
static size_t page_start(const epub::document_t* document, uint8_t page)
{
    size_t offset = 0;
    for (uint8_t current = 0; current < page && offset < document->length; ++current)
    {
        size_t lines = 0;
        size_t column = 0;
        while (offset < document->length && lines < lines_per_page)
        {
            const char value = document->text[offset++];
            if (value == '\n' || ++column >= characters_per_line)
            {
                column = 0;
                ++lines;
            }
        }
    }
    return offset;
}
}

uint8_t page_count(const epub::document_t* document)
{
    if (document == nullptr || document->length == 0)
        return 1;
    size_t offset = 0;
    uint16_t pages = 0;
    while (offset < document->length && pages < 255)
    {
        size_t lines = 0;
        size_t column = 0;
        while (offset < document->length && lines < lines_per_page)
        {
            const char value = document->text[offset++];
            if (value == '\n' || ++column >= characters_per_line)
            {
                column = 0;
                ++lines;
            }
        }
        ++pages;
    }
    return static_cast<uint8_t>(pages == 0 ? 1 : pages);
}

void draw_reader(gfx::framebuffer_t* framebuffer, const epub::book_t* book,
                 const epub::document_t* document, uint8_t page, uint8_t page_count)
{
    if (framebuffer == nullptr || book == nullptr || document == nullptr)
        return;
    gfx::clear(framebuffer, 0x0f);
    gfx::draw_text(framebuffer, 24, 18, book->title, 2, 0x00);
    gfx::fill_rect(framebuffer, 24, 52, framebuffer->width - 48, 2, 0x00);
    const size_t start = page_start(document, page);
    size_t offset = start;
    uint16_t line = 0;
    uint16_t column = 0;
    while (offset < document->length && line < lines_per_page)
    {
        const char value = document->text[offset++];
        if (value == '\n')
        {
            ++line;
            column = 0;
            continue;
        }
        char glyph[2] = {value, '\0'};
        gfx::draw_text(framebuffer, static_cast<uint16_t>(28 + column * 15),
                       static_cast<uint16_t>(76 + line * 36), glyph, 2, 0x00);
        if (++column >= characters_per_line)
        {
            column = 0;
            ++line;
        }
    }
    char footer[32] = {};
    snprintf(footer, sizeof(footer), "%u / %u", static_cast<unsigned>(page + 1),
             static_cast<unsigned>(page_count));
    gfx::draw_text(framebuffer, framebuffer->width - 130, framebuffer->height - 28, footer, 1,
                   0x00);
}

} // namespace ui
} // namespace xreader
