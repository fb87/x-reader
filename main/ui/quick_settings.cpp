#include "quick_settings.hpp"

#include <stdio.h>

#include "gfx/font.hpp"
#include "ui/layout/layout.hpp"
#include "ui/navigation/focus.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static const char* const labels[quick_setting_count] = {
    "TEXT SIZE",
    "LINE SPACING",
    "REFRESH MODE",
    "SLEEP TIMEOUT",
};

static layout::rect_t panel(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    const uint16_t width = m.display_class == layout::display_compact
                               ? static_cast<uint16_t>(vp.width * 78U / 100U)
                               : static_cast<uint16_t>(vp.width * 58U / 100U);
    const uint16_t height = static_cast<uint16_t>(vp.height * 76U / 100U);
    return {static_cast<uint16_t>((vp.width - width) / 2U),
            static_cast<uint16_t>((vp.height - height) / 2U), width, height};
}

static layout::rect_t rows_area(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    layout::rect_t area = layout::inset(panel(vp), m.panel_padding);
    const uint16_t heading = 42;
    const uint16_t footer = 22;
    if (area.height > heading + footer)
    {
        area.y = static_cast<uint16_t>(area.y + heading);
        area.height = static_cast<uint16_t>(area.height - heading - footer);
    }
    return area;
}
} // namespace

void draw_quick_settings(gfx::framebuffer_t* framebuffer, quick_setting_t focus,
                         const quick_settings_values_t* values)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    const layout::rect_t box = panel(vp);

    gfx::fill_rect(framebuffer, box.x, box.y, box.width, box.height, 0x0f);
    gfx::draw_rect(framebuffer, box.x, box.y, box.width, box.height, 0x00);
    gfx::fill_rect(framebuffer, box.x, box.y, box.width, 34, 0x00);
    gfx::draw_text(framebuffer, static_cast<uint16_t>(box.x + 14U),
                   static_cast<uint16_t>(box.y + 9U), "READER MENU", 1, 0x0f);

    const quick_settings_values_t defaults = {2, 0, 0, 60};
    const quick_settings_values_t* current = values == nullptr ? &defaults : values;
    char sleep_value[24] = {};
    snprintf(sleep_value, sizeof(sleep_value), "%u MIN",
             static_cast<unsigned>(current->sleep_timeout_minutes));
    const char* value_labels[quick_setting_count] = {
        current->text_scale == 1 ? "SMALL" : "MEDIUM",
        current->line_spacing != 0 ? "WIDE" : "NORMAL",
        current->refresh_mode != 0 ? "FAST" : "QUALITY",
        sleep_value,
    };

    const layout::rect_t area = rows_area(vp);
    for (uint8_t index = 0; index < quick_setting_count; ++index)
    {
        const layout::rect_t item = layout::row(area, index, quick_setting_count,
                                                static_cast<uint16_t>(m.row_height), m.gap);
        const bool selected = index == static_cast<uint8_t>(focus);
        if (selected)
            gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height, 0x00);
        else
            gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height, 0x0f);
        const uint8_t fg = selected ? 0x0f : 0x00;
        const uint8_t secondary = selected ? 0x0c : 0x06;
        gfx::draw_text(framebuffer, static_cast<uint16_t>(item.x + 10U),
                       static_cast<uint16_t>(item.y + (item.height - 16U) / 2U), labels[index], 1,
                       fg);
        const uint16_t value_width = gfx::measure_text(value_labels[index], 1);
        const uint16_t value_x =
            item.width > value_width + 10U
                ? static_cast<uint16_t>(item.x + item.width - value_width - 10U)
                : item.x;
        gfx::draw_text(framebuffer, value_x,
                       static_cast<uint16_t>(item.y + (item.height - 16U) / 2U),
                       value_labels[index], 1, secondary);
        if (!selected)
            gfx::fill_rect(framebuffer, item.x, static_cast<uint16_t>(item.y + item.height - 1U),
                           item.width, 1, 0x0c);
    }
    gfx::draw_text(framebuffer, static_cast<uint16_t>(box.x + 14U),
                   static_cast<uint16_t>(box.y + box.height - 22U), "BACK TO CLOSE", 1, 0x06);
}

bool quick_settings_touch(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y,
                          quick_setting_t* item)
{
    if (item == nullptr)
        return false;
    const layout::viewport_t vp = {display_width, display_height};
    const layout::metrics_t m = layout::metrics(vp);
    uint8_t index = 0;
    if (!focus::hit_rows(rows_area(vp), quick_setting_count, m.row_height, m.gap, x, y, &index))
        return false;
    *item = static_cast<quick_setting_t>(index);
    return true;
}

bool quick_settings_contains(uint16_t display_width, uint16_t display_height, uint16_t x,
                             uint16_t y)
{
    return layout::contains(panel({display_width, display_height}), x, y);
}

} // namespace ui
} // namespace xreader
