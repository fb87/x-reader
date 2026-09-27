#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{

enum home_action_t : uint8_t
{
    home_continue_reading,
    home_library,
    home_recent_books,
    home_settings,
    home_sleep,
    home_action_count,
};

void draw_home(gfx::framebuffer_t* framebuffer, bool storage_mounted, const char* book_title,
               home_action_t focus);
bool home_touch_action(uint16_t y, home_action_t* action);

} // namespace ui
} // namespace xreader
