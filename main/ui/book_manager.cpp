#include "book_manager.hpp"

#include <stdio.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"
#include "ui/navigation/focus.hpp"

namespace xreader
{
namespace ui
{
namespace
{
static layout::rect_t items_area(layout::viewport_t viewport)
{
    return layout::inset(layout::content(viewport), layout::metrics(viewport).margin);
}
} // namespace

void draw_book_manager(gfx::framebuffer_t* framebuffer, book_manager_item_t focus,
                       uint16_t book_count, bool storage_mounted, uint16_t duplicate_count,
                       bool scanning, int8_t footer_focus)
{
    if (framebuffer == nullptr)
        return;

    const layout::viewport_t viewport = {framebuffer->width, framebuffer->height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "Book Manager");

    char count_text[24] = {};
    snprintf(count_text, sizeof(count_text), "%u BOOKS", static_cast<unsigned>(book_count));
    const char* const labels[book_manager_item_count] = {
        "LIBRARY", "FILE BROWSER", "IMPORT BOOKS", "STORAGE", "CLEANUP", "BACK",
    };
    // A scan runs in the background for both Import and Cleanup (the cheap file
    // move/delete is always followed by a full catalog rebuild), so both rows
    // show the same in-progress state while either is running.
    const char* const values[book_manager_item_count] = {
        count_text,
        "BROWSE SD CARD",
        scanning ? "SCANNING..." : "FROM /IMPORT",
        storage_mounted ? "AVAILABLE" : "UNAVAILABLE",
        scanning ? "SCANNING..."
                 : (duplicate_count > 0U ? "TEMP FILES / DUPES" : "REMOVE TEMP FILES"),
        "RETURN",
    };

    const layout::rect_t area = items_area(viewport);
    const uint16_t row_height = static_cast<uint16_t>(metrics.row_height + 4U);
    for (uint8_t index = 0; index < book_manager_item_count; ++index)
    {
        const layout::rect_t item =
            layout::row(area, index, book_manager_item_count, row_height, metrics.gap);
        const bool selected = footer_focus < 0 && index == static_cast<uint8_t>(focus);
        // Focus-move redraws use the panel's fast 1-bit-only refresh mode,
        // which thresholds every pixel to pure black/white -- a subtle gray
        // wash is invisible under it. Inverting to a solid black row with
        // white content survives that threshold instead.
        gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height,
                       selected ? 0x00 : 0x0f);
        const uint8_t foreground = selected ? 0x0f : 0x00;
        const uint8_t secondary = selected ? 0x0f : 0x00;
        gfx::draw_text(framebuffer, static_cast<uint16_t>(item.x + 14U),
                       static_cast<uint16_t>(item.y + (item.height - 24U) / 2U), labels[index], 1,
                       foreground);
        const uint16_t value_width = gfx::measure_text(values[index], 1);
        const uint16_t value_x =
            item.width > value_width + 14U
                ? static_cast<uint16_t>(item.x + item.width - value_width - 14U)
                : item.x;
        gfx::draw_text(framebuffer, value_x,
                       static_cast<uint16_t>(item.y + (item.height - 24U) / 2U), values[index], 1,
                       secondary);
    }
    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_list}, {"Open", gfx::icon_check},
                                {"Back", gfx::icon_arrow_back}, footer_focus);
}

bool book_manager_touch_item(uint16_t display_width, uint16_t display_height, uint16_t x,
                             uint16_t y, book_manager_item_t* item)
{
    if (item == nullptr)
        return false;
    const layout::viewport_t viewport = {display_width, display_height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    const layout::rect_t area = items_area(viewport);
    const uint16_t row_height = static_cast<uint16_t>(metrics.row_height + 4U);
    uint8_t index = 0;
    if (!focus::hit_rows(area, book_manager_item_count, row_height, metrics.gap, x, y, &index))
        return false;
    *item = static_cast<book_manager_item_t>(index);
    return true;
}

} // namespace ui
} // namespace xreader
