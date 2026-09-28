#include "reader.hpp"

#include <stdio.h>
#include <string.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static constexpr uint16_t minimum_vertical_margin = 12;
struct reader_layout_t
{
    uint8_t scale;
    uint16_t line_step;
    uint16_t text_left;
    uint16_t text_width;
    uint16_t text_top;
    size_t lines_per_screen;
};
static reader_layout_t reader_layout(const reader_settings_t* settings, layout::viewport_t vp)
{
    reader_layout_t result = {};
    const layout::metrics_t m = layout::metrics(vp);
    result.scale = settings != nullptr && settings->text_scale == 1 ? 1 : 2;
    result.line_step = settings != nullptr && settings->line_spacing != 0
                           ? static_cast<uint16_t>(result.scale == 1 ? 24 : 48)
                           : static_cast<uint16_t>(result.scale == 1 ? 20 : 44);
    const layout::rect_t body = layout::content(vp);
    const uint16_t horizontal_margin =
        m.display_class == layout::display_compact ? 20 : static_cast<uint16_t>(m.margin - 12U);
    result.text_left = horizontal_margin;
    result.text_width = vp.width > static_cast<uint32_t>(horizontal_margin) * 2U
                            ? static_cast<uint16_t>(vp.width - horizontal_margin * 2U)
                            : vp.width;
    const uint16_t glyph_height = static_cast<uint16_t>(20U * result.scale);
    const uint16_t usable_height =
        body.height > minimum_vertical_margin * 2U
            ? static_cast<uint16_t>(body.height - minimum_vertical_margin * 2U)
            : body.height;
    result.lines_per_screen =
        usable_height >= glyph_height ? 1U + (usable_height - glyph_height) / result.line_step : 1U;
    const uint16_t text_height =
        static_cast<uint16_t>(glyph_height + (result.lines_per_screen - 1U) * result.line_step);
    result.text_top = static_cast<uint16_t>(
        body.y + (body.height > text_height ? (body.height - text_height) / 2U : 0U));
    return result;
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
                               const reader_settings_t* settings, layout::viewport_t vp)
{
    const reader_layout_t reader = reader_layout(settings, vp);
    size_t lines = 0;
    uint16_t x = 0;
    while (offset < document->length && lines < reader.lines_per_screen)
    {
        uint32_t codepoint = 0;
        size_t consumed = 0;
        const uint16_t width = next_glyph_width(document->text, document->length, offset,
                                                reader.scale, &consumed, &codepoint);
        if (codepoint == '\n')
        {
            ++lines;
            x = 0;
            offset += consumed;
            continue;
        }
        if (x > 0 && x + width > reader.text_width)
        {
            ++lines;
            x = 0;
            if (lines >= reader.lines_per_screen)
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
    uint16_t display_width;
    uint16_t display_height;
    uint8_t count;
    size_t offsets[256];
};

static pagination_cache_t pagination_cache = {};

static bool pagination_matches(const epub::document_t* document, const reader_settings_t* settings,
                               layout::viewport_t vp)
{
    const uint8_t text_scale = settings != nullptr ? settings->text_scale : 0;
    const uint8_t line_spacing = settings != nullptr ? settings->line_spacing : 0;
    return pagination_cache.document == document && pagination_cache.length == document->length &&
           pagination_cache.text_scale == text_scale &&
           pagination_cache.line_spacing == line_spacing &&
           pagination_cache.display_width == vp.width &&
           pagination_cache.display_height == vp.height && pagination_cache.count != 0;
}

static void build_pagination(const epub::document_t* document, const reader_settings_t* settings,
                             layout::viewport_t vp)
{
    pagination_cache.document = document;
    pagination_cache.length = document != nullptr ? document->length : 0;
    pagination_cache.text_scale = settings != nullptr ? settings->text_scale : 0;
    pagination_cache.line_spacing = settings != nullptr ? settings->line_spacing : 0;
    pagination_cache.display_width = vp.width;
    pagination_cache.display_height = vp.height;
    pagination_cache.count = 1;
    pagination_cache.offsets[0] = 0;
    if (document == nullptr || document->length == 0)
        return;

    size_t offset = 0;
    uint16_t pages = 0;
    while (offset < document->length && pages < 255)
    {
        const size_t next = next_page_offset(document, offset, settings, vp);
        ++pages;
        pagination_cache.offsets[pages] = next;
        if (next <= offset)
            break;
        offset = next;
    }
    pagination_cache.count = static_cast<uint8_t>(pages == 0 ? 1 : pages);
}

static void ensure_pagination(const epub::document_t* document, const reader_settings_t* settings,
                              layout::viewport_t vp)
{
    if (document == nullptr)
        return;
    if (!pagination_matches(document, settings, vp))
        build_pagination(document, settings, vp);
}

static size_t page_start(const epub::document_t* document, uint8_t page,
                         const reader_settings_t* settings, layout::viewport_t vp)
{
    ensure_pagination(document, settings, vp);
    if (pagination_cache.count == 0)
        return 0;
    const uint8_t index =
        page < pagination_cache.count ? page : static_cast<uint8_t>(pagination_cache.count - 1);
    return pagination_cache.offsets[index];
}

} // namespace

uint8_t page_count(const epub::document_t* document, const reader_settings_t* settings,
                   uint16_t display_width, uint16_t display_height)
{
    if (document == nullptr || document->length == 0)
        return 1;
    // page_count() is called when a document/spine is loaded or layout settings change.
    // Rebuild here unconditionally so reusing the same document_t buffer for a new spine
    // cannot accidentally reuse offsets when the new text happens to have the same length.
    build_pagination(document, settings, {display_width, display_height});
    return pagination_cache.count;
}

void draw_reader(gfx::framebuffer_t* framebuffer, const epub::book_t* book,
                 const epub::document_t* document, uint8_t page, uint8_t page_count,
                 const reader_settings_t* settings)
{
    if (framebuffer == nullptr || book == nullptr || document == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    const reader_layout_t reader = reader_layout(settings, vp);
    // A page turn repaints almost the whole screen. Clearing the packed framebuffer is
    // much cheaper than touching each body pixel through set_pixel(). present() still
    // compares against the front buffer and transfers only pixels that actually changed.
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, book->title, "READING");
    const size_t start = page_start(document, page, settings, vp);
    size_t offset = start;
    uint16_t line = 0;
    uint16_t x = 0;
    while (offset < document->length && line < reader.lines_per_screen)
    {
        uint32_t codepoint = 0;
        size_t consumed = 0;
        const uint16_t width = next_glyph_width(document->text, document->length, offset,
                                                reader.scale, &consumed, &codepoint);
        if (codepoint == '\n')
        {
            ++line;
            x = 0;
            offset += consumed;
            continue;
        }
        if (x > 0 && x + width > reader.text_width)
        {
            ++line;
            x = 0;
            if (line >= reader.lines_per_screen)
                break;
        }
        gfx::draw_codepoint(framebuffer, static_cast<uint16_t>(reader.text_left + x),
                            static_cast<uint16_t>(reader.text_top + line * reader.line_step),
                            codepoint, reader.scale, 0x00);
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
