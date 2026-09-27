#include "reader.hpp"

#include <stdio.h>
#include <string.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static constexpr size_t characters_per_line = 56;
static constexpr size_t lines_per_page = 14;
static size_t page_start(const epub::document_t* document, uint8_t page,
                         const reader_settings_t* settings)
{
    const uint8_t scale = settings != nullptr && settings->text_scale == 1 ? 1 : 2;
    const size_t columns = scale == 1 ? 112 : characters_per_line;
    const size_t lines_per_screen = scale == 1 ? 20 : lines_per_page;
    size_t offset = 0;
    for (uint8_t current = 0; current < page && offset < document->length; ++current)
    {
        size_t lines = 0;
        size_t column = 0;
        while (offset < document->length && lines < lines_per_screen)
        {
            const char value = document->text[offset++];
            if (value == '\n' || ++column >= columns)
            {
                column = 0;
                ++lines;
            }
        }
    }
    return offset;
}
} // namespace

uint8_t page_count(const epub::document_t* document, const reader_settings_t* settings)
{
    if (document == nullptr || document->length == 0)
        return 1;
    const uint8_t scale = settings != nullptr && settings->text_scale == 1 ? 1 : 2;
    const size_t columns = scale == 1 ? 112 : characters_per_line;
    const size_t lines_per_screen = scale == 1 ? 20 : lines_per_page;
    size_t offset = 0;
    uint16_t pages = 0;
    while (offset < document->length && pages < 255)
    {
        size_t lines = 0;
        size_t column = 0;
        while (offset < document->length && lines < lines_per_screen)
        {
            const char value = document->text[offset++];
            if (value == '\n' || ++column >= columns)
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
                 const epub::document_t* document, uint8_t page, uint8_t page_count,
                 const reader_settings_t* settings)
{
    if (framebuffer == nullptr || book == nullptr || document == nullptr)
        return;
    const uint8_t scale = settings != nullptr && settings->text_scale == 1 ? 1 : 2;
    const uint16_t line_step = settings != nullptr && settings->line_spacing != 0
                                   ? static_cast<uint16_t>(scale == 1 ? 24 : 32)
                                   : static_cast<uint16_t>(scale == 1 ? 20 : 28);
    const size_t columns = scale == 1 ? 112 : characters_per_line;
    const size_t lines_per_screen = scale == 1 ? 20 : lines_per_page;
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, book->title, "READING");
    const size_t start = page_start(document, page, settings);
    size_t offset = start;
    uint16_t line = 0;
    uint16_t column = 0;
    while (offset < document->length && line < lines_per_screen)
    {
        const char value = document->text[offset++];
        if (value == '\n')
        {
            ++line;
            column = 0;
            continue;
        }
        char glyph[2] = {value, '\0'};
        gfx::draw_text(framebuffer, static_cast<uint16_t>(28 + column * (scale == 1 ? 9 : 16)),
                       static_cast<uint16_t>(58 + line * line_step), glyph, scale, 0x00);
        if (++column >= columns)
        {
            column = 0;
            ++line;
        }
    }
    char footer[32] = {};
    snprintf(footer, sizeof(footer), "%u / %u", static_cast<unsigned>(page + 1),
             static_cast<unsigned>(page_count));
    chrome::draw_indication_bar(framebuffer, "< PREV", footer, "NEXT >");
}

} // namespace ui
} // namespace xreader
