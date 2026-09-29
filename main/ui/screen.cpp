#include "screen.hpp"

#include "ui/book_manager.hpp"
#include "ui/book_sync.hpp"
#include "ui/bookmarks.hpp"
#include "ui/chrome.hpp"
#include "ui/connectivity.hpp"
#include "ui/contents.hpp"
#include "ui/library.hpp"
#include "ui/navigation.hpp"
#include "ui/navigation/focus.hpp"
#include "ui/ota.hpp"

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
    if (action == home_book_manager)
    {
        state->screen = screen_book_manager;
        return screen_command_show_book_manager;
    }
    if (action == home_book_sync)
    {
        state->screen = screen_book_sync;
        return screen_command_show_book_sync;
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
    state->quick_focus = quick_setting_contents;
    state->contents_focus = 0;
    state->bookmarks_focus = 0;
    state->connectivity_focus = connectivity_wifi;
    state->ota_focus = ota_check_update;
    state->book_manager_focus = book_manager_library;
    state->book_sync_focus = book_sync_now;
    state->wifi_network_focus = 0;
    state->pending_wifi_ssid[0] = '\0';
    keyboard_begin(&state->keyboard, keyboard_purpose_none, "INPUT", "", false);
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
        if (item == settings_connectivity)
        {
            state->screen = screen_connectivity;
            return screen_command_show_connectivity;
        }
        if (item == settings_ota)
        {
            state->screen = screen_ota;
            return screen_command_show_ota;
        }
        if (item == settings_back)
        {
            state->screen = screen_home;
            return screen_command_show_home;
        }
        return screen_command_edit_setting;
    }

    if (state->screen == screen_connectivity)
    {
        if (event->action == input::action_down)
        {
            state->connectivity_focus = static_cast<connectivity_item_t>(focus::next(
                static_cast<uint8_t>(state->connectivity_focus), connectivity_item_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_up)
        {
            state->connectivity_focus = static_cast<connectivity_item_t>(focus::previous(
                static_cast<uint8_t>(state->connectivity_focus), connectivity_item_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_back || event->action == input::action_left)
        {
            state->screen = screen_settings;
            return screen_command_show_settings;
        }
        connectivity_item_t item = state->connectivity_focus;
        if (event->action == input::action_pointer)
        {
            if (!connectivity_touch_item(context->viewport.width, context->viewport.height,
                                         event->x, event->y, &item))
                return screen_command_none;
            state->connectivity_focus = item;
        }
        else if (event->action != input::action_select && event->action != input::action_right)
            return screen_command_none;
        if (item == connectivity_back)
        {
            state->screen = screen_settings;
            return screen_command_show_settings;
        }
        if (item == connectivity_network)
        {
            state->screen = screen_wifi_networks;
            state->wifi_network_focus = 0;
            return screen_command_show_wifi_networks;
        }
        return screen_command_connectivity_action;
    }

    if (state->screen == screen_wifi_networks)
    {
        const uint8_t visible = context->wifi_network_count > 6U ? 6U : context->wifi_network_count;
        const uint8_t total = static_cast<uint8_t>(visible + 2U);
        if (event->action == input::action_down)
        {
            state->wifi_network_focus = focus::next(state->wifi_network_focus, total);
            return screen_command_redraw;
        }
        if (event->action == input::action_up)
        {
            state->wifi_network_focus = focus::previous(state->wifi_network_focus, total);
            return screen_command_redraw;
        }
        if (event->action == input::action_back || event->action == input::action_left)
        {
            state->screen = screen_connectivity;
            return screen_command_show_connectivity;
        }
        uint8_t item = state->wifi_network_focus;
        if (event->action == input::action_pointer)
        {
            if (!wifi_networks_touch_item(context->viewport.width, context->viewport.height,
                                          event->x, event->y, context->wifi_network_count, &item))
                return screen_command_none;
            state->wifi_network_focus = item;
        }
        else if (event->action != input::action_select && event->action != input::action_right)
            return screen_command_none;
        if (item < visible)
            return screen_command_select_wifi_network;
        if (item == visible)
            return screen_command_rescan_wifi;
        keyboard_begin(&state->keyboard, keyboard_purpose_wifi_ssid, "HIDDEN SSID", "", false,
                       keyboard_qwerty);
        state->screen = screen_keyboard;
        return screen_command_show_keyboard;
    }

    if (state->screen == screen_keyboard)
    {
        const keyboard_result_t result =
            keyboard_handle(&state->keyboard, event, context->viewport);
        if (result == keyboard_result_redraw)
            return screen_command_redraw;
        if (result == keyboard_result_submit)
            return screen_command_keyboard_submit;
        if (result == keyboard_result_cancel)
        {
            state->screen = state->keyboard.purpose == keyboard_purpose_sync_server
                                ? screen_connectivity
                                : screen_wifi_networks;
            return screen_command_keyboard_cancel;
        }
        return screen_command_none;
    }

    if (state->screen == screen_ota)
    {
        if (event->action == input::action_down)
        {
            state->ota_focus = static_cast<ota_item_t>(
                focus::next(static_cast<uint8_t>(state->ota_focus), ota_item_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_up)
        {
            state->ota_focus = static_cast<ota_item_t>(
                focus::previous(static_cast<uint8_t>(state->ota_focus), ota_item_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_back || event->action == input::action_left)
        {
            state->screen = screen_settings;
            return screen_command_show_settings;
        }
        ota_item_t item = state->ota_focus;
        if (event->action == input::action_pointer)
        {
            if (!ota_touch_item(context->viewport.width, context->viewport.height, event->x,
                                event->y, &item))
                return screen_command_none;
            state->ota_focus = item;
        }
        else if (event->action != input::action_select && event->action != input::action_right)
            return screen_command_none;
        if (item == ota_back)
        {
            state->screen = screen_settings;
            return screen_command_show_settings;
        }
        return screen_command_ota_action;
    }

    if (state->screen == screen_book_manager)
    {
        if (event->action == input::action_down)
        {
            state->book_manager_focus = static_cast<book_manager_item_t>(focus::next(
                static_cast<uint8_t>(state->book_manager_focus), book_manager_item_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_up)
        {
            state->book_manager_focus = static_cast<book_manager_item_t>(focus::previous(
                static_cast<uint8_t>(state->book_manager_focus), book_manager_item_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_back || event->action == input::action_left)
        {
            state->screen = screen_home;
            return screen_command_show_home;
        }
        book_manager_item_t item = state->book_manager_focus;
        if (event->action == input::action_pointer)
        {
            if (!book_manager_touch_item(context->viewport.width, context->viewport.height,
                                         event->x, event->y, &item))
                return screen_command_none;
            state->book_manager_focus = item;
        }
        else if (event->action != input::action_select && event->action != input::action_right)
            return screen_command_none;
        if (item == book_manager_back)
        {
            state->screen = screen_home;
            return screen_command_show_home;
        }
        if (item == book_manager_library)
        {
            state->screen = screen_library;
            return screen_command_show_library;
        }
        return screen_command_book_manager_action;
    }

    if (state->screen == screen_book_sync)
    {
        if (event->action == input::action_down)
        {
            state->book_sync_focus = static_cast<book_sync_item_t>(
                focus::next(static_cast<uint8_t>(state->book_sync_focus), book_sync_item_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_up)
        {
            state->book_sync_focus = static_cast<book_sync_item_t>(focus::previous(
                static_cast<uint8_t>(state->book_sync_focus), book_sync_item_count));
            return screen_command_redraw;
        }
        if (event->action == input::action_back || event->action == input::action_left)
        {
            state->screen = screen_home;
            return screen_command_show_home;
        }
        book_sync_item_t item = state->book_sync_focus;
        if (event->action == input::action_pointer)
        {
            if (!book_sync_touch_item(context->viewport.width, context->viewport.height, event->x,
                                      event->y, &item))
                return screen_command_none;
            state->book_sync_focus = item;
        }
        else if (event->action != input::action_select && event->action != input::action_right)
            return screen_command_none;
        if (item == book_sync_back)
        {
            state->screen = screen_home;
            return screen_command_show_home;
        }
        return screen_command_book_sync_action;
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
        {
            if (state->quick_focus == quick_setting_contents)
            {
                state->screen = screen_contents;
                return screen_command_show_contents;
            }
            if (state->quick_focus == quick_setting_bookmarks)
            {
                state->screen = screen_bookmarks;
                return screen_command_show_bookmarks;
            }
            if (state->quick_focus == quick_setting_book_info)
            {
                state->screen = screen_book_info;
                return screen_command_show_book_info;
            }
            if (state->quick_focus == quick_setting_add_bookmark)
                return screen_command_add_bookmark;
            return screen_command_edit_setting;
        }
        return screen_command_none;
    }

    if (state->screen == screen_contents)
    {
        const uint8_t count = context->toc_count ? context->toc_count : context->spine_count;
        if (event->action == input::action_down)
        {
            state->contents_focus = focus::next(state->contents_focus, count ? count : 1);
            return screen_command_redraw;
        }
        if (event->action == input::action_up)
        {
            state->contents_focus = focus::previous(state->contents_focus, count ? count : 1);
            return screen_command_redraw;
        }
        if (event->action == input::action_back || event->action == input::action_left)
        {
            state->screen = screen_reader;
            return screen_command_close_quick_settings;
        }
        if (event->action == input::action_pointer)
        {
            uint8_t index = 0;
            if (!contents_touch_index(context->viewport.width, context->viewport.height, event->x,
                                      event->y, count, &index))
                return screen_command_none;
            state->contents_focus = index;
            return screen_command_open_contents_item;
        }
        if (event->action == input::action_select || event->action == input::action_right)
            return screen_command_open_contents_item;
        return screen_command_none;
    }

    if (state->screen == screen_bookmarks)
    {
        const uint8_t count = context->bookmark_count ? context->bookmark_count : 1;
        if (event->action == input::action_down)
        {
            state->bookmarks_focus = focus::next(state->bookmarks_focus, count);
            return screen_command_redraw;
        }
        if (event->action == input::action_up)
        {
            state->bookmarks_focus = focus::previous(state->bookmarks_focus, count);
            return screen_command_redraw;
        }
        if (event->action == input::action_back || event->action == input::action_left)
        {
            state->screen = screen_reader;
            return screen_command_close_quick_settings;
        }
        if (event->action == input::action_pointer)
        {
            uint8_t index = 0;
            if (!bookmarks_touch_index(context->viewport.width, context->viewport.height, event->x,
                                       event->y, context->bookmark_count, &index))
                return screen_command_none;
            state->bookmarks_focus = index;
            return screen_command_open_bookmark;
        }
        if (event->action == input::action_select && context->bookmark_count)
            return screen_command_open_bookmark;
        return screen_command_none;
    }

    if (state->screen == screen_book_info)
    {
        if (event->action == input::action_back || event->action == input::action_left ||
            event->action == input::action_select || event->action == input::action_pointer)
        {
            state->screen = screen_reader;
            return screen_command_close_quick_settings;
        }
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
