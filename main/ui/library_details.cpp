#include "library_details.hpp"

#include <stdio.h>

#include "epub/image.hpp"
#include "esp_heap_caps.h"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"

namespace xreader::ui
{

namespace
{
const char* basename_of(const char* path)
{
    const char* result = path == nullptr ? "" : path;
    if (path == nullptr)
        return result;
    for (const char* p = path; *p != '\0'; ++p)
        if (*p == '/')
            result = p + 1;
    return result;
}

static bool draw_cover(gfx::framebuffer_t* framebuffer,
                       const services::library_index::entry_t* entry, const layout::rect_t& area)
{
    if (framebuffer == nullptr || entry == nullptr || entry->cover_cache[0] == '\0' ||
        !entry->cover_supported || area.width < 16U || area.height < 16U)
        return false;
    FILE* file = fopen(entry->cover_cache, "rb");
    if (file == nullptr || fseek(file, 0, SEEK_END) != 0)
    {
        if (file != nullptr)
            fclose(file);
        return false;
    }
    const long length = ftell(file);
    if (length <= 0 || length > 2L * 1024L * 1024L || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }
    uint8_t* encoded = static_cast<uint8_t*>(
        heap_caps_malloc(static_cast<size_t>(length), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (encoded == nullptr)
        encoded =
            static_cast<uint8_t*>(heap_caps_malloc(static_cast<size_t>(length), MALLOC_CAP_8BIT));
    if (encoded == nullptr ||
        fread(encoded, 1, static_cast<size_t>(length), file) != static_cast<size_t>(length))
    {
        heap_caps_free(encoded);
        fclose(file);
        return false;
    }
    fclose(file);
    epub::image::info_t info = {};
    if (epub::image::inspect(encoded, static_cast<size_t>(length), &info) != ESP_OK ||
        !info.supported || info.width == 0U || info.height == 0U ||
        static_cast<uint32_t>(info.width) * info.height > 1024U * 1024U)
    {
        heap_caps_free(encoded);
        return false;
    }
    const size_t decoded_bytes = (static_cast<size_t>(info.width) * info.height + 1U) / 2U;
    uint8_t* pixels =
        static_cast<uint8_t*>(heap_caps_malloc(decoded_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pixels == nullptr)
        pixels = static_cast<uint8_t*>(heap_caps_malloc(decoded_bytes, MALLOC_CAP_8BIT));
    if (pixels == nullptr || epub::image::decode_mono(encoded, static_cast<size_t>(length), pixels,
                                                      decoded_bytes) != ESP_OK)
    {
        heap_caps_free(pixels);
        heap_caps_free(encoded);
        return false;
    }
    heap_caps_free(encoded);

    uint32_t width = info.width;
    uint32_t height = info.height;
    if (width > area.width)
    {
        height = height * area.width / width;
        width = area.width;
    }
    if (height > area.height)
    {
        width = width * area.height / height;
        height = area.height;
    }
    const uint16_t draw_width = static_cast<uint16_t>(width == 0U ? 1U : width);
    const uint16_t draw_height = static_cast<uint16_t>(height == 0U ? 1U : height);
    const uint16_t x = static_cast<uint16_t>(area.x + (area.width - draw_width) / 2U);
    const uint16_t y = static_cast<uint16_t>(area.y + (area.height - draw_height) / 2U);
    gfx::draw_rect(framebuffer, static_cast<uint16_t>(x > 1U ? x - 1U : x),
                   static_cast<uint16_t>(y > 1U ? y - 1U : y),
                   static_cast<uint16_t>(draw_width + 2U), static_cast<uint16_t>(draw_height + 2U),
                   0x0a);
    gfx::blit_4bpp_scaled(framebuffer, x, y, draw_width, draw_height, pixels, info.width,
                          info.height);
    heap_caps_free(pixels);
    return true;
}
} // namespace

void draw_library_details(gfx::framebuffer_t* framebuffer,
                          const services::library_index::entry_t* entry, int8_t footer_focus)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp{framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "Book Details");
    const layout::rect_t body = layout::inset(layout::content(vp), m.margin);

    if (entry == nullptr)
    {
        gfx::draw_text(framebuffer, body.x, body.y, "BOOK NOT AVAILABLE", 2, 0x00);
        chrome::draw_indication_bar(framebuffer, {nullptr, gfx::icon_none},
                                    {nullptr, gfx::icon_none}, {"Back", gfx::icon_arrow_back},
                                    footer_focus);
        return;
    }

    const uint16_t cover_width = body.width >= 520U ? static_cast<uint16_t>(body.width / 3U) : 0U;
    const layout::rect_t cover_area = {
        body.x, body.y, cover_width,
        static_cast<uint16_t>(body.height > 8U ? body.height - 8U : body.height)};
    const bool have_cover = cover_width > 0U && draw_cover(framebuffer, entry, cover_area);
    const uint16_t text_x =
        have_cover ? static_cast<uint16_t>(body.x + cover_width + m.gap) : body.x;
    const uint16_t text_width = have_cover && body.width > cover_width + m.gap
                                    ? static_cast<uint16_t>(body.width - cover_width - m.gap)
                                    : body.width;
    gfx::draw_text(framebuffer, text_x, body.y, entry->title, 2, 0x00);
    gfx::draw_text(framebuffer, text_x, static_cast<uint16_t>(body.y + 50U), entry->author, 1,
                   0x05);
    const uint16_t rule_y = static_cast<uint16_t>(body.y + 80U);
    gfx::fill_rect(framebuffer, text_x, rule_y, text_width, 1, 0x0b);

    char size[32] = {};
    if (entry->file_size >= 1024U * 1024U)
        snprintf(size, sizeof(size), "%lu.%lu MB",
                 static_cast<unsigned long>(entry->file_size / (1024U * 1024U)),
                 static_cast<unsigned long>((entry->file_size % (1024U * 1024U)) * 10U /
                                            (1024U * 1024U)));
    else
        snprintf(size, sizeof(size), "%lu KB",
                 static_cast<unsigned long>(entry->file_size / 1024U));
    char read[24] = {};
    snprintf(read, sizeof(read), "%s", entry->last_read_order == 0U ? "NOT READ" : "READ BEFORE");

    const char* labels[] = {"FILE", "SIZE", "READING", "FORMAT"};
    const char* values[] = {basename_of(entry->path), size, read, "EPUB"};
    const uint16_t list_y = static_cast<uint16_t>(rule_y + 18U);
    for (uint8_t i = 0; i < 4U; ++i)
    {
        const uint16_t y = static_cast<uint16_t>(list_y + i * (m.row_height + 4U));
        gfx::draw_text(framebuffer, text_x, y, labels[i], 1, 0x06);
        const uint16_t value_w = gfx::measure_text(values[i], 1);
        const uint16_t value_x =
            value_w < text_width ? static_cast<uint16_t>(text_x + text_width - value_w) : text_x;
        gfx::draw_text(framebuffer, value_x, y, values[i], 1, 0x00);
        gfx::fill_rect(framebuffer, text_x, static_cast<uint16_t>(y + 25U), text_width, 1, 0x0d);
    }
    chrome::draw_indication_bar(framebuffer, {"Manage", gfx::icon_edit}, {"Open", gfx::icon_book},
                                {"Back", gfx::icon_arrow_back}, footer_focus);
}

} // namespace xreader::ui
