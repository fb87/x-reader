#include "bookmarks.hpp"

#include <stdio.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"
#include "ui/navigation/focus.hpp"

namespace xreader::ui
{
namespace
{
layout::rect_t bookmarks_area(layout::viewport_t vp)
{
    const auto m = layout::metrics(vp);
    return layout::inset(layout::content(vp), m.margin);
}
} // namespace

void draw_bookmarks(gfx::framebuffer_t* framebuffer, const bookmark_view_t* items, uint8_t count,
                    uint8_t focus)
{
    if (framebuffer == nullptr)
        return;

    const layout::viewport_t vp{framebuffer->width, framebuffer->height};
    const auto m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "BOOKMARKS", count != 0 ? "SAVED" : "EMPTY");
    const auto area = bookmarks_area(vp);

    if (count == 0)
    {
        gfx::draw_text(framebuffer, area.x, area.y, "No bookmarks yet", 1, 0x05);
        gfx::draw_text(framebuffer, area.x, static_cast<uint16_t>(area.y + 28U),
                       "Add one from the Reader Menu", 1, 0x07);
    }
    else
    {
        for (uint8_t i = 0; i < count; ++i)
        {
            const auto row = layout::row(area, i, count, m.row_height, m.gap);
            const bool selected = i == focus;
            gfx::fill_rect(framebuffer, row.x, row.y, row.width, row.height,
                           selected ? 0x00 : 0x0f);

            char line[64] = {};
            snprintf(line, sizeof(line), "Chapter %u   Page %u",
                     static_cast<unsigned>(items[i].spine + 1U),
                     static_cast<unsigned>(items[i].page + 1U));
            gfx::draw_text(framebuffer, static_cast<uint16_t>(row.x + 12U),
                           static_cast<uint16_t>(row.y + 12U), line, 1, selected ? 0x0f : 0x00);
        }
    }

    chrome::draw_indication_bar(framebuffer, "MOVE", "OPEN", "BACK");
}

bool bookmarks_touch_index(uint16_t width, uint16_t height, uint16_t x, uint16_t y, uint8_t count,
                           uint8_t* index)
{
    if (index == nullptr || count == 0)
        return false;
    const layout::viewport_t vp{width, height};
    const auto m = layout::metrics(vp);
    return focus::hit_rows(bookmarks_area(vp), count, m.row_height, m.gap, x, y, index);
}

} // namespace xreader::ui
