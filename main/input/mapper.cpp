#include "mapper.hpp"

namespace xreader
{
namespace input
{

namespace
{
static action_t action_for_key(key_t key)
{
    switch (key)
    {
    case key_up:
        return action_up;
    case key_down:
        return action_down;
    case key_left:
        return action_left;
    case key_right:
        return action_right;
    case key_select:
        return action_select;
    case key_back:
        return action_back;
    case key_menu:
        return action_menu;
    case key_page_next:
        return action_page_next;
    case key_page_prev:
        return action_page_prev;
    case key_home:
        return action_home;
    case key_power:
        return action_power;
    default:
        return action_none;
    }
}
} // namespace

bool map_event(const event_t* event, action_event_t* action)
{
    if (event == nullptr || action == nullptr)
        return false;

    *action = {action_none, event->x, event->y};
    switch (event->type)
    {
    case event_touch_up:
        action->action = action_pointer;
        break;
    case event_rotary_clockwise:
        action->action = action_down;
        break;
    case event_rotary_counterclockwise:
        action->action = action_up;
        break;
    case event_rotary_long_press:
        action->action = action_footer_toggle;
        break;
    case event_button_up:
        action->action = action_select;
        break;
    case event_button_long_press:
        action->action = action_home;
        break;
    case event_key_up:
    case event_key_repeat:
        action->action = action_for_key(event->key);
        break;
    default:
        return false;
    }
    return action->action != action_none;
}

} // namespace input
} // namespace xreader
