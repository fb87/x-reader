#include "settings.hpp"

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
    "TEXT SIZE",
    "LINE SPACING",
    "REFRESH MODE",
    "SLEEP TIMEOUT",
};
static const char* const item_values[settings_item_count] = {
    "MEDIUM",
    "NORMAL",
    "GC16",
    "60 MINUTES",
};
} // namespace

void draw_settings(gfx::framebuffer_t* framebuffer, settings_item_t focus)
{
    if (framebuffer == nullptr)
        return;
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "SETTINGS", "DEVICE");
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
