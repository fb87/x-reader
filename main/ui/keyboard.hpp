#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gfx/framebuffer.hpp"
#include "input/action.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{

enum keyboard_mode_t : uint8_t
{
    keyboard_qwerty,
    keyboard_t9,
};

enum keyboard_purpose_t : uint8_t
{
    keyboard_purpose_none,
    keyboard_purpose_wifi_ssid,
    keyboard_purpose_wifi_password,
    keyboard_purpose_sync_server,
    keyboard_purpose_search,
};

enum keyboard_result_t : uint8_t
{
    keyboard_result_none,
    keyboard_result_redraw,
    keyboard_result_submit,
    keyboard_result_cancel,
};

struct keyboard_state_t
{
    keyboard_mode_t mode;
    keyboard_purpose_t purpose;
    uint8_t focus_row;
    uint8_t focus_col;
    bool shifted;
    bool masked;
    bool t9_pending;
    uint8_t t9_group;
    uint8_t t9_index;
    char title[24];
    char text[96];
};

void keyboard_begin(keyboard_state_t* state, keyboard_purpose_t purpose, const char* title,
                    const char* initial, bool masked, keyboard_mode_t mode = keyboard_qwerty);
void draw_keyboard(gfx::framebuffer_t* framebuffer, const keyboard_state_t* state);
keyboard_result_t keyboard_handle(keyboard_state_t* state, const input::action_event_t* event,
                                  layout::viewport_t viewport);

} // namespace ui
} // namespace xreader
