#include "settings.hpp"

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
static const char* const item_labels[settings_item_count] = {
    "TEXT SIZE", "LINE SPACING", "REFRESH MODE", "SLEEP TIMEOUT", "BACK",
};

static layout::rect_t items_area(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    return layout::inset(layout::content(vp), m.margin);
}
} // namespace

void draw_settings(gfx::framebuffer_t* framebuffer, settings_item_t focus,
                   const quick_settings_values_t* values)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "SETTINGS", "READER");

    const quick_settings_values_t defaults = {2, 0, 0, 60};
    const quick_settings_values_t* current = values == nullptr ? &defaults : values;
    char sleep_value[24] = {};
    snprintf(sleep_value, sizeof(sleep_value), "%u MIN",
             static_cast<unsigned>(current->sleep_timeout_minutes));
    const char* item_values[settings_item_count] = {
        current->text_scale == 1 ? "SMALL" : "MEDIUM",
        current->line_spacing != 0 ? "WIDE" : "NORMAL",
        current->refresh_mode != 0 ? "FAST" : "QUALITY",
        sleep_value,
        "RETURN",
    };

    const layout::rect_t area = items_area(vp);
    for (uint8_t index = 0; index < settings_item_count; ++index)
    {
        const layout::rect_t item = layout::row(area, index, settings_item_count,
                                                static_cast<uint16_t>(m.row_height + 4U), m.gap);
        const bool selected = index == static_cast<uint8_t>(focus);
        if (selected)
            gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height, 0x00);
        else
        {
            gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height, 0x0f);
            gfx::fill_rect(framebuffer, item.x, static_cast<uint16_t>(item.y + item.height - 1U),
                           item.width, 1, 0x0b);
        }
        const uint8_t fg = selected ? 0x0f : 0x00;
        const uint8_t secondary = selected ? 0x0c : 0x06;
        gfx::draw_text(framebuffer, static_cast<uint16_t>(item.x + 14U),
                       static_cast<uint16_t>(item.y + (item.height - 16U) / 2U), item_labels[index],
                       1, fg);
        const uint16_t value_width = gfx::measure_text(item_values[index], 1);
        const uint16_t value_x =
            item.width > value_width + 14U
                ? static_cast<uint16_t>(item.x + item.width - value_width - 14U)
                : item.x;
        gfx::draw_text(framebuffer, value_x,
                       static_cast<uint16_t>(item.y + (item.height - 16U) / 2U), item_values[index],
                       1, secondary);
    }
    chrome::draw_indication_bar(framebuffer, "MOVE", "CHANGE", "BACK");
}

bool settings_touch_item(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y,
                         settings_item_t* item)
{
    if (item == nullptr)
        return false;
    const layout::viewport_t vp = {display_width, display_height};
    const layout::metrics_t m = layout::metrics(vp);
    uint8_t index = 0;
    if (!focus::hit_rows(items_area(vp), settings_item_count,
                         static_cast<uint16_t>(m.row_height + 4U), m.gap, x, y, &index))
        return false;
    *item = static_cast<settings_item_t>(index);
    return true;
}

} // namespace ui
} // namespace xreader
