#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"
#include "ui/quick_settings.hpp"
#include "ui/settings_panel.hpp"

namespace xreader
{
namespace ui
{

// Mockup 9.  Brightness and contrast from the mockup are intentionally absent:
// the M5Paper has no frontlight and no contrast control, and a slider that moves
// nothing is worse than no slider.
enum display_setting_t : uint8_t
{
    display_setting_refresh_mode,
    display_setting_orientation,
    display_setting_invert,
    display_setting_show_clock,
    display_setting_sleep_timeout,
    display_setting_back,
    display_setting_count,
};

void draw_display_settings(gfx::framebuffer_t* framebuffer, display_setting_t focus,
                           const quick_settings_values_t* values, int8_t footer_focus);
bool display_settings_hit(layout::viewport_t viewport, uint16_t x, uint16_t y, settings_hit_t* hit);

} // namespace ui
} // namespace xreader
