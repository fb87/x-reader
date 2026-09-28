#include "screen.hpp"

#include "ui/chrome.hpp"
#include "ui/library.hpp"
#include "ui/navigation.hpp"
#include "ui/navigation/focus.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static screen_command_t activate_home(screen_state_t* state, home_action_t action)
{
    state->home_focus = action;
    if (action == home_continue_reading || action == home_recent_books)
    {
        state->screen = screen_reader;
        return screen_command_open_reader;
    }
    if (action == home_library)
    {
        state->screen = screen_library;
        return screen_command_show_library;
    }
    if (action == home_settings)
    {
        state->screen = screen_settings;
        return screen_command_show_settings;
    }
    return screen_command_sleep;
}
} // namespace

void initialize(screen_state_t* state)
{
    if (state == nullptr)
        return;
    state->screen = screen_home;
    state->home_focus = home_continue_reading;
    state->library_focus = 0;
    state->settings_focus = settings_text_size;
    state->quick_focus = quick_setting_text_size;
}

screen_command_t dispatch(screen_state_t* state, const input::action_event_t* event,
                          const screen_context_t* context)
{
    if (state == nullptr || event == nullptr || context == nullptr)
        return screen_command_none;

    if (event->action == input::action_power)
        return screen_command_sleep;
    if (event->action == input::action_home && state->screen != screen_home)
    {
        state->screen = screen_home;
        return screen_command_show_home;
    }

    if (state->screen == screen_home)
    {
        if (event->action == input::action_down || event->action == input::action_right)
        {
            state->home_focus = static_cast<home_action_t>(
                focus::next(static_cast<uint8_t>(state->home_focus), home_action_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_up || event->action == input::action_left)
        {
            state->home_focus = static_cast<home_action_t>(
                focus::previous(static_cast<uint8_t>(state->home_focus), home_action_count));
            return screen_command_redraw;
        }
        home_action_t action = state->home_focus;
        if (event->action == input::action_pointer)
        {
            if (!home_touch_action(context->viewport.width, context->viewport.height, event->x,
                                   event->y, &action))
                return screen_command_none;
        }
        else if (event->action != input::action_select)
            return screen_command_none;
        return activate_home(state, action);
    }

    if (state->screen == screen_library)
    {
        const uint8_t count = context->library_count == 0 ? 1 : context->library_count;
        if (event->action == input::action_down || event->action == input::action_right)
        {
            state->library_focus = focus::next(state->library_focus, count);
            return screen_command_redraw;
        }
        if (event->action == input::action_up || event->action == input::action_left)
        {
            state->library_focus = focus::previous(state->library_focus, count);
            return screen_command_redraw;
        }
        if (event->action == input::action_back)
        {
            state->screen = screen_home;
            return screen_command_show_home;
        }
        if (event->action == input::action_pointer)
        {
            uint8_t index = 0;
            if (!library_touch_index(context->viewport.width, context->viewport.height, event->x,
                                     event->y, context->library_count, &index))
                return screen_command_none;
            state->library_focus = index;
        }
        else if (event->action != input::action_select)
            return screen_command_none;
        if (context->library_count == 0)
            return screen_command_none;
        state->screen = screen_reader;
        return screen_command_open_reader;
    }

    if (state->screen == screen_settings)
    {
        if (event->action == input::action_down)
        {
            state->settings_focus = static_cast<settings_item_t>(
                focus::next(static_cast<uint8_t>(state->settings_focus), settings_item_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_up)
        {
            state->settings_focus = static_cast<settings_item_t>(
                focus::previous(static_cast<uint8_t>(state->settings_focus), settings_item_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_back || event->action == input::action_left)
        {
            state->screen = screen_home;
            return screen_command_show_home;
        }
        settings_item_t item = state->settings_focus;
        if (event->action == input::action_pointer)
        {
            if (!settings_touch_item(context->viewport.width, context->viewport.height, event->x,
                                     event->y, &item))
                return screen_command_none;
            state->settings_focus = item;
        }
        else if (event->action != input::action_select && event->action != input::action_right)
            return screen_command_none;
        if (item == settings_back)
        {
            state->screen = screen_home;
            return screen_command_show_home;
        }
        return screen_command_edit_setting;
    }

    if (state->screen == screen_quick_settings)
    {
        if (event->action == input::action_down)
        {
            state->quick_focus = static_cast<quick_setting_t>(
                focus::next(static_cast<uint8_t>(state->quick_focus), quick_setting_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_up)
        {
            state->quick_focus = static_cast<quick_setting_t>(
                focus::previous(static_cast<uint8_t>(state->quick_focus), quick_setting_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_back || event->action == input::action_menu)
        {
            state->screen = screen_reader;
            return screen_command_close_quick_settings;
        }
        if (event->action == input::action_pointer)
        {
            if (quick_settings_touch(context->viewport.width, context->viewport.height, event->x,
                                     event->y, &state->quick_focus))
                return screen_command_edit_setting;
            if (!quick_settings_contains(context->viewport.width, context->viewport.height,
                                         event->x, event->y))
            {
                state->screen = screen_reader;
                return screen_command_close_quick_settings;
            }
            return screen_command_none;
        }
        if (event->action == input::action_select || event->action == input::action_left ||
            event->action == input::action_right)
            return screen_command_edit_setting;
        return screen_command_none;
    }

    if (state->screen == screen_reader)
    {
        if (event->action == input::action_back)
        {
            state->screen = screen_home;
            return screen_command_show_home;
        }
        if (event->action == input::action_select || event->action == input::action_menu ||
            (event->action == input::action_pointer &&
             event->y >=
                 context->viewport.height - layout::metrics(context->viewport).footer_height &&
             event->x > context->viewport.width / 3U &&
             event->x < context->viewport.width * 2U / 3U))
        {
            state->screen = screen_quick_settings;
            return screen_command_open_quick_settings;
        }
        const navigation_result_t result =
            navigation_result(event, context->viewport.width, context->page, context->page_count,
                              context->spine_index, context->spine_count);
        if (result == navigation_page_forward)
            return screen_command_page_forward;
        if (result == navigation_page_backward)
            return screen_command_page_backward;
        if (result == navigation_chapter_forward)
            return screen_command_chapter_forward;
        if (result == navigation_chapter_backward)
            return screen_command_chapter_backward;
    }
    return screen_command_none;
}

} // namespace ui
} // namespace xreader
