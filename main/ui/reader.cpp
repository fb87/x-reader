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
static constexpr uint16_t text_left = 28;
static constexpr uint16_t text_width = 904;
static constexpr uint16_t display_height = 540;
static constexpr uint16_t minimum_vertical_margin = 16;
struct reader_layout_t
{
    uint8_t scale;
    uint16_t line_step;
    uint16_t text_top;
    size_t lines_per_screen;
};
static reader_layout_t reader_layout(const reader_settings_t* settings)
{
    reader_layout_t layout = {};
    layout.scale = settings != nullptr && settings->text_scale == 1 ? 1 : 2;
    layout.line_step = settings != nullptr && settings->line_spacing != 0
                           ? static_cast<uint16_t>(layout.scale == 1 ? 24 : 48)
                           : static_cast<uint16_t>(layout.scale == 1 ? 20 : 44);
    const uint16_t body_height = display_height - chrome::status_height - chrome::indication_height;
    const uint16_t glyph_height = static_cast<uint16_t>(20 * layout.scale);
    const uint16_t usable_height = static_cast<uint16_t>(body_height - 2 * minimum_vertical_margin);
    layout.lines_per_screen = 1 + (usable_height - glyph_height) / layout.line_step;
    const uint16_t text_height =
        static_cast<uint16_t>(glyph_height + (layout.lines_per_screen - 1) * layout.line_step);
    layout.text_top =
        static_cast<uint16_t>(chrome::status_height + (body_height - text_height) / 2);
    return layout;
}
static size_t next_codepoint(const char* text, size_t length, size_t offset, uint32_t* codepoint)
{
    size_t consumed = gfx::decode_utf8(text + offset, codepoint);
    uint32_t second = 0;
    uint32_t third = 0;
    const size_t second_bytes =
        offset + consumed < length ? gfx::decode_utf8(text + offset + consumed, &second) : 0;
    const size_t third_bytes =
        second_bytes > 0 && second >= 0x0300U && second <= 0x036fU &&
                offset + consumed + second_bytes < length
            ? gfx::decode_utf8(text + offset + consumed + second_bytes, &third)
            : 0;
    uint32_t composed = 0;
    size_t consumed_codepoints = 0;
    if (second_bytes > 0 &&
        gfx::compose_unicode(*codepoint, second, third, &composed, &consumed_codepoints))
    {
        *codepoint = composed;
        consumed += second_bytes;
        if (consumed_codepoints == 3)
            consumed += third_bytes;
    }
    return consumed;
}
static uint16_t next_glyph_width(const char* text, size_t length, size_t offset, uint8_t scale,
                                 size_t* consumed, uint32_t* codepoint)
{
    *consumed = next_codepoint(text, length, offset, codepoint);
    return gfx::glyph_advance(*codepoint, scale);
}
static size_t next_page_offset(const epub::document_t* document, size_t offset,
                               const reader_settings_t* settings)
{
    const reader_layout_t layout = reader_layout(settings);
    size_t lines = 0;
    uint16_t x = 0;
    while (offset < document->length && lines < layout.lines_per_screen)
    {
        uint32_t codepoint = 0;
        size_t consumed = 0;
        const uint16_t width = next_glyph_width(document->text, document->length, offset,
                                                layout.scale, &consumed, &codepoint);
        if (codepoint == '\n')
        {
            ++lines;
            x = 0;
            offset += consumed;
            continue;
        }
        if (x > 0 && x + width > text_width)
        {
            ++lines;
            x = 0;
            if (lines >= layout.lines_per_screen)
                break;
        }
        x = static_cast<uint16_t>(x + width);
        offset += consumed;
    }
    return offset;
}
struct pagination_cache_t
{
    const epub::document_t* document;
    size_t length;
    uint8_t text_scale;
    uint8_t line_spacing;
    uint8_t count;
    size_t offsets[256];
};
static pagination_cache_t pagination_cache = {};
static bool pagination_matches(const epub::document_t* document, const reader_settings_t* settings)
{
    const uint8_t text_scale = settings != nullptr ? settings->text_scale : 0;
    const uint8_t line_spacing = settings != nullptr ? settings->line_spacing : 0;
    return pagination_cache.document == document && pagination_cache.length == document->length &&
           pagination_cache.text_scale == text_scale &&
           pagination_cache.line_spacing == line_spacing && pagination_cache.count != 0;
}
static void build_pagination(const epub::document_t* document, const reader_settings_t* settings)
{
    pagination_cache.document = document;
    pagination_cache.length = document != nullptr ? document->length : 0;
    pagination_cache.text_scale = settings != nullptr ? settings->text_scale : 0;
    pagination_cache.line_spacing = settings != nullptr ? settings->line_spacing : 0;
    pagination_cache.count = 1;
    pagination_cache.offsets[0] = 0;
    if (document == nullptr || document->length == 0)
        return;
    size_t offset = 0;
    uint16_t pages = 0;
    while (offset < document->length && pages < 255)
    {
        const size_t next = next_page_offset(document, offset, settings);
        ++pages;
        pagination_cache.offsets[pages] = next;
        if (next <= offset)
            break;
        offset = next;
    }
    pagination_cache.count = static_cast<uint8_t>(pages == 0 ? 1 : pages);
}
static void ensure_pagination(const epub::document_t* document, const reader_settings_t* settings)
{
    if (document != nullptr && !pagination_matches(document, settings))
        build_pagination(document, settings);
}
static size_t page_start(const epub::document_t* document, uint8_t page,
                         const reader_settings_t* settings)
{
    ensure_pagination(document, settings);
    const uint8_t index =
        page < pagination_cache.count ? page : static_cast<uint8_t>(pagination_cache.count - 1);
    return pagination_cache.offsets[index];
}
} // namespace

uint8_t page_count(const epub::document_t* document, const reader_settings_t* settings)
{
    if (document == nullptr || document->length == 0)
        return 1;
    build_pagination(document, settings);
    return pagination_cache.count;
}

void draw_reader(gfx::framebuffer_t* framebuffer, const epub::book_t* book,
                 const epub::document_t* document, uint8_t page, uint8_t page_count,
                 const reader_settings_t* settings)
{
    if (framebuffer == nullptr || book == nullptr || document == nullptr)
        return;
    const reader_layout_t layout = reader_layout(settings);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, book->title, "READING");
    const size_t start = page_start(document, page, settings);
    size_t offset = start;
    uint16_t line = 0;
    uint16_t x = 0;
    while (offset < document->length && line < layout.lines_per_screen)
    {
        uint32_t codepoint = 0;
        size_t consumed = 0;
        const uint16_t width = next_glyph_width(document->text, document->length, offset,
                                                layout.scale, &consumed, &codepoint);
        if (codepoint == '\n')
        {
            ++line;
            x = 0;
            offset += consumed;
            continue;
        }
        if (x > 0 && x + width > text_width)
        {
            ++line;
            x = 0;
            if (line >= layout.lines_per_screen)
                break;
        }
        gfx::draw_codepoint(framebuffer, static_cast<uint16_t>(text_left + x),
                            static_cast<uint16_t>(layout.text_top + line * layout.line_step),
                            codepoint, layout.scale, 0x00);
        x = static_cast<uint16_t>(x + width);
        offset += consumed;
    }
    char footer[32] = {};
    snprintf(footer, sizeof(footer), "%u / %u", static_cast<unsigned>(page + 1),
             static_cast<unsigned>(page_count));
    chrome::draw_indication_bar(framebuffer, "< PREV", footer, "NEXT >");
}

} // namespace ui
} // namespace xreader
