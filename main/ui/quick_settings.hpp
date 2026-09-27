#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{

enum quick_setting_t : uint8_t
{
    quick_setting_text_size,
    quick_setting_line_spacing,
    quick_setting_refresh_mode,
    quick_setting_sleep_timeout,
    quick_setting_count,
};

void draw_quick_settings(gfx::framebuffer_t* framebuffer, quick_setting_t focus);
bool quick_settings_touch(uint16_t x, uint16_t y, quick_setting_t* setting);

} // namespace ui
} // namespace xreader
