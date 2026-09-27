#include "settings.hpp"

#include <stdio.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static constexpr uint16_t item_left = 40;
static constexpr uint16_t item_top = 90;
static constexpr uint16_t item_height = 54;
static constexpr uint16_t item_gap = 8;
static const char* const item_labels[settings_item_count] = {
    "TEXT SIZE", "LINE SPACING", "REFRESH MODE", "SLEEP TIMEOUT", "BACK",
};
} // namespace

void draw_settings(gfx::framebuffer_t* framebuffer, settings_item_t focus,
                   const quick_settings_values_t* values)
{
    if (framebuffer == nullptr)
        return;
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "SETTINGS", "DEVICE");
    const quick_settings_values_t defaults = {2, 0, 0, 60};
    const quick_settings_values_t* current = values == nullptr ? &defaults : values;
    char sleep_value[24] = {};
    snprintf(sleep_value, sizeof(sleep_value), "%u MINUTES",
             static_cast<unsigned>(current->sleep_timeout_minutes));
    const char* item_values[settings_item_count] = {
        current->text_scale == 1 ? "SMALL" : "MEDIUM",
        current->line_spacing != 0 ? "WIDE" : "NORMAL",
        current->refresh_mode != 0 ? "FAST" : "GC16",
        sleep_value,
        "RETURN",
    };
    for (uint8_t index = 0; index < settings_item_count; ++index)
    {
        const uint16_t y = static_cast<uint16_t>(item_top + index * (item_height + item_gap));
        const bool selected = index == static_cast<uint8_t>(focus);
        if (selected)
            gfx::fill_rect(framebuffer, item_left, y, framebuffer->width - item_left * 2,
                           item_height, 0x00);
        gfx::draw_text(framebuffer, static_cast<uint16_t>(item_left + 16),
                       static_cast<uint16_t>(y + 12), item_labels[index], 1,
                       selected ? 0x0f : 0x00);
        gfx::draw_text(framebuffer, static_cast<uint16_t>(item_left + 16),
                       static_cast<uint16_t>(y + 31), item_values[index], 1,
                       selected ? 0x0f : 0x04);
        if (!selected)
            gfx::draw_rect(framebuffer, item_left, y, framebuffer->width - item_left * 2,
                           item_height, 0x04);
    }
    chrome::draw_indication_bar(framebuffer, "ROTARY", "EDIT", "PRESS");
}

bool settings_touch_item(uint16_t y, settings_item_t* item)
{
    if (item == nullptr || y < item_top)
        return false;
    const uint16_t stride = item_height + item_gap;
    const uint16_t index = static_cast<uint16_t>((y - item_top) / stride);
    if (index >= settings_item_count || (y - item_top) % stride >= item_height)
        return false;
    *item = static_cast<settings_item_t>(index);
    return true;
}

} // namespace ui
} // namespace xreader
