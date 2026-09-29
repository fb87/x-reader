#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"
#include "ui/quick_settings.hpp"
#include "ui/settings_panel.hpp"

namespace xreader
{
namespace ui
{

// Mockup 10.
enum reading_setting_t : uint8_t
{
    reading_setting_font_size,
    reading_setting_line_spacing,
    reading_setting_margins,
    reading_setting_paragraph_gap,
    reading_setting_alignment,
    reading_setting_page_turn,
    reading_setting_back,
    reading_setting_count,
};

void draw_reading_settings(gfx::framebuffer_t* framebuffer, reading_setting_t focus,
                           const quick_settings_values_t* values);
bool reading_settings_hit(layout::viewport_t viewport, uint16_t x, uint16_t y, settings_hit_t* hit);

} // namespace ui
} // namespace xreader
