#pragma once

#include <stdint.h>

#include "input/action.hpp"
#include "ui/book_manager.hpp"
#include "ui/book_sync.hpp"
#include "ui/bookmarks.hpp"
#include "ui/connectivity.hpp"
#include "ui/home.hpp"
#include "ui/keyboard.hpp"
#include "ui/layout/layout.hpp"
#include "ui/ota.hpp"
#include "ui/quick_settings.hpp"
#include "ui/settings.hpp"
#include "ui/wifi_networks.hpp"

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
    screen_contents,
    screen_bookmarks,
    screen_book_info,
    screen_connectivity,
    screen_ota,
    screen_book_manager,
    screen_book_sync,
    screen_wifi_networks,
    screen_keyboard,
};

struct screen_state_t
{
    screen_t screen;
    home_action_t home_focus;
    uint8_t library_focus;
    settings_item_t settings_focus;
    quick_setting_t quick_focus;
    uint8_t contents_focus;
    uint8_t bookmarks_focus;
    connectivity_item_t connectivity_focus;
    ota_item_t ota_focus;
    book_manager_item_t book_manager_focus;
    book_sync_item_t book_sync_focus;
    uint8_t wifi_network_focus;
    keyboard_state_t keyboard;
    char pending_wifi_ssid[33];
};

struct screen_context_t
{
    layout::viewport_t viewport;
    uint8_t library_count;
    uint8_t page;
    uint8_t page_count;
    uint8_t spine_index;
    uint8_t spine_count;
    uint8_t toc_count;
    uint8_t bookmark_count;
    uint8_t wifi_network_count;
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
    screen_command_show_contents,
    screen_command_show_bookmarks,
    screen_command_show_book_info,
    screen_command_add_bookmark,
    screen_command_open_contents_item,
    screen_command_open_bookmark,
    screen_command_show_connectivity,
    screen_command_show_ota,
    screen_command_show_book_manager,
    screen_command_show_book_sync,
    screen_command_connectivity_action,
    screen_command_ota_action,
    screen_command_book_manager_action,
    screen_command_book_sync_action,
    screen_command_show_wifi_networks,
    screen_command_rescan_wifi,
    screen_command_select_wifi_network,
    screen_command_show_keyboard,
    screen_command_keyboard_submit,
    screen_command_keyboard_cancel,
};

void initialize(screen_state_t* state);
screen_command_t dispatch(screen_state_t* state, const input::action_event_t* event,
                          const screen_context_t* context);

} // namespace ui
} // namespace xreader
