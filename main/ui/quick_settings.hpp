#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{

struct quick_settings_values_t
{
    uint8_t text_scale;
    uint8_t line_spacing;
    uint8_t margin_mode;
    uint8_t paragraph_spacing;
    uint8_t text_alignment;
    uint8_t reverse_page_turn;
    uint8_t refresh_mode;
    uint8_t orientation;
    uint8_t invert_colors;
    uint8_t show_clock;
    uint32_t sleep_timeout_minutes;
};

// Mockup 6: the reader menu is a full-screen list of destinations, not an
// overlay mixing navigation with value cycling.  The value controls moved to the
// Display and Reading settings screens.
enum quick_setting_t : uint8_t
{
    quick_setting_contents,
    quick_setting_bookmarks,
    quick_setting_add_bookmark,
    quick_setting_display_settings,
    quick_setting_reading_settings,
    quick_setting_search,
    quick_setting_book_info,
    quick_setting_exit_to_library,
    quick_setting_count,
};

void draw_quick_settings(gfx::framebuffer_t* framebuffer, quick_setting_t focus,
                         const quick_settings_values_t* values);
bool quick_settings_touch(uint16_t display_width, uint16_t display_height, quick_setting_t focus,
                          uint16_t x, uint16_t y, quick_setting_t* item);
bool quick_settings_contains(uint16_t display_width, uint16_t display_height, uint16_t x,
                             uint16_t y);

} // namespace ui
} // namespace xreader
