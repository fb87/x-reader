#include "book_sync.hpp"

#include <stdio.h>
#include <string.h>

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
                    const char* last_sync, const char* activity, uint16_t completed,
                    uint16_t total, bool pending_retry, uint8_t retry_count,
                    const char* last_result, uint32_t bytes_downloaded, uint32_t bytes_total,
                    const char* history1, const char* history2)
{
    if (framebuffer == nullptr)
        return;

    const layout::viewport_t viewport = {framebuffer->width, framebuffer->height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "Book Sync");

    const char* const labels[book_sync_item_count] = {
        "SYNC NOW", "BOOK FILES", "READING PROGRESS", "SYNC SERVER", "HISTORY", "BACK",
    };
    const char* const default_values[book_sync_item_count] = {
        connected ? "READY" : "NEEDS NETWORK",
        sync_books ? "ENABLED" : "DISABLED",
        sync_progress ? "ENABLED" : "DISABLED",
        server_configured ? "CONFIGURED" : "NOT CONFIGURED",
        last_result != nullptr && last_result[0] != '\0' ? last_result : "NO HISTORY",
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
                       selected ? 0x0d : 0x0f);
        const uint8_t foreground = 0x00;
        const uint8_t secondary = selected ? 0x04 : 0x06;
        gfx::draw_text(framebuffer, static_cast<uint16_t>(item.x + 14U),
                       static_cast<uint16_t>(item.y + (item.height - 16U) / 2U), labels[index], 1,
                       foreground);
        char sync_value[48] = {};
        const char* value = default_values[index];
        if (index == book_sync_now)
        {
            if (activity != nullptr && activity[0] != '\0' && strcmp(activity, "IDLE") != 0)
            {
                if (total > 0)
                    snprintf(sync_value, sizeof(sync_value), "%s %u/%u", activity,
                             static_cast<unsigned>(completed), static_cast<unsigned>(total));
                else
                    snprintf(sync_value, sizeof(sync_value), "%s", activity);
                value = sync_value;
            }
            else if (last_sync != nullptr && last_sync[0] != '\0')
                value = last_sync;
        }
        const uint16_t value_width = gfx::measure_text(value, 1);
        const uint16_t value_x =
            item.width > value_width + 14U
                ? static_cast<uint16_t>(item.x + item.width - value_width - 14U)
                : item.x;
        gfx::draw_text(framebuffer, value_x,
                       static_cast<uint16_t>(item.y + (item.height - 16U) / 2U), value, 1,
                       secondary);
    }
    char detail[112] = {};
    if (focus == book_sync_history && history1 != nullptr && history1[0] != '\0')
        snprintf(detail, sizeof(detail), "1: %.42s  2: %.42s", history1,
                 history2 != nullptr ? history2 : "");
    else if (pending_retry)
        snprintf(detail, sizeof(detail), "OFFLINE RETRY #%u PENDING", static_cast<unsigned>(retry_count));
    else if (bytes_total > 0U)
        snprintf(detail, sizeof(detail), "%lu / %lu KB", static_cast<unsigned long>(bytes_downloaded / 1024U),
                 static_cast<unsigned long>(bytes_total / 1024U));
    else if (last_result != nullptr && last_result[0] != '\0')
        snprintf(detail, sizeof(detail), "LAST: %.92s", last_result);
    if (detail[0] != '\0')
    {
        const uint16_t y = viewport.height > metrics.footer_height + 24U
                               ? static_cast<uint16_t>(viewport.height - metrics.footer_height - 22U)
                               : 0U;
        gfx::draw_text(framebuffer, metrics.margin, y, detail, 1, 0x06);
    }
    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_list},
                                {"Select", gfx::icon_check},
                                {"Back", gfx::icon_arrow_back});
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
                         static_cast<uint16_t>(metrics.row_height + 4U), metrics.gap, x, y,
                         &index))
        return false;
    *item = static_cast<book_sync_item_t>(index);
    return true;
}

} // namespace ui
} // namespace xreader
