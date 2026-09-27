#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{

enum settings_item_t : uint8_t
{
    settings_text_size,
    settings_line_spacing,
    settings_refresh_mode,
    settings_sleep_timeout,
    settings_item_count,
};

void draw_settings(gfx::framebuffer_t* framebuffer, settings_item_t focus);
bool settings_touch_item(uint16_t y, settings_item_t* item);

} // namespace ui
} // namespace xreader
