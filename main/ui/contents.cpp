#include "contents.hpp"

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"
#include "ui/navigation/focus.hpp"
#include <stdio.h>

namespace xreader::ui
{
namespace
{
layout::rect_t rows_area(layout::viewport_t vp)
{
    const auto m = layout::metrics(vp);
    return layout::inset(layout::content(vp), m.margin);
}
uint8_t toc_count(const epub::book_t* book)
{
    if (!book)
        return 0;
    return book->toc_count ? book->toc_count : book->spine_count;
}
const char* item_title(const epub::book_t* book, uint8_t i)
{
    if (book->toc_count && i < book->toc_count && book->toc[i].title[0])
        return book->toc[i].title;
    if (i < book->spine_count && book->spine[i].title[0])
        return book->spine[i].title;
    return "Chapter";
}
} // namespace

void draw_contents(gfx::framebuffer_t* framebuffer, const epub::book_t* book, uint8_t focus,
                   int8_t footer_focus)
{
    if (!framebuffer)
        return;
    const layout::viewport_t vp{framebuffer->width, framebuffer->height};
    const auto m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "Table of Contents");
    const auto area = rows_area(vp);
    const uint8_t count = toc_count(book);
    if (count == 0)
    {
        gfx::draw_text(framebuffer, area.x, area.y, "No table of contents", 1, 0x05);
    }
    else
    {
        const uint8_t visible =
            static_cast<uint8_t>((area.height + m.gap) / (m.row_height + m.gap));
        const uint8_t max_visible = visible ? visible : 1;
        uint8_t first = focus >= max_visible ? static_cast<uint8_t>(focus - max_visible + 1U) : 0;
        if (first + max_visible > count && count > max_visible)
            first = static_cast<uint8_t>(count - max_visible);
        for (uint8_t slot = 0; slot < max_visible && first + slot < count; ++slot)
        {
            const uint8_t idx = static_cast<uint8_t>(first + slot);
            auto r = layout::row(area, slot, max_visible, m.row_height, m.gap);
            const bool selected = footer_focus < 0 && idx == focus;
            // Focus-move redraws use the panel's fast 1-bit-only refresh mode,
            // which thresholds every pixel to pure black/white -- a subtle gray
            // wash is invisible under it. Inverting to a solid black row with
            // white content survives that threshold instead.
            gfx::fill_rect(framebuffer, r.x, r.y, r.width, r.height, selected ? 0x00 : 0x0f);
            const uint8_t fg = selected ? 0x0f : 0x00;
            char n[12] = {};
            snprintf(n, sizeof(n), "%u", static_cast<unsigned>(idx + 1U));
            gfx::draw_text(framebuffer, static_cast<uint16_t>(r.x + 10U),
                           static_cast<uint16_t>(r.y + 12U), n, 1, selected ? 0x0f : 0x07);
            gfx::draw_text(framebuffer, static_cast<uint16_t>(r.x + 42U),
                           static_cast<uint16_t>(r.y + 12U), item_title(book, idx), 1, fg);
            if (!selected)
                gfx::fill_rect(framebuffer, r.x, static_cast<uint16_t>(r.y + r.height - 1U),
                               r.width, 1, 0x0d);
        }
    }
    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_list}, {"Open", gfx::icon_book},
                                {"Back", gfx::icon_arrow_back}, footer_focus);
}

bool contents_touch_index(uint16_t width, uint16_t height, uint16_t x, uint16_t y, uint8_t count,
                          uint8_t* index)
{
    if (!index || count == 0)
        return false;
    const layout::viewport_t vp{width, height};
    const auto m = layout::metrics(vp);
    const auto area = rows_area(vp);
    const uint8_t visible = static_cast<uint8_t>((area.height + m.gap) / (m.row_height + m.gap));
    uint8_t slot = 0;
    if (!focus::hit_rows(area, visible ? visible : 1, m.row_height, m.gap, x, y, &slot))
        return false;
    if (slot >= count)
        return false;
    *index = slot;
    return true;
}
} // namespace xreader::ui
