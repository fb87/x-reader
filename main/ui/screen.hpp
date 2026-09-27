#pragma once

#include <stdint.h>

#include "ui/home.hpp"
#include "ui/quick_settings.hpp"
#include "ui/settings.hpp"

namespace xreader
{
namespace ui
{

enum screen_t : uint8_t
{
    screen_home,
    screen_library,
    screen_settings,
    screen_reader,
    screen_quick_settings,
};

enum logical_event_type_t : uint8_t
{
    logical_touch_up,
    logical_rotary_clockwise,
    logical_rotary_counterclockwise,
    logical_button_up,
};

struct logical_event_t
{
    logical_event_type_t type;
    uint16_t x;
    uint16_t y;
};

struct screen_state_t
{
    screen_t screen;
    home_action_t home_focus;
    settings_item_t settings_focus;
    quick_setting_t quick_focus;
};

enum screen_command_t : uint8_t
{
    screen_command_none,
    screen_command_redraw,
    screen_command_open_reader,
    screen_command_show_library,
    screen_command_show_settings,
    screen_command_show_home,
    screen_command_edit_setting,
    screen_command_open_quick_settings,
    screen_command_close_quick_settings,
    screen_command_page_forward,
    screen_command_page_backward,
    screen_command_chapter_forward,
    screen_command_chapter_backward,
    screen_command_sleep,
};

void initialize(screen_state_t* state);
screen_command_t dispatch(screen_state_t* state, const logical_event_t* event,
                          uint16_t display_width, uint16_t display_height, uint8_t page,
                          uint8_t page_count, uint8_t spine_index, uint8_t spine_count);

} // namespace ui
} // namespace xreader
