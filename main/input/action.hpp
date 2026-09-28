#pragma once

#include <stdint.h>

namespace xreader
{
namespace input
{

enum action_t : uint8_t
{
    action_none,
    action_pointer,
    action_up,
    action_down,
    action_left,
    action_right,
    action_select,
    action_back,
    action_menu,
    action_page_next,
    action_page_prev,
    action_home,
    action_power,
};

struct action_event_t
{
    action_t action;
    uint16_t x;
    uint16_t y;
};

} // namespace input
} // namespace xreader
