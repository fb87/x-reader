#include "reader.hpp"

#include <stdio.h>
#include <string.h>

#include "epub/image.hpp"
#include "esp_heap_caps.h"

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"
#include "ui/widgets.hpp"

namespace xreader
{
namespace ui
{

namespace
{
// Height of the progress bar + position/attribution row drawn above the footer.
static constexpr uint16_t progress_strip_height = 34;
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
    // Line steps give the reader's 28px glyphs (see reader_font.hpp) room to
    // breathe: normal is glyph height + 4px leading, relaxed adds more on top.
    result.line_step = settings != nullptr && settings->line_spacing != 0
                           ? static_cast<uint16_t>(result.scale == 1 ? 38 : 66)
                           : static_cast<uint16_t>(result.scale == 1 ? 32 : 60);
    layout::rect_t body = layout::content(vp);
    // Reserve the strip above the action bar for the progress indicator and the
    // title/author line, or the last text line collides with them.
    if (body.height > progress_strip_height)
        body.height = static_cast<uint16_t>(body.height - progress_strip_height);
    // Widened from 20/8 (compact/other) -- text this close to the physical
    // bezel could lose its leading pixels on real hardware, especially at the
    // "Narrow" margin_mode below.
    const uint16_t base_margin =
        m.display_class == layout::display_compact ? 28 : static_cast<uint16_t>(m.margin - 4U);
    const uint8_t margin_mode = settings != nullptr ? settings->margin_mode : 1U;
    const uint16_t horizontal_margin =
        margin_mode == 0U
            ? static_cast<uint16_t>(base_margin / 2U)
            : (margin_mode == 2U ? static_cast<uint16_t>(base_margin + base_margin / 2U)
                                 : base_margin);
    result.text_left = horizontal_margin;
    result.text_width = vp.width > static_cast<uint32_t>(horizontal_margin) * 2U
                            ? static_cast<uint16_t>(vp.width - horizontal_margin * 2U)
                            : vp.width;
    const uint16_t glyph_height = static_cast<uint16_t>(28U * result.scale);
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

static const epub::document_image_t* image_at(const epub::document_t* document, size_t offset)
{
    if (document == nullptr)
        return nullptr;
    for (uint8_t i = 0; i < document->image_count; ++i)
        if (document->images[i].text_offset == offset)
            return &document->images[i];
    return nullptr;
}

static void image_target(const epub::document_image_t* image, const reader_layout_t& reader,
                         uint16_t* width, uint16_t* height)
{
    *width = 0;
    *height = 0;
    if (image == nullptr || image->width == 0U || image->height == 0U)
        return;
    const uint16_t max_width = reader.text_width;
    const uint16_t max_height = static_cast<uint16_t>(reader.lines_per_screen * reader.line_step);
    uint32_t w = image->width;
    uint32_t h = image->height;
    if (w > max_width)
    {
        h = h * max_width / w;
        w = max_width;
    }
    if (h > max_height)
    {
        w = w * max_height / h;
        h = max_height;
    }
    *width = static_cast<uint16_t>(w == 0U ? 1U : w);
    *height = static_cast<uint16_t>(h == 0U ? 1U : h);
}

struct image_cache_t
{
    char book_path[256];
    char href[epub::book_text_length];
    uint16_t width;
    uint16_t height;
    uint8_t* pixels;
};

static image_cache_t image_cache = {};

static void clear_image_cache()
{
    heap_caps_free(image_cache.pixels);
    image_cache = {};
}

static const uint8_t* load_image_pixels(const char* book_path, const epub::document_image_t* image)
{
    if (book_path == nullptr || image == nullptr)
        return nullptr;
    if (image_cache.pixels != nullptr && strcmp(image_cache.book_path, book_path) == 0 &&
        strcmp(image_cache.href, image->href) == 0)
        return image_cache.pixels;

    clear_image_cache();
    uint8_t* encoded = nullptr;
    size_t encoded_size = 0;
    if (epub::load_resource(book_path, image->href, &encoded, &encoded_size) != ESP_OK)
        return nullptr;
    epub::image::info_t info = {};
    if (epub::image::inspect(encoded, encoded_size, &info) != ESP_OK || !info.supported ||
        info.width == 0U || info.height == 0U ||
        static_cast<uint32_t>(info.width) * info.height > 1024U * 1024U)
    {
        heap_caps_free(encoded);
        return nullptr;
    }
    const size_t bytes = (static_cast<size_t>(info.width) * info.height + 1U) / 2U;
    uint8_t* pixels =
        static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pixels == nullptr)
        pixels = static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
    if (pixels == nullptr ||
        epub::image::decode_mono(encoded, encoded_size, pixels, bytes) != ESP_OK)
    {
        heap_caps_free(pixels);
        heap_caps_free(encoded);
        return nullptr;
    }
    heap_caps_free(encoded);
    snprintf(image_cache.book_path, sizeof(image_cache.book_path), "%s", book_path);
    snprintf(image_cache.href, sizeof(image_cache.href), "%s", image->href);
    image_cache.width = info.width;
    image_cache.height = info.height;
    image_cache.pixels = pixels;
    return image_cache.pixels;
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
    return gfx::glyph_advance(*codepoint, scale, gfx::font_reader);
}
// Finds where the current line ends: at a literal '\n' (left unconsumed so
// callers can still see it and apply paragraph spacing), at the document end,
// or -- when the line would otherwise overflow mid-word -- at the last space
// boundary before the overflowing glyph, so prose wraps on whole words. Only
// falls back to a hard character break when a single run has no space at all
// (e.g. a long URL) and is itself wider than the line, so pagination cannot
// stall.
static size_t next_line_break(const epub::document_t* document, size_t offset,
                              const reader_layout_t& reader)
{
    uint16_t x = 0;
    size_t break_offset = offset;
    bool have_break = false;
    while (offset < document->length)
    {
        if (image_at(document, offset) != nullptr)
            break;
        uint32_t codepoint = 0;
        size_t consumed = 0;
        const uint16_t width = next_glyph_width(document->text, document->length, offset,
                                                reader.scale, &consumed, &codepoint);
        if (codepoint == '\n')
            break;
        if (x > 0 && x + width > reader.text_width)
            return have_break ? break_offset : offset;
        x = static_cast<uint16_t>(x + width);
        offset += consumed;
        if (codepoint == ' ')
        {
            have_break = true;
            break_offset = offset;
        }
    }
    return offset;
}

static size_t next_page_offset(const epub::document_t* document, size_t offset,
                               const reader_settings_t* settings, layout::viewport_t vp)
{
    const reader_layout_t reader = reader_layout(settings, vp);
    size_t lines = 0;
    while (offset < document->length && lines < reader.lines_per_screen)
    {
        if (const epub::document_image_t* image = image_at(document, offset))
        {
            uint16_t image_width = 0;
            uint16_t image_height = 0;
            image_target(image, reader, &image_width, &image_height);
            const size_t image_lines =
                image_height == 0U ? 1U
                                   : (static_cast<size_t>(image_height) + reader.line_step - 1U) /
                                         reader.line_step;
            if (lines > 0U && lines + image_lines > reader.lines_per_screen)
                break;
            lines += image_lines;
            uint32_t marker = 0;
            offset += gfx::decode_utf8(document->text + offset, &marker);
            continue;
        }
        offset = next_line_break(document, offset, reader);
        ++lines;
        if (offset < document->length)
        {
            uint32_t marker = 0;
            const size_t marker_bytes = gfx::decode_utf8(document->text + offset, &marker);
            if (marker == '\n')
            {
                offset += marker_bytes;
                if (settings != nullptr && settings->paragraph_spacing != 0U)
                    ++lines;
            }
        }
    }
    return offset;
}
struct pagination_cache_t
{
    const epub::document_t* document;
    size_t length;
    uint8_t text_scale;
    uint8_t line_spacing;
    uint8_t margin_mode;
    uint8_t paragraph_spacing;
    uint8_t text_alignment;
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
    const uint8_t margin_mode = settings != nullptr ? settings->margin_mode : 1;
    const uint8_t paragraph_spacing = settings != nullptr ? settings->paragraph_spacing : 0;
    const uint8_t text_alignment = settings != nullptr ? settings->text_alignment : 0;
    return pagination_cache.document == document && pagination_cache.length == document->length &&
           pagination_cache.text_scale == text_scale &&
           pagination_cache.line_spacing == line_spacing &&
           pagination_cache.margin_mode == margin_mode &&
           pagination_cache.paragraph_spacing == paragraph_spacing &&
           pagination_cache.text_alignment == text_alignment &&
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
    pagination_cache.margin_mode = settings != nullptr ? settings->margin_mode : 1;
    pagination_cache.paragraph_spacing = settings != nullptr ? settings->paragraph_spacing : 0;
    pagination_cache.text_alignment = settings != nullptr ? settings->text_alignment : 0;
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

static uint16_t visual_line_width(const epub::document_t* document, size_t offset,
                                  const reader_layout_t& reader)
{
    if (document == nullptr)
        return 0;
    const size_t line_end = next_line_break(document, offset, reader);
    uint16_t width = 0;
    while (offset < line_end)
    {
        uint32_t codepoint = 0;
        size_t consumed = 0;
        width = static_cast<uint16_t>(
            width + next_glyph_width(document->text, document->length, offset, reader.scale,
                                     &consumed, &codepoint));
        offset += consumed;
    }
    return width;
}

static uint16_t aligned_line_x(const reader_settings_t* settings, const reader_layout_t& reader,
                               uint16_t line_width)
{
    const uint8_t alignment = settings != nullptr ? settings->text_alignment : 0U;
    if (line_width >= reader.text_width || alignment == 0U)
        return 0U;
    if (alignment == 1U)
        return static_cast<uint16_t>((reader.text_width - line_width) / 2U);
    return static_cast<uint16_t>(reader.text_width - line_width);
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

bool find_page(const epub::document_t* document, const char* query,
               const reader_settings_t* settings, uint16_t display_width, uint16_t display_height,
               size_t start_offset, uint8_t* page, size_t* match_offset)
{
    if (document == nullptr || query == nullptr || query[0] == '\0' || page == nullptr)
        return false;
    if (start_offset >= document->length)
        start_offset = 0;
    const char* match = strstr(document->text + start_offset, query);
    if (match == nullptr)
        return false;
    const size_t offset = static_cast<size_t>(match - document->text);
    const layout::viewport_t vp = {display_width, display_height};
    ensure_pagination(document, settings, vp);
    uint8_t found_page = 0;
    for (uint8_t index = 0; index < pagination_cache.count; ++index)
    {
        const size_t begin = pagination_cache.offsets[index];
        const size_t end = index + 1U <= pagination_cache.count
                               ? pagination_cache.offsets[index + 1U]
                               : document->length;
        if (offset >= begin && offset < end)
        {
            found_page = index;
            break;
        }
    }
    *page = found_page;
    if (match_offset != nullptr)
        *match_offset = offset;
    return true;
}

void draw_reader(gfx::framebuffer_t* framebuffer, const char* book_path, const epub::book_t* book,
                 const epub::document_t* document, uint8_t page, uint8_t page_count,
                 uint8_t spine_index, const reader_settings_t* settings)
{
    if (framebuffer == nullptr || book == nullptr || document == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    const reader_layout_t reader = reader_layout(settings, vp);
    // A page turn repaints almost the whole screen. Clearing the packed framebuffer is
    // much cheaper than touching each body pixel through set_pixel(). present() still
    // compares against the front buffer and transfers only pixels that actually changed.
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, book->title);
    const size_t start = page_start(document, page, settings, vp);
    size_t offset = start;
    uint16_t line = 0;
    uint16_t line_origin =
        aligned_line_x(settings, reader, visual_line_width(document, offset, reader));
    uint16_t x = line_origin;
    while (offset < document->length && line < reader.lines_per_screen)
    {
        if (const epub::document_image_t* image = image_at(document, offset))
        {
            uint16_t target_width = 0;
            uint16_t target_height = 0;
            image_target(image, reader, &target_width, &target_height);
            const size_t image_lines =
                target_height == 0U ? 1U
                                    : (static_cast<size_t>(target_height) + reader.line_step - 1U) /
                                          reader.line_step;
            if (line > 0U && line + image_lines > reader.lines_per_screen)
                break;
            const uint16_t image_y =
                static_cast<uint16_t>(reader.text_top + line * reader.line_step);
            const uint16_t image_x =
                target_width < reader.text_width
                    ? static_cast<uint16_t>(reader.text_left +
                                            (reader.text_width - target_width) / 2U)
                    : reader.text_left;
            const uint8_t* pixels = load_image_pixels(book_path, image);
            if (pixels != nullptr && target_width > 0U && target_height > 0U)
                gfx::blit_4bpp_scaled(framebuffer, image_x, image_y, target_width, target_height,
                                      pixels, image_cache.width, image_cache.height);
            else
            {
                const uint16_t placeholder_h =
                    target_height == 0U ? reader.line_step : target_height;
                const uint16_t placeholder_w =
                    target_width == 0U ? reader.text_width / 2U : target_width;
                gfx::draw_rect(framebuffer, image_x, image_y, placeholder_w, placeholder_h, 0x08);
                gfx::draw_text(framebuffer, static_cast<uint16_t>(image_x + 8U),
                               static_cast<uint16_t>(image_y + 8U), "IMAGE", 1, 0x06);
            }
            line = static_cast<uint16_t>(line + image_lines);
            uint32_t marker = 0;
            offset += gfx::decode_utf8(document->text + offset, &marker);
            line_origin =
                aligned_line_x(settings, reader, visual_line_width(document, offset, reader));
            x = line_origin;
            continue;
        }
        const size_t line_end = next_line_break(document, offset, reader);
        while (offset < line_end)
        {
            uint32_t codepoint = 0;
            size_t consumed = 0;
            const uint16_t width = next_glyph_width(document->text, document->length, offset,
                                                    reader.scale, &consumed, &codepoint);
            gfx::draw_codepoint(framebuffer, static_cast<uint16_t>(reader.text_left + x),
                                static_cast<uint16_t>(reader.text_top + line * reader.line_step),
                                codepoint, reader.scale, 0x00, gfx::font_reader);
            x = static_cast<uint16_t>(x + width);
            offset += consumed;
        }
        ++line;
        if (offset < document->length)
        {
            uint32_t marker = 0;
            const size_t marker_bytes = gfx::decode_utf8(document->text + offset, &marker);
            if (marker == '\n')
            {
                offset += marker_bytes;
                if (settings != nullptr && settings->paragraph_spacing != 0U)
                    ++line;
            }
        }
        line_origin = aligned_line_x(settings, reader, visual_line_width(document, offset, reader));
        x = line_origin;
    }
    // navigation_result() (navigation.cpp) already refuses to move past these same
    // bounds, so tapping a disabled cell was already a silent no-op; this just
    // shows that up front instead of only after an unexplained non-reaction.
    const bool can_prev = page > 0 || spine_index > 0;
    const bool can_next = static_cast<uint16_t>(page) + 1U < page_count ||
                          static_cast<uint16_t>(spine_index) + 1U < book->spine_count;
    chrome::draw_indication_bar(framebuffer, {"Prev", gfx::icon_arrow_back, !can_prev},
                                {"Menu", gfx::icon_menu},
                                {"Next", gfx::icon_chevron_right, !can_next});

    // Mockup 5 shows a progress bar with "3 / 256" on the left and the book's
    // title and author on the right, just above the action bar.
    const layout::metrics_t footer_metrics = layout::metrics(vp);
    const uint16_t margin = footer_metrics.margin;
    const uint16_t bar_y = static_cast<uint16_t>(
        framebuffer->height - footer_metrics.footer_height - progress_strip_height + 6U);
    const uint16_t bar_width = framebuffer->width > margin * 2U
                                   ? static_cast<uint16_t>(framebuffer->width - margin * 2U)
                                   : framebuffer->width;
    widgets::draw_progress(framebuffer, {margin, bar_y, bar_width, 3},
                           static_cast<uint32_t>(page) + 1U, page_count);

    char position[24] = {};
    snprintf(position, sizeof(position), "%u / %u", static_cast<unsigned>(page + 1),
             static_cast<unsigned>(page_count));
    const uint16_t label_y = static_cast<uint16_t>(bar_y + 8U);
    gfx::draw_text(framebuffer, margin, label_y, position, 1, 0x06);

    // Title and author are fixed-size fields in book_t; bound the copy so the
    // compiler can prove the format cannot truncate past the buffer.
    char attribution[288] = {};
    if (book != nullptr && book->author[0] != '\0')
        snprintf(attribution, sizeof(attribution), "%.127s - %.127s", book->title, book->author);
    else if (book != nullptr)
        snprintf(attribution, sizeof(attribution), "%.127s", book->title);
    if (attribution[0] != '\0')
    {
        const uint16_t width = gfx::measure_text(attribution, 1);
        if (bar_width > width)
            gfx::draw_text(framebuffer, static_cast<uint16_t>(margin + bar_width - width), label_y,
                           attribution, 1, 0x07);
    }
}

} // namespace ui
} // namespace xreader
