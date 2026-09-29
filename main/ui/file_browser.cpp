#include "file_browser.hpp"

#include <stdio.h>
#include <string.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/widgets.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{
namespace
{
static uint8_t visible_rows(layout::viewport_t viewport)
{
    const layout::metrics_t metrics = layout::metrics(viewport);
    const layout::rect_t area = layout::inset(layout::content(viewport), metrics.margin);
    const uint16_t stride = static_cast<uint16_t>(metrics.row_height + metrics.gap);
    const uint16_t count = stride == 0U ? 1U : static_cast<uint16_t>(area.height / stride);
    return static_cast<uint8_t>(count == 0U ? 1U : (count > 8U ? 8U : count));
}

static uint8_t first_visible(uint8_t count, uint8_t focus, uint8_t visible)
{
    if (count <= visible || focus < visible)
        return 0U;
    uint8_t first = static_cast<uint8_t>(focus - visible + 1U);
    const uint8_t maximum = static_cast<uint8_t>(count - visible);
    if (first > maximum)
        first = maximum;
    return first;
}

} // namespace

void draw_file_browser(gfx::framebuffer_t* framebuffer,
                       const services::file_browser::listing_t* listing, uint8_t focus)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t viewport = {framebuffer->width, framebuffer->height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, nullptr);

    // Mockup 3 heads the screen with the path trail rather than a fixed title.
    const char* parts[4] = {"SD Card", nullptr, nullptr, nullptr};
    uint8_t part_count = 1;
    if (listing != nullptr)
    {
        const char* cursor = listing->path;
        const size_t root_length = strlen(listing->root);
        if (strncmp(listing->path, listing->root, root_length) == 0)
            cursor += root_length;
        static char segments[3][64];
        uint8_t depth = 0;
        while (*cursor != '\0' && depth < 3U)
        {
            while (*cursor == '/')
                ++cursor;
            if (*cursor == '\0')
                break;
            size_t length = 0;
            while (cursor[length] != '\0' && cursor[length] != '/')
                ++length;
            if (length >= sizeof(segments[0]))
                length = sizeof(segments[0]) - 1U;
            memcpy(segments[depth], cursor, length);
            segments[depth][length] = '\0';
            parts[1U + depth] = segments[depth];
            ++depth;
            cursor += length;
        }
        part_count = static_cast<uint8_t>(1U + depth);
    }
    const uint16_t header_height = chrome::title_height({framebuffer->width, framebuffer->height});
    const uint16_t header_pad = metrics.display_class == layout::display_compact ? 14U : 20U;
    const layout::rect_t header = {header_pad, metrics.status_height,
                                   static_cast<uint16_t>(framebuffer->width - header_pad * 2U),
                                   header_height};
    widgets::draw_breadcrumb(framebuffer, header, parts, part_count);
    gfx::fill_rect(framebuffer, header.x, static_cast<uint16_t>(header.y + header.height - 1U),
                   header.width, 1, 0x0b);

    layout::rect_t area = layout::inset(layout::content(viewport), metrics.margin);
    if (area.height > header_height)
    {
        area.y = static_cast<uint16_t>(area.y + header_height);
        area.height = static_cast<uint16_t>(area.height - header_height);
    }
    if (listing == nullptr || listing->count == 0U)
    {
        gfx::draw_text(framebuffer, area.x, area.y, "EMPTY FOLDER", 2, 0x00);
        chrome::draw_indication_bar(framebuffer, {nullptr, gfx::icon_none},
                                {nullptr, gfx::icon_none},
                                {"Back", gfx::icon_arrow_back});
        return;
    }

    const uint8_t visible = visible_rows(viewport);
    const uint8_t first = first_visible(listing->count, focus, visible);
    const uint8_t shown = static_cast<uint8_t>(listing->count - first < visible
                                                   ? listing->count - first
                                                   : visible);
    for (uint8_t row = 0; row < shown; ++row)
    {
        const uint8_t index = static_cast<uint8_t>(first + row);
        const auto& entry = listing->entries[index];
        const layout::rect_t item = layout::row(area, row, shown, metrics.row_height, metrics.gap);
        const bool selected = index == focus;
        gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height,
                       selected ? 0x0d : 0x0f);
        const uint8_t fg = 0x00;
        const uint8_t secondary = selected ? 0x04 : 0x06;
        const uint16_t glyph = gfx::icon_advance(1);
        gfx::draw_icon(framebuffer, static_cast<uint16_t>(item.x + 8U),
                       static_cast<uint16_t>(item.y + (item.height - glyph) / 2U),
                       entry.directory ? gfx::icon_folder : gfx::icon_description, 1, fg);
        gfx::draw_text(framebuffer, static_cast<uint16_t>(item.x + 12U + glyph),
                       static_cast<uint16_t>(item.y + 6U), entry.name, 1, fg);
        (void)secondary;
        if (!entry.directory)
        {
            char size[24] = {};
            if (entry.size >= 1024U * 1024U)
                snprintf(size, sizeof(size), "%lu.%lu MB",
                         static_cast<unsigned long>(entry.size / (1024U * 1024U)),
                         static_cast<unsigned long>((entry.size % (1024U * 1024U)) / (105U * 1024U)));
            else
                snprintf(size, sizeof(size), "%lu KB",
                         static_cast<unsigned long>(entry.size / 1024U));
            const uint16_t width = gfx::measure_text(size, 1);
            if (item.width > width + 12U)
                gfx::draw_text(framebuffer,
                               static_cast<uint16_t>(item.x + item.width - width - 10U),
                               static_cast<uint16_t>(item.y + item.height - 22U), size, 1,
                               secondary);
        }
    }
    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_list},
                                {"Open", gfx::icon_folder},
                                {"Back", gfx::icon_arrow_back});
}

bool file_browser_touch_index(uint16_t width, uint16_t height, uint16_t x, uint16_t y,
                              uint8_t count, uint8_t focus, uint8_t* index)
{
    if (index == nullptr || count == 0U)
        return false;
    const layout::viewport_t viewport = {width, height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    const layout::rect_t area = layout::inset(layout::content(viewport), metrics.margin);
    const uint8_t visible = visible_rows(viewport);
    const uint8_t first = first_visible(count, focus, visible);
    const uint8_t shown = static_cast<uint8_t>(count - first < visible ? count - first : visible);
    for (uint8_t row = 0; row < shown; ++row)
    {
        const layout::rect_t item = layout::row(area, row, shown, metrics.row_height, metrics.gap);
        if (x >= item.x && x < item.x + item.width && y >= item.y && y < item.y + item.height)
        {
            *index = static_cast<uint8_t>(first + row);
            return true;
        }
    }
    return false;
}

} // namespace ui
} // namespace xreader
