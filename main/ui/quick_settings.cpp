#include "quick_settings.hpp"

#include "gfx/font.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static constexpr uint16_t panel_left = 150;
static constexpr uint16_t panel_top = 54;
static constexpr uint16_t panel_width = 660;
static constexpr uint16_t panel_height = 420;
static constexpr uint16_t row_top = 120;
static constexpr uint16_t row_height = 58;
static constexpr uint16_t row_gap = 8;
static const char* const labels[quick_setting_count] = {
    "TEXT SIZE",
    "LINE SPACING",
    "REFRESH MODE",
    "SLEEP TIMEOUT",
};
static const char* const values[quick_setting_count] = {
    "MEDIUM",
    "NORMAL",
    "GC16",
    "60 MINUTES",
};
} // namespace

void draw_quick_settings(gfx::framebuffer_t* framebuffer, quick_setting_t focus)
{
    if (framebuffer == nullptr)
        return;
    gfx::fill_rect(framebuffer, panel_left, panel_top, panel_width, panel_height, 0x0f);
    gfx::draw_rect(framebuffer, panel_left, panel_top, panel_width, panel_height, 0x00);
    gfx::draw_text(framebuffer, panel_left + 28, panel_top + 24, "QUICK SETTINGS", 2, 0x00);
    for (uint8_t index = 0; index < quick_setting_count; ++index)
    {
        const uint16_t y = static_cast<uint16_t>(row_top + index * (row_height + row_gap));
        const bool selected = index == static_cast<uint8_t>(focus);
        if (selected)
            gfx::fill_rect(framebuffer, panel_left + 20, y, panel_width - 40, row_height, 0x00);
        gfx::draw_text(framebuffer, panel_left + 38, y + 12, labels[index], 1,
                       selected ? 0x0f : 0x00);
        gfx::draw_text(framebuffer, panel_left + 38, y + 33, values[index], 1,
                       selected ? 0x0f : 0x04);
        if (!selected)
            gfx::draw_rect(framebuffer, panel_left + 20, y, panel_width - 40, row_height, 0x04);
    }
    gfx::draw_text(framebuffer, panel_left + 28, panel_top + panel_height - 28, "PRESS TO CLOSE", 1,
                   0x04);
}

bool quick_settings_touch(uint16_t x, uint16_t y, quick_setting_t* setting)
{
    if (setting == nullptr || x < panel_left + 20 || x >= panel_left + panel_width - 20 ||
        y < row_top)
        return false;
    const uint16_t stride = row_height + row_gap;
    const uint16_t index = static_cast<uint16_t>((y - row_top) / stride);
    if (index >= quick_setting_count || (y - row_top) % stride >= row_height)
        return false;
    *setting = static_cast<quick_setting_t>(index);
    return true;
}

} // namespace ui
} // namespace xreader
