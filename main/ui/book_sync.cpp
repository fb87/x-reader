#include "book_sync.hpp"

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

void draw_book_sync(gfx::framebuffer_t* framebuffer, book_sync_item_t focus, bool connected,
                    bool sync_books, bool sync_progress, bool server_configured,
                    const char* last_sync)
{
    if (framebuffer == nullptr)
        return;

    const layout::viewport_t viewport = {framebuffer->width, framebuffer->height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "BOOK SYNC", connected ? "ONLINE" : "OFFLINE");

    const char* const labels[book_sync_item_count] = {
        "SYNC NOW", "BOOK FILES", "READING PROGRESS", "SYNC SERVER", "BACK",
    };
    const char* const default_values[book_sync_item_count] = {
        connected ? "READY" : "NEEDS NETWORK",
        sync_books ? "ENABLED" : "DISABLED",
        sync_progress ? "ENABLED" : "DISABLED",
        server_configured ? "CONFIGURED" : "NOT CONFIGURED",
        "RETURN",
    };

    const layout::rect_t area = items_area(viewport);
    for (uint8_t index = 0; index < book_sync_item_count; ++index)
    {
        const layout::rect_t item =
            layout::row(area, index, book_sync_item_count,
                        static_cast<uint16_t>(metrics.row_height + 4U), metrics.gap);
        const bool selected = index == static_cast<uint8_t>(focus);
        gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height,
                       selected ? 0x00 : 0x0f);
        const uint8_t foreground = selected ? 0x0f : 0x00;
        const uint8_t secondary = selected ? 0x0c : 0x06;
        gfx::draw_text(framebuffer, static_cast<uint16_t>(item.x + 14U),
                       static_cast<uint16_t>(item.y + (item.height - 16U) / 2U), labels[index], 1,
                       foreground);
        const char* value = index == book_sync_now && last_sync != nullptr && last_sync[0] != '\0'
                                ? last_sync
                                : default_values[index];
        const uint16_t value_width = gfx::measure_text(value, 1);
        const uint16_t value_x =
            item.width > value_width + 14U
                ? static_cast<uint16_t>(item.x + item.width - value_width - 14U)
                : item.x;
        gfx::draw_text(framebuffer, value_x,
                       static_cast<uint16_t>(item.y + (item.height - 16U) / 2U), value, 1,
                       secondary);
    }
    chrome::draw_indication_bar(framebuffer, "MOVE", "SELECT", "BACK");
}

bool book_sync_touch_item(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y,
                          book_sync_item_t* item)
{
    if (item == nullptr)
        return false;
    const layout::viewport_t viewport = {display_width, display_height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    uint8_t index = 0;
    if (!focus::hit_rows(items_area(viewport), book_sync_item_count,
                         static_cast<uint16_t>(metrics.row_height + 4U), metrics.gap, x, y, &index))
        return false;
    *item = static_cast<book_sync_item_t>(index);
    return true;
}

} // namespace ui
} // namespace xreader
