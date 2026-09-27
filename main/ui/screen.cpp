#include "screen.hpp"

#include "ui/chrome.hpp"
#include "ui/navigation.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static bool home_action_at(uint16_t y, home_action_t* action)
{
    static constexpr uint16_t menu_top = 150;
    static constexpr uint16_t menu_height = 48;
    static constexpr uint16_t menu_gap = 8;
    if (action == nullptr || y < menu_top)
        return false;
    const uint16_t offset = static_cast<uint16_t>(y - menu_top);
    const uint16_t stride = menu_height + menu_gap;
    const uint16_t index = static_cast<uint16_t>(offset / stride);
    if (index >= home_action_count || offset % stride >= menu_height)
        return false;
    *action = static_cast<home_action_t>(index);
    return true;
}

static bool settings_item_at(uint16_t y, settings_item_t* item)
{
    static constexpr uint16_t item_top = 90;
    static constexpr uint16_t item_height = 54;
    static constexpr uint16_t item_gap = 8;
    if (item == nullptr || y < item_top)
        return false;
    const uint16_t offset = static_cast<uint16_t>(y - item_top);
    const uint16_t stride = item_height + item_gap;
    const uint16_t index = static_cast<uint16_t>(offset / stride);
    if (index >= settings_item_count || offset % stride >= item_height)
        return false;
    *item = static_cast<settings_item_t>(index);
    return true;
}

static bool quick_setting_at(uint16_t x, uint16_t y, quick_setting_t* setting)
{
    static constexpr uint16_t panel_left = 150;
    static constexpr uint16_t panel_width = 660;
    static constexpr uint16_t row_top = 120;
    static constexpr uint16_t row_height = 58;
    static constexpr uint16_t row_gap = 8;
    if (setting == nullptr || x < panel_left + 20 || x >= panel_left + panel_width - 20 ||
        y < row_top)
        return false;
    const uint16_t offset = static_cast<uint16_t>(y - row_top);
    const uint16_t stride = row_height + row_gap;
    const uint16_t index = static_cast<uint16_t>(offset / stride);
    if (index >= quick_setting_count || offset % stride >= row_height)
        return false;
    *setting = static_cast<quick_setting_t>(index);
    return true;
}
} // namespace

void initialize(screen_state_t* state)
{
    if (state == nullptr)
        return;
    state->screen = screen_home;
    state->home_focus = home_continue_reading;
    state->settings_focus = settings_text_size;
    state->quick_focus = quick_setting_text_size;
}

screen_command_t dispatch(screen_state_t* state, const logical_event_t* event,
                          uint16_t display_width, uint16_t display_height, uint8_t page,
                          uint8_t page_count, uint8_t spine_index, uint8_t spine_count)
{
    if (state == nullptr || event == nullptr)
        return screen_command_none;

    if (state->screen == screen_home)
    {
        if (event->type == logical_rotary_clockwise)
        {
            state->home_focus = static_cast<home_action_t>(
                (static_cast<uint8_t>(state->home_focus) + 1) % home_action_count);
            return screen_command_redraw;
        }
        if (event->type == logical_rotary_counterclockwise)
        {
            state->home_focus = static_cast<home_action_t>(
                (static_cast<uint8_t>(state->home_focus) + home_action_count - 1) %
                home_action_count);
            return screen_command_redraw;
        }
        home_action_t action = state->home_focus;
        const bool activate =
            event->type == logical_button_up ||
            (event->type == logical_touch_up && home_action_at(event->y, &action));
        if (!activate)
            return screen_command_none;
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

    if (state->screen == screen_library)
    {
        if (event->type == logical_button_up || event->type == logical_touch_up)
        {
            state->screen = screen_reader;
            return screen_command_open_reader;
        }
        return screen_command_none;
    }

    if (state->screen == screen_settings)
    {
        if (event->type == logical_rotary_clockwise)
        {
            state->settings_focus = static_cast<settings_item_t>(
                (static_cast<uint8_t>(state->settings_focus) + 1) % settings_item_count);
            return screen_command_redraw;
        }
        if (event->type == logical_rotary_counterclockwise)
        {
            state->settings_focus = static_cast<settings_item_t>(
                (static_cast<uint8_t>(state->settings_focus) + settings_item_count - 1) %
                settings_item_count);
            return screen_command_redraw;
        }
        settings_item_t item = state->settings_focus;
        if (event->type == logical_touch_up)
        {
            if (event->y < chrome::status_height)
            {
                state->screen = screen_home;
                return screen_command_show_home;
            }
            if (!settings_item_at(event->y, &item))
                return screen_command_none;
            state->settings_focus = item;
        }
        else if (event->type != logical_button_up)
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
        if (event->type == logical_rotary_clockwise)
        {
            state->quick_focus = static_cast<quick_setting_t>(
                (static_cast<uint8_t>(state->quick_focus) + 1) % quick_setting_count);
            return screen_command_redraw;
        }
        if (event->type == logical_rotary_counterclockwise)
        {
            state->quick_focus = static_cast<quick_setting_t>(
                (static_cast<uint8_t>(state->quick_focus) + quick_setting_count - 1) %
                quick_setting_count);
            return screen_command_redraw;
        }
        if (event->type == logical_button_up)
            return screen_command_edit_setting;
        if (event->type == logical_touch_up)
        {
            if (quick_setting_at(event->x, event->y, &state->quick_focus))
                return screen_command_edit_setting;
            state->screen = screen_reader;
            return screen_command_close_quick_settings;
        }
        return screen_command_none;
    }

    if (state->screen == screen_reader)
    {
        if (event->type == logical_button_up ||
            (event->type == logical_touch_up &&
             event->y >= display_height - chrome::indication_height &&
             event->x > display_width / 3 && event->x < display_width * 2 / 3))
        {
            state->screen = screen_quick_settings;
            return screen_command_open_quick_settings;
        }
        if (event->type == logical_rotary_clockwise ||
            event->type == logical_rotary_counterclockwise || event->type == logical_touch_up)
        {
            navigation_input_t input = navigation_touch_up;
            if (event->type == logical_rotary_clockwise)
                input = navigation_rotary_clockwise;
            else if (event->type == logical_rotary_counterclockwise)
                input = navigation_rotary_counterclockwise;
            const navigation_result_t result = navigation_result(
                input, event->x, display_width, page, page_count, spine_index, spine_count);
            if (result == navigation_page_forward)
                return screen_command_page_forward;
            if (result == navigation_page_backward)
                return screen_command_page_backward;
            if (result == navigation_chapter_forward)
                return screen_command_chapter_forward;
            if (result == navigation_chapter_backward)
                return screen_command_chapter_backward;
        }
    }
    return screen_command_none;
}

} // namespace ui
} // namespace xreader
