#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "esp_app_desc.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "board/m5paper/m5paper_board.hpp"
#include "board/m5paper/m5paper_pins.hpp"
#include "drivers/gt911/gt911.hpp"
#include "drivers/it8951e/it8951e.hpp"
#include "epub/book.hpp"
#include "gfx/framebuffer.hpp"
#include "input/input.hpp"
#include "input/mapper.hpp"
#include "storage/book_loader.hpp"
#include "storage/persistence.hpp"
#include "services/connectivity.hpp"
#include "services/ota.hpp"
#include "services/book_manager.hpp"
#include "services/file_browser.hpp"
#include "services/library_index.hpp"
#include "services/book_sync.hpp"
#include "services/debug_console.hpp"
#include "storage/sdcard/sdcard.hpp"
#include "ui/chrome.hpp"
#include "ui/book_info.hpp"
#include "ui/book_sync.hpp"
#include "ui/book_manager.hpp"
#include "ui/file_browser.hpp"
#include "ui/book_actions.hpp"
#include "ui/ota.hpp"
#include "ui/connectivity.hpp"
#include "ui/bookmarks.hpp"
#include "ui/contents.hpp"
#include "ui/hardware_test.hpp"
#include "ui/home.hpp"
#include "ui/library.hpp"
#include "ui/library_details.hpp"
#include "ui/navigation.hpp"
#include "ui/quick_settings.hpp"
#include "ui/reader.hpp"
#include "ui/screen.hpp"
#include "ui/settings.hpp"
#include "ui/keyboard.hpp"
#include "ui/wifi_networks.hpp"
#include "ui/storage_screen.hpp"
#include "ui/about.hpp"
#include "ui/dialog.hpp"

namespace xreader
{
namespace app
{

static const char* const tag = "xreader";
static const char* active_book_path = nullptr;

#if XREADER_DIAGNOSTICS
#define XR_LOGI(...) ESP_LOGI(tag, __VA_ARGS__)
#define XR_TIME_US() esp_timer_get_time()
#else
#define XR_LOGI(...)
#define XR_TIME_US() 0
#endif

static esp_err_t
transfer_dirty(gfx::framebuffer_t* framebuffer, drivers::it8951e::device_t* display,
               drivers::it8951e::refresh_mode_t refresh_mode = drivers::it8951e::refresh_gc16)
{
    esp_err_t error = gfx::present(framebuffer);
    if (error != ESP_OK)
        return error;
    uint16_t dirty_x = 0;
    uint16_t dirty_y = 0;
    uint16_t dirty_width = 0;
    uint16_t dirty_height = 0;
    if (!gfx::take_dirty(framebuffer, &dirty_x, &dirty_y, &dirty_width, &dirty_height))
        return ESP_OK;
    const uint16_t right =
        static_cast<uint16_t>(((dirty_x + dirty_width + 3U) & ~3U) > framebuffer->width
                                  ? framebuffer->width
                                  : (dirty_x + dirty_width + 3U) & ~3U);
    dirty_x = static_cast<uint16_t>(dirty_x & ~3U);
    dirty_width = static_cast<uint16_t>(right - dirty_x);
    if ((dirty_width & 3U) != 0)
        return ESP_ERR_INVALID_SIZE;
    const size_t transfer_size = gfx::size(dirty_width, dirty_height);
    [[maybe_unused]] const int64_t transfer_start = XR_TIME_US();
    XR_LOGI("graphics dirty x=%u y=%u w=%u h=%u mode=%u", static_cast<unsigned>(dirty_x),
            static_cast<unsigned>(dirty_y), static_cast<unsigned>(dirty_width),
            static_cast<unsigned>(dirty_height), static_cast<unsigned>(refresh_mode));
    uint8_t* transfer =
        static_cast<uint8_t*>(heap_caps_malloc(transfer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (transfer == nullptr)
        return ESP_ERR_NO_MEM;
    error = gfx::copy_region_4bpp(framebuffer, dirty_x, dirty_y, dirty_width, dirty_height,
                                  transfer, transfer_size);
    if (error == ESP_OK)
        error = drivers::it8951e::write_image_4bpp(display, transfer, dirty_x, dirty_y, dirty_width,
                                                   dirty_height);
    if (error == ESP_OK)
        error = drivers::it8951e::refresh(display, dirty_x, dirty_y, dirty_width, dirty_height,
                                          refresh_mode);
    heap_caps_free(transfer);
    XR_LOGI("graphics transfer done error=%s elapsed_us=%lld", esp_err_to_name(error),
            static_cast<long long>(XR_TIME_US() - transfer_start));
    return error;
}

static esp_err_t show(gfx::framebuffer_t* framebuffer, drivers::it8951e::device_t* display,
                      const char* book_path, const epub::book_t* book,
                      const epub::document_t* document, uint8_t page,
                      uint8_t page_count, const ui::reader_settings_t* settings)
{
    if (book_path != nullptr)
        active_book_path = book_path;
    [[maybe_unused]] const int64_t draw_start = XR_TIME_US();
    XR_LOGI("draw start page=%u/%u", static_cast<unsigned>(page + 1),
            static_cast<unsigned>(page_count));
    ui::draw_reader(framebuffer, book_path, book, document, page, page_count, settings);
    XR_LOGI("draw done elapsed_us=%lld", static_cast<long long>(XR_TIME_US() - draw_start));
    const drivers::it8951e::refresh_mode_t refresh_mode =
        settings != nullptr && settings->refresh_mode != 0 ? drivers::it8951e::refresh_du
                                                           : drivers::it8951e::refresh_gc16;
    return transfer_dirty(framebuffer, display, refresh_mode);
}

static esp_err_t show(gfx::framebuffer_t* framebuffer, drivers::it8951e::device_t* display,
                      const epub::book_t* book, const epub::document_t* document, uint8_t page,
                      uint8_t page_count, const ui::reader_settings_t* settings)
{
    return show(framebuffer, display, active_book_path, book, document, page, page_count, settings);
}

static bool load_book_file(const char* path, epub::book_t* book, epub::document_t* document)
{
    if (path == nullptr || book == nullptr || document == nullptr)
        return false;
    if (storage::book_loader::start(path) != ESP_OK)
        return false;
    esp_err_t result = ESP_ERR_INVALID_STATE;
    for (uint32_t wait_ms = 0; wait_ms < 15000U; wait_ms += 50U)
    {
        if (storage::book_loader::poll(book, document, &result))
            return result == ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}

static bool load_spine(const char* path, const epub::book_t* book, uint8_t spine_index,
                       epub::book_t* loaded_book, epub::document_t* document)
{
    if (storage::book_loader::start_document(path, book, spine_index) != ESP_OK)
        return false;
    esp_err_t result = ESP_ERR_INVALID_STATE;
    for (uint32_t wait_ms = 0; wait_ms < 15000; wait_ms += 50)
    {
        if (storage::book_loader::poll(loaded_book, document, &result))
            return result == ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}


static void refresh_battery_status()
{
    uint16_t millivolts = 0;
    const esp_err_t error = board::m5paper::battery_voltage_mv(&millivolts);
    if (error != ESP_OK)
    {
        ui::chrome::set_battery_status(false, 0);
        XR_LOGI("battery unavailable: %s", esp_err_to_name(error));
        return;
    }
    const uint8_t percent = board::m5paper::battery_percent(millivolts);
    ui::chrome::set_battery_status(true, percent);
    XR_LOGI("battery %umV %u%%", static_cast<unsigned>(millivolts),
            static_cast<unsigned>(percent));
}

static void enter_sleep(const char* book_path, uint32_t spine, uint32_t page)
{
    if (book_path != nullptr && book_path[0] != '\0')
        storage::persistence::save_position_for_book(book_path, spine, page);

    // Deep sleep resets the Wi-Fi driver on wake, so disconnect it cleanly first
    // instead of leaving an in-flight HTTP/Wi-Fi transaction behind. Saved NVS
    // credentials are kept and connectivity::init() reconnects on the next boot.
    services::connectivity::set_enabled(false);
    vTaskDelay(pdMS_TO_TICKS(30));

    // The power key is the primary wake source. Keep a 24-hour timer as a
    // recovery fallback rather than waking every hour and wasting battery.
    const esp_err_t error = board::m5paper::enter_deep_sleep(24ULL * 60ULL * 60ULL * 1000000ULL);
    if (error != ESP_OK)
        ESP_LOGE(tag, "deep sleep failed: %s", esp_err_to_name(error));
}

static TickType_t idle_timeout_ticks(const storage::persistence::settings_t& settings)
{
    const uint64_t milliseconds = static_cast<uint64_t>(settings.sleep_timeout_minutes) * 60ULL * 1000ULL;
    const uint64_t ticks = milliseconds / portTICK_PERIOD_MS;
    return static_cast<TickType_t>(ticks > static_cast<uint64_t>(portMAX_DELAY - 1)
                                       ? portMAX_DELAY - 1
                                       : ticks);
}

static bool idle_expired(TickType_t last_activity, const storage::persistence::settings_t& settings)
{
    return static_cast<TickType_t>(xTaskGetTickCount() - last_activity) >= idle_timeout_ticks(settings);
}
// Value editing now lives on the Display and Reading settings screens.  A touch
// on a slider or segmented control carries the chosen value; a physical select
// has none, so it cycles to the next option instead.
static void apply_display_setting(storage::persistence::settings_t* settings,
                                  ui::display_setting_t item, bool has_value, uint8_t value)
{
    if (settings == nullptr)
        return;
    switch (item)
    {
    case ui::display_setting_refresh_mode:
        settings->refresh_mode = settings->refresh_mode == 0 ? 1 : 0;
        break;
    case ui::display_setting_orientation:
        settings->orientation = settings->orientation == 0 ? 1 : 0;
        break;
    case ui::display_setting_invert:
        settings->invert_colors = settings->invert_colors == 0 ? 1 : 0;
        break;
    case ui::display_setting_show_clock:
        settings->show_clock = settings->show_clock == 0 ? 1 : 0;
        break;
    case ui::display_setting_sleep_timeout:
        settings->sleep_timeout_minutes = settings->sleep_timeout_minutes == 30
                                              ? 60
                                              : (settings->sleep_timeout_minutes == 60 ? 120 : 30);
        break;
    default:
        break;
    }
    (void)has_value;
    (void)value;
}

static void apply_reading_setting(storage::persistence::settings_t* settings,
                                  ui::reading_setting_t item, bool has_value, uint8_t value)
{
    if (settings == nullptr)
        return;
    switch (item)
    {
    case ui::reading_setting_font_size:
        settings->text_scale = has_value ? static_cast<uint8_t>(value != 0U ? 2U : 1U)
                                         : static_cast<uint8_t>(settings->text_scale == 1U ? 2U : 1U);
        break;
    case ui::reading_setting_line_spacing:
        settings->line_spacing = has_value ? static_cast<uint8_t>(value != 0U ? 1U : 0U)
                                           : static_cast<uint8_t>(settings->line_spacing == 0U ? 1U : 0U);
        break;
    case ui::reading_setting_margins:
        settings->margin_mode = has_value ? static_cast<uint8_t>(value > 2U ? 2U : value)
                                          : static_cast<uint8_t>((settings->margin_mode + 1U) % 3U);
        break;
    case ui::reading_setting_paragraph_gap:
        settings->paragraph_spacing = settings->paragraph_spacing == 0 ? 1 : 0;
        break;
    case ui::reading_setting_alignment:
        settings->text_alignment = has_value ? static_cast<uint8_t>(value > 2U ? 0U : value)
                                             : static_cast<uint8_t>((settings->text_alignment + 1U) % 3U);
        break;
    case ui::reading_setting_page_turn:
        settings->reverse_page_turn = settings->reverse_page_turn == 0 ? 1 : 0;
        break;
    default:
        break;
    }
}

static void draw_connectivity_state(gfx::framebuffer_t* framebuffer,
                                    ui::connectivity_item_t focus)
{
    const services::connectivity::state_t network = services::connectivity::snapshot();
    const services::book_sync::state_t sync = services::book_sync::snapshot();
    ui::draw_connectivity(framebuffer, focus, network.enabled, network.connected, network.ssid,
                          network.ip, sync.server[0] != '\0',
                          services::connectivity::status_text(network));
}

static uint8_t draw_wifi_networks_state(gfx::framebuffer_t* framebuffer, uint8_t focus)
{
    services::connectivity::scan_result_t results[services::connectivity::max_scan_results] = {};
    const uint8_t count = services::connectivity::scan_results(
        results, services::connectivity::max_scan_results);
    ui::wifi_network_view_t views[services::connectivity::max_scan_results] = {};
    for (uint8_t index = 0; index < count; ++index)
    {
        snprintf(views[index].ssid, sizeof(views[index].ssid), "%s", results[index].ssid);
        views[index].rssi = results[index].rssi;
        views[index].secured = results[index].secured;
    }
    const services::connectivity::state_t network = services::connectivity::snapshot();
    ui::draw_wifi_networks(framebuffer, views, count, focus,
                           services::connectivity::status_text(network));
    return count;
}

static void draw_book_sync_state(gfx::framebuffer_t* framebuffer, ui::book_sync_item_t focus);

static bool handle_connectivity_ui_command(ui::screen_command_t command,
                                           ui::screen_state_t* screen_state,
                                           gfx::framebuffer_t* framebuffer,
                                           drivers::it8951e::device_t* display)
{
    // The Book Sync SERVER row emits book_sync_action but perform_service_action
    // has no case for it, so the row was inert.  Open the same editor the
    // Connectivity screen uses and remember where to return.
    if (command == ui::screen_command_book_sync_action &&
        screen_state->screen == ui::screen_book_sync &&
        screen_state->book_sync_focus == ui::book_sync_server)
    {
        const services::book_sync::state_t sync = services::book_sync::snapshot();
        ui::keyboard_begin(&screen_state->keyboard, ui::keyboard_purpose_sync_server,
                           "SYNC SERVER", sync.server, false, ui::keyboard_qwerty);
        screen_state->return_screen = ui::screen_book_sync;
        screen_state->screen = ui::screen_keyboard;
        ui::draw_keyboard(framebuffer, &screen_state->keyboard);
        transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
        return true;
    }
    if (screen_state == nullptr || framebuffer == nullptr || display == nullptr)
        return false;

    if (command == ui::screen_command_connectivity_action)
    {
        if (screen_state->connectivity_focus == ui::connectivity_wifi)
        {
            const services::connectivity::state_t network = services::connectivity::snapshot();
            services::connectivity::set_enabled(!network.enabled);
        }
        else if (screen_state->connectivity_focus == ui::connectivity_retry)
        {
            const services::connectivity::state_t network = services::connectivity::snapshot();
            if (network.phase == services::connectivity::phase_connecting)
                services::connectivity::cancel_connect();
            else
                services::connectivity::reconnect();
        }
        else if (screen_state->connectivity_focus == ui::connectivity_sync_server)
        {
            const services::book_sync::state_t sync = services::book_sync::snapshot();
            ui::keyboard_begin(&screen_state->keyboard, ui::keyboard_purpose_sync_server,
                               "SYNC SERVER", sync.server, false, ui::keyboard_qwerty);
            screen_state->return_screen = ui::screen_connectivity;
            screen_state->screen = ui::screen_keyboard;
            ui::draw_keyboard(framebuffer, &screen_state->keyboard);
            transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
            return true;
        }
        draw_connectivity_state(framebuffer, screen_state->connectivity_focus);
        transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
        return true;
    }

    if (command == ui::screen_command_show_wifi_networks ||
        command == ui::screen_command_rescan_wifi)
    {
        services::connectivity::request_scan();
        draw_wifi_networks_state(framebuffer, screen_state->wifi_network_focus);
        transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
        return true;
    }

    if (command == ui::screen_command_select_wifi_network)
    {
        services::connectivity::scan_result_t results[services::connectivity::max_scan_results] = {};
        const uint8_t count = services::connectivity::scan_results(
            results, services::connectivity::max_scan_results);
        const uint8_t selected = screen_state->wifi_network_focus;
        if (selected < count && selected < 6U)
        {
            snprintf(screen_state->pending_wifi_ssid, sizeof(screen_state->pending_wifi_ssid),
                     "%s", results[selected].ssid);
            if (results[selected].secured)
            {
                ui::keyboard_begin(&screen_state->keyboard, ui::keyboard_purpose_wifi_password,
                                   "WI-FI PASSWORD", "", true, ui::keyboard_qwerty);
                screen_state->screen = ui::screen_keyboard;
                ui::draw_keyboard(framebuffer, &screen_state->keyboard);
            }
            else
            {
                services::connectivity::configure(screen_state->pending_wifi_ssid, "");
                screen_state->screen = ui::screen_wifi_networks;
                draw_wifi_networks_state(framebuffer, screen_state->wifi_network_focus);
            }
            transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
        }
        return true;
    }

    if (command == ui::screen_command_show_keyboard)
    {
        ui::draw_keyboard(framebuffer, &screen_state->keyboard);
        transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
        return true;
    }

    if (command == ui::screen_command_keyboard_submit)
    {
        bool handled = true;
        if (screen_state->keyboard.purpose == ui::keyboard_purpose_wifi_ssid)
        {
            if (screen_state->keyboard.text[0] != '\0')
            {
                snprintf(screen_state->pending_wifi_ssid, sizeof(screen_state->pending_wifi_ssid),
                         "%.32s", screen_state->keyboard.text);
                ui::keyboard_begin(&screen_state->keyboard, ui::keyboard_purpose_wifi_password,
                                   "WI-FI PASSWORD", "", true, ui::keyboard_qwerty);
                screen_state->screen = ui::screen_keyboard;
                ui::draw_keyboard(framebuffer, &screen_state->keyboard);
            }
        }
        else if (screen_state->keyboard.purpose == ui::keyboard_purpose_wifi_password)
        {
            services::connectivity::configure(screen_state->pending_wifi_ssid,
                                              screen_state->keyboard.text);
            screen_state->screen = ui::screen_wifi_networks;
            draw_wifi_networks_state(framebuffer, screen_state->wifi_network_focus);
        }
        else if (screen_state->keyboard.purpose == ui::keyboard_purpose_sync_server)
        {
            services::book_sync::configure_server(screen_state->keyboard.text);
            if (screen_state->return_screen == ui::screen_book_sync)
            {
                screen_state->screen = ui::screen_book_sync;
                draw_book_sync_state(framebuffer, screen_state->book_sync_focus);
            }
            else
            {
                screen_state->screen = ui::screen_connectivity;
                draw_connectivity_state(framebuffer, screen_state->connectivity_focus);
            }
        }
        else
            handled = false;
        if (!handled)
            return false;
        transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
        return true;
    }

    if (command == ui::screen_command_keyboard_cancel)
    {
        if (screen_state->keyboard.purpose != ui::keyboard_purpose_wifi_ssid &&
            screen_state->keyboard.purpose != ui::keyboard_purpose_wifi_password &&
            screen_state->keyboard.purpose != ui::keyboard_purpose_sync_server)
            return false;
        if (screen_state->screen == ui::screen_wifi_networks)
            draw_wifi_networks_state(framebuffer, screen_state->wifi_network_focus);
        else if (screen_state->screen == ui::screen_book_sync)
            draw_book_sync_state(framebuffer, screen_state->book_sync_focus);
        else
            draw_connectivity_state(framebuffer, screen_state->connectivity_focus);
        transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
        return true;
    }
    return false;
}

static void draw_ota_state(gfx::framebuffer_t* framebuffer, ui::ota_item_t focus)
{
    const services::ota::state_t ota_state = services::ota::snapshot();
    const char* status = "UP TO DATE";
    if (ota_state.manifest_url[0] == '\0')
        status = "NOT CONFIGURED";
    if (ota_state.phase == services::ota::phase_checking)
        status = "CHECKING...";
    else if (ota_state.phase == services::ota::phase_ready)
        status = "UPDATE AVAILABLE";
    else if (ota_state.phase == services::ota::phase_installing)
        status = "INSTALLING...";
    else if (ota_state.phase == services::ota::phase_error)
        status = "UPDATE ERROR";
    ui::draw_ota(framebuffer, focus, ota_state.current_version, ota_state.available_version, status,
                 ota_state.update_available, ota_state.release_notes, ota_state.progress_percent,
                 ota_state.last_result, ota_state.rollback_pending);
}

static void draw_book_sync_state(gfx::framebuffer_t* framebuffer, ui::book_sync_item_t focus)
{
    const services::connectivity::state_t network = services::connectivity::snapshot();
    const services::book_sync::state_t sync = services::book_sync::snapshot();
    ui::draw_book_sync(framebuffer, focus, network.connected, sync.sync_books,
                       sync.sync_progress, sync.server[0] != '\0',
                       sync.last_sync[0] == '\0' ? "NEVER" : sync.last_sync,
                       sync.activity, sync.books_completed, sync.books_total,
                       sync.pending_retry, sync.retry_count, sync.last_result,
                       sync.bytes_downloaded, sync.bytes_total,
                       sync.history_count > 1U ? sync.history[1] : nullptr,
                       sync.history_count > 2U ? sync.history[2] : nullptr);
}


static size_t refresh_library_catalog(const char* mount_path,
                                      services::library_index::catalog_t* catalog,
                                      char book_paths[][ui::book_path_length], size_t capacity,
                                      bool force_rebuild)
{
    if (mount_path == nullptr || catalog == nullptr || book_paths == nullptr || capacity == 0U)
        return 0U;
    const esp_err_t error = force_rebuild
                                ? services::library_index::rebuild(mount_path, catalog)
                                : services::library_index::load(mount_path, catalog);
    if (error != ESP_OK)
        return 0U;
    const size_t count = catalog->count < capacity ? catalog->count : capacity;
    for (size_t index = 0; index < count; ++index)
        snprintf(book_paths[index], ui::book_path_length, "%s", catalog->entries[index].path);
    return count;
}


static void refresh_book_paths(const services::library_index::catalog_t* catalog,
                               char book_paths[][ui::book_path_length], size_t capacity,
                               size_t* count)
{
    if (catalog == nullptr || book_paths == nullptr || count == nullptr)
        return;
    *count = catalog->count < capacity ? catalog->count : capacity;
    for (size_t i = 0; i < *count; ++i)
        snprintf(book_paths[i], ui::book_path_length, "%s", catalog->entries[i].path);
}

static uint16_t selected_catalog_index(ui::screen_t source_screen, uint8_t focus,
                                       const uint16_t* search_indices, size_t search_count,
                                       size_t book_count)
{
    if (source_screen == ui::screen_library_search_results)
    {
        if (search_count == 0U || search_indices == nullptr)
            return 0U;
        return search_indices[static_cast<size_t>(focus) % search_count];
    }
    if (book_count == 0U)
        return 0U;
    return static_cast<uint16_t>(static_cast<size_t>(focus) % book_count);
}


static const char* path_basename(const char* path)
{
    const char* result = path == nullptr ? "" : path;
    if (path != nullptr)
        for (const char* cursor = path; *cursor != '\0'; ++cursor)
            if (*cursor == '/') result = cursor + 1;
    return result;
}

static void book_name_stem(const char* path, char* output, size_t capacity)
{
    if (output == nullptr || capacity == 0U)
        return;
    snprintf(output, capacity, "%s", path_basename(path));
    const size_t length = strlen(output);
    if (length > 5U && strcmp(output + length - 5U, ".epub") == 0)
        output[length - 5U] = '\0';
}

static uint16_t catalog_index_for_path(const services::library_index::catalog_t* catalog,
                                       const char* path)
{
    if (catalog == nullptr || path == nullptr)
        return 0U;
    for (uint16_t index = 0; index < catalog->count; ++index)
        if (strcmp(catalog->entries[index].path, path) == 0)
            return index;
    return 0U;
}

static void draw_book_manager_state(gfx::framebuffer_t* framebuffer,
                                    const ui::screen_state_t* state,
                                    size_t book_count, bool mounted, uint16_t duplicate_count)
{
    ui::draw_book_manager(framebuffer,
                          state == nullptr ? ui::book_manager_library : state->book_manager_focus,
                          static_cast<uint16_t>(book_count), mounted, duplicate_count);
}

static void draw_library_view(gfx::framebuffer_t* framebuffer, bool mounted,
                              const services::library_index::catalog_t* catalog,
                              const ui::screen_state_t* state, size_t book_count,
                              const uint16_t* search_indices, size_t search_count,
                              const char* search_query, bool recent_mode)
{
    if (state != nullptr && state->screen == ui::screen_library_search_results)
    {
        char heading[32] = "Search";
        if (search_query != nullptr && search_query[0] != '\0')
            snprintf(heading, sizeof(heading), "Search: %.22s", search_query);
        ui::draw_library_catalog_view(framebuffer, mounted, catalog, search_indices, search_count,
                                      state->library_focus, heading);
        return;
    }
    ui::draw_library_catalog(framebuffer, mounted, catalog, book_count,
                             state == nullptr ? 0U : state->library_focus,
                             recent_mode ? "Recent books" : "Library");
}

static bool handle_dialog_ui_command(ui::screen_command_t command,
                                     ui::screen_state_t* screen_state,
                                     gfx::framebuffer_t* framebuffer,
                                     drivers::it8951e::device_t* display)
{
    if (screen_state == nullptr || framebuffer == nullptr || display == nullptr)
        return false;

    if (command == ui::screen_command_show_dialog)
    {
        if (screen_state->dialog_action == ui::dialog_action_forget_wifi)
        {
            ui::dialog_begin(&screen_state->dialog, ui::dialog_confirm, "FORGET NETWORK",
                             "Remove saved Wi-Fi credentials?", "FORGET", "CANCEL");
        }
        else if (screen_state->dialog_action == ui::dialog_action_ota_install)
        {
            const services::ota::state_t update = services::ota::snapshot();
            char message[128] = {};
            snprintf(message, sizeof(message), "Install firmware %s? %.72s",
                     update.available_version[0] != '\0' ? update.available_version : "update",
                     update.release_notes);
            ui::dialog_begin(&screen_state->dialog, ui::dialog_confirm, "SYSTEM UPDATE", message,
                             "INSTALL", "CANCEL");
        }
        else if (screen_state->dialog_action == ui::dialog_action_delete_book)
        {
            ui::dialog_begin(&screen_state->dialog, ui::dialog_confirm, "DELETE BOOK",
                             "Delete this EPUB from storage?", "DELETE", "CANCEL");
        }
        else
        {
            ui::dialog_begin(&screen_state->dialog, ui::dialog_info, "MESSAGE", "Nothing to do.");
        }
        ui::draw_dialog(framebuffer, &screen_state->dialog);
        transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
        return true;
    }

    if (command != ui::screen_command_dialog_accept &&
        command != ui::screen_command_dialog_cancel)
        return false;

    if (screen_state->dialog_action == ui::dialog_action_delete_book)
        return false;

    if (command == ui::screen_command_dialog_accept)
    {
        if (screen_state->dialog_action == ui::dialog_action_forget_wifi)
        {
            services::connectivity::forget_network();
        }
        else if (screen_state->dialog_action == ui::dialog_action_ota_install)
        {
            uint16_t battery_mv = 0;
            if (board::m5paper::battery_voltage_mv(&battery_mv) == ESP_OK && battery_mv < 3600U)
            {
                screen_state->dialog_action = ui::dialog_action_none;
                ui::dialog_begin(&screen_state->dialog, ui::dialog_warning, "LOW BATTERY",
                                 "Charge above 10% before installing firmware.", "OK", "");
                ui::draw_dialog(framebuffer, &screen_state->dialog);
                transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
                return true;
            }
            services::ota::request_install();
        }
    }

    const ui::screen_t target = screen_state->return_screen;
    screen_state->dialog_action = ui::dialog_action_none;
    screen_state->screen = target;
    if (target == ui::screen_connectivity)
        draw_connectivity_state(framebuffer, screen_state->connectivity_focus);
    else if (target == ui::screen_ota)
        draw_ota_state(framebuffer, screen_state->ota_focus);
    else if (target == ui::screen_book_manager)
        ui::draw_book_manager(framebuffer, screen_state->book_manager_focus, 0U, true);
    else
        ui::draw_settings(framebuffer, screen_state->settings_focus, nullptr);
    transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
    return true;
}

static void perform_service_action(const ui::screen_state_t* screen_state, const char* mount_path,
                                   services::library_index::catalog_t* catalog,
                                   char book_paths[][ui::book_path_length], size_t* book_count,
                                   const char* book_path, uint32_t spine, uint32_t page)
{
    if (screen_state == nullptr)
        return;
    if (screen_state->screen == ui::screen_ota)
    {
        if (screen_state->ota_focus == ui::ota_check_update)
            services::ota::request_check();
        else if (screen_state->ota_focus == ui::ota_install)
            services::ota::request_install();
        return;
    }
    if (screen_state->screen == ui::screen_book_manager)
    {
        if (mount_path == nullptr)
            return;
        if (screen_state->book_manager_focus == ui::book_manager_import)
            services::book_manager::import_books(mount_path);
        else if (screen_state->book_manager_focus == ui::book_manager_cleanup)
            services::book_manager::cleanup(mount_path);
        if (catalog != nullptr && book_paths != nullptr && book_count != nullptr)
            *book_count = refresh_library_catalog(mount_path, catalog, book_paths, services::library_index::max_books, true);
        return;
    }
    if (screen_state->screen == ui::screen_book_sync)
    {
        const services::book_sync::state_t sync = services::book_sync::snapshot();
        if (screen_state->book_sync_focus == ui::book_sync_now)
            services::book_sync::request_sync(book_path, spine, page);
        else if (screen_state->book_sync_focus == ui::book_sync_books)
            services::book_sync::set_sync_books(!sync.sync_books);
        else if (screen_state->book_sync_focus == ui::book_sync_progress)
            services::book_sync::set_sync_progress(!sync.sync_progress);
    }
}

#if XREADER_SIMULATE_NAVIGATION
static void send_simulated_event(QueueHandle_t events, input::event_t event)
{
    xQueueSend(events, &event, 0);
    vTaskDelay(pdMS_TO_TICKS(1200));
}

static void simulate_navigation_task(void* argument)
{
    QueueHandle_t events = static_cast<QueueHandle_t>(argument);
    vTaskDelay(pdMS_TO_TICKS(2500));
    send_simulated_event(events, {input::event_rotary_clockwise, 0, 0});
    send_simulated_event(events, {input::event_rotary_clockwise, 0, 0});
    send_simulated_event(events, {input::event_rotary_clockwise, 0, 0});
    ESP_LOGI(tag, "simulated Home -> Settings focus");
    send_simulated_event(events, {input::event_button_up, 0, 0});
    ESP_LOGI(tag, "simulated Settings -> Home");
    send_simulated_event(events, {input::event_rotary_counterclockwise, 0, 0});
    send_simulated_event(events, {input::event_rotary_counterclockwise, 0, 0});
    send_simulated_event(events, {input::event_button_up, 0, 0});
    ESP_LOGI(tag, "simulated Home -> Library");
    send_simulated_event(events, {input::event_touch_up, 480, 160});
    ESP_LOGI(tag, "simulated Library -> Reading");
    vTaskDelete(nullptr);
}
#endif


static void draw_storage_state(gfx::framebuffer_t* framebuffer, const char* mount_path,
                               bool mounted, uint16_t book_count)
{
    uint64_t total = 0;
    uint64_t free_bytes = 0;
    if (mounted && mount_path != nullptr)
    {
        (void)esp_vfs_fat_info(mount_path, &total, &free_bytes);
    }
    ui::draw_storage(framebuffer, mounted, total, free_bytes, book_count, mount_path);
}

static void draw_about_state(gfx::framebuffer_t* framebuffer)
{
    const esp_app_desc_t* description = esp_app_get_description();
    ui::draw_about(framebuffer,
                   description == nullptr ? "UNKNOWN" : description->version,
                   "M5PAPER", esp_get_idf_version(), __DATE__);
}

static void run()
{
#if XREADER_DEBUG_CONSOLE
    if (services::debug_console::start() != ESP_OK)
        ESP_LOGW(tag, "debug console unavailable");
#endif
    esp_err_t error = storage::persistence::init();
    if (error != ESP_OK)
        ESP_LOGW(tag, "NVS initialization failed: %s", esp_err_to_name(error));
    storage::persistence::settings_t settings = {};
    storage::persistence::default_settings(&settings);
    storage::persistence::load_settings(&settings);
    services::connectivity::init();
    if (services::connectivity::snapshot().ssid[0] != '\0')
        services::connectivity::set_enabled(true);
    services::ota::init();
    services::book_sync::init();
    ui::quick_settings_values_t settings_values = {
        .text_scale = settings.text_scale,
        .line_spacing = settings.line_spacing,
        .margin_mode = settings.margin_mode,
        .paragraph_spacing = settings.paragraph_spacing,
        .text_alignment = settings.text_alignment,
        .reverse_page_turn = settings.reverse_page_turn,
        .refresh_mode = settings.refresh_mode,
        .orientation = settings.orientation,
        .invert_colors = settings.invert_colors,
        .show_clock = settings.show_clock,
        .sleep_timeout_minutes = settings.sleep_timeout_minutes,
    };
    error = board::m5paper::power_on();
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "M5Paper power initialization failed: %s", esp_err_to_name(error));
        return;
    }
    refresh_battery_status();
    uint16_t startup_battery_mv = 0;
    if (board::m5paper::battery_voltage_mv(&startup_battery_mv) == ESP_OK &&
        startup_battery_mv <= 3300U)
    {
        ESP_LOGW(tag, "battery critically low (%umV); entering safe sleep",
                 static_cast<unsigned>(startup_battery_mv));
        services::connectivity::set_enabled(false);
        board::m5paper::enter_deep_sleep(60ULL * 60ULL * 1000000ULL);
        return;
    }

    const drivers::it8951e::config_t display_config = {
        .spi_host = board::m5paper::epd_spi_host,
        .sck_pin = board::m5paper::epd_sck_pin,
        .mosi_pin = board::m5paper::epd_mosi_pin,
        .miso_pin = board::m5paper::epd_miso_pin,
        .cs_pin = board::m5paper::epd_cs_pin,
        .busy_pin = board::m5paper::epd_busy_pin,
        .width = board::m5paper::display_width,
        .height = board::m5paper::display_height,
        .rotation = settings.orientation,
        .spi_frequency_hz = 10000000,
    };
    drivers::it8951e::device_t display = {};
    error = drivers::it8951e::init(&display, &display_config);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Display initialization failed: %s", esp_err_to_name(error));
        board::m5paper::power_off();
        return;
    }

    drivers::it8951e::set_inverted(&display, settings.invert_colors != 0U);
    ui::chrome::set_clock_visible(settings.show_clock != 0U);

    const drivers::gt911::config_t touch_config = {
        .port = I2C_NUM_0,
        .sda_pin = board::m5paper::touch_sda_pin,
        .scl_pin = board::m5paper::touch_scl_pin,
        .frequency_hz = 100000,
    };
    static drivers::gt911::device_t touch = {};
    error = drivers::gt911::init(&touch, &touch_config);
    if (error != ESP_OK)
    {
        ESP_LOGW(tag, "Touch initialization failed: %s", esp_err_to_name(error));
    }

    const storage::sdcard::config_t sd_config = {
        .spi_host = board::m5paper::epd_spi_host,
        .cs_pin = board::m5paper::sd_cs_pin,
        .mount_path = "/sdcard",
        .max_files = 4,
    };
    storage::sdcard::device_t sd_card = {};
    error = storage::sdcard::mount(&sd_card, &sd_config);
    if (error != ESP_OK)
    {
        ESP_LOGW(tag, "SD-card initialization failed: %s", esp_err_to_name(error));
    }

    gfx::framebuffer_t framebuffer = {};
    error = gfx::create(&framebuffer, drivers::it8951e::logical_width(&display),
                        drivers::it8951e::logical_height(&display));
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Unable to allocate display framebuffer: %s", esp_err_to_name(error));
        board::m5paper::power_off();
        return;
    }

    ui::screen_state_t screen_state = {};
    ui::initialize(&screen_state);
    ui::draw_home(&framebuffer, sd_card.mounted, nullptr, screen_state.home_focus);
    error = transfer_dirty(&framebuffer, &display);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Library display update failed: %s", esp_err_to_name(error));
        gfx::destroy(&framebuffer);
        board::m5paper::power_off();
        return;
    }
    ESP_LOGI(tag, "Home display update complete");
    error = services::ota::confirm_running_image();
    if (error != ESP_OK)
        ESP_LOGE(tag, "Unable to confirm running image: %s", esp_err_to_name(error));

    QueueHandle_t events = xQueueCreate(8, sizeof(input::event_t));
    if (events != nullptr)
    {
        const input::config_t input_config = {
            .rotary_right_pin = board::m5paper::rotary_right_pin,
            .rotary_press_pin = board::m5paper::rotary_press_pin,
            .rotary_left_pin = board::m5paper::rotary_left_pin,
            .touch = &touch,
            .poll_interval_ms = 10,
            .touch_width = board::m5paper::display_height,
            .touch_height = board::m5paper::display_width,
            .touch_rotation = 1,
        };
        input::start(&input_config, events);
    }
#if XREADER_DEBUG_CONSOLE
    services::debug_console::set_event_queue(events);
    services::debug_console::set_ui_state(&screen_state, framebuffer.width, framebuffer.height);
#endif

    epub::book_t* book = static_cast<epub::book_t*>(
        heap_caps_calloc(1, sizeof(epub::book_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    epub::document_t* document = static_cast<epub::document_t*>(
        heap_caps_calloc(1, sizeof(epub::document_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (book == nullptr || document == nullptr)
    {
        heap_caps_free(book);
        heap_caps_free(document);
        return;
    }
    services::library_index::catalog_t* library_catalog =
        static_cast<services::library_index::catalog_t*>(
            heap_caps_calloc(1, sizeof(services::library_index::catalog_t),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (library_catalog == nullptr)
        library_catalog = static_cast<services::library_index::catalog_t*>(
            heap_caps_calloc(1, sizeof(services::library_index::catalog_t), MALLOC_CAP_8BIT));
    if (library_catalog == nullptr)
    {
        heap_caps_free(book);
        heap_caps_free(document);
        return;
    }
    char book_paths[services::library_index::max_books][ui::book_path_length] = {};
    size_t book_count = sd_card.mounted
                            ? refresh_library_catalog(sd_config.mount_path, library_catalog,
                                                      book_paths, services::library_index::max_books, false)
                            : 0U;
    uint16_t library_search_indices[services::library_index::max_books] = {};
    size_t library_search_count = 0U;
    char library_search_query[96] = {};
    bool library_recent_mode = false;
    uint16_t library_detail_index = 0U;
    uint16_t duplicate_book_count = 0U;
    services::file_browser::listing_t file_listing = {};
    char book_path[ui::book_path_length] = {};
    bool have_book = false;
    for (size_t book_index = 0; book_index < book_count && !have_book; ++book_index)
    {
        have_book = load_book_file(book_paths[book_index], book, document);
        if (have_book)
            strcpy(book_path, book_paths[book_index]);
        else
            ESP_LOGW(tag, "book unavailable: %s", book_paths[book_index]);
    }
    if (!have_book)
    {
        // Stay in the normal application loop even with an empty library.  The old
        // special-case loop only redrew a handful of screens and silently skipped
        // settings/service commands, which made the footer and Settings appear dead.
        // Keeping the zeroed book/document objects alive also lets the user import or
        // discover a book without rebooting.
        ESP_LOGW(tag, "no valid EPUB book available; starting with an empty library");
        book_path[0] = '\0';
    }
    ui::draw_home(&framebuffer, sd_card.mounted, book_path[0] != '\0' ? book->title : nullptr, screen_state.home_focus);
    error = transfer_dirty(&framebuffer, &display);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Home display refresh failed: %s", esp_err_to_name(error));
        heap_caps_free(book);
        heap_caps_free(document);
        gfx::destroy(&framebuffer);
        board::m5paper::power_off();
        return;
    }
    input::flush(events);
#if XREADER_SIMULATE_NAVIGATION
    xTaskCreate(simulate_navigation_task, "xreader_nav_sim", 2048, events, 3, nullptr);
#endif
    bool open_reader = events == nullptr;
    TickType_t last_activity = xTaskGetTickCount();
    while (!open_reader)
    {
        input::event_t event = {};
        const bool service_screen = screen_state.screen == ui::screen_connectivity ||
                                    screen_state.screen == ui::screen_wifi_networks ||
                                    screen_state.screen == ui::screen_ota ||
                                    screen_state.screen == ui::screen_book_sync;
        const TickType_t wait_ticks = service_screen ? pdMS_TO_TICKS(500U) : idle_timeout_ticks(settings);
        if (xQueueReceive(events, &event, wait_ticks) != pdTRUE)
        {
            if (service_screen)
            {
                if (idle_expired(last_activity, settings))
                {
                    enter_sleep(nullptr, 0, 0);
                    continue;
                }
                services::connectivity::poll();
                const services::connectivity::state_t net_state = services::connectivity::snapshot();
                services::book_sync::poll(net_state.connected);
                if (screen_state.screen == ui::screen_wifi_networks && net_state.connected &&
                    screen_state.pending_wifi_ssid[0] != '\0')
                {
                    screen_state.pending_wifi_ssid[0] = '\0';
                    screen_state.screen = ui::screen_connectivity;
                    draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
                    transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
                    continue;
                }
                if (screen_state.screen == ui::screen_connectivity)
                    draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
                else if (screen_state.screen == ui::screen_wifi_networks)
                    draw_wifi_networks_state(&framebuffer, screen_state.wifi_network_focus);
                else if (screen_state.screen == ui::screen_ota)
                    draw_ota_state(&framebuffer, screen_state.ota_focus);
                else if (screen_state.screen == ui::screen_book_sync)
                    draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
                transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
                continue;
            }
            enter_sleep(nullptr, 0, 0);
            continue;
        }
        last_activity = xTaskGetTickCount();
        XR_LOGI("screen input type=%u x=%u y=%u screen=%u", static_cast<unsigned>(event.type),
                static_cast<unsigned>(event.x), static_cast<unsigned>(event.y),
                static_cast<unsigned>(screen_state.screen));
        input::action_event_t action_event = {};
        if (!input::map_event(&event, &action_event))
            continue;
        if (action_event.action == input::action_pointer && settings.orientation != 0)
        {
            const uint16_t physical_x = action_event.x;
            const uint16_t physical_y = action_event.y;
            action_event.x = physical_y;
            action_event.y = static_cast<uint16_t>(display_config.width - 1U - physical_x);
        }
        if (screen_state.screen == ui::screen_reader && settings.reverse_page_turn != 0U)
        {
            if (action_event.action == input::action_left)
                action_event.action = input::action_right;
            else if (action_event.action == input::action_right)
                action_event.action = input::action_left;
            else if (action_event.action == input::action_page_next)
                action_event.action = input::action_page_prev;
            else if (action_event.action == input::action_page_prev)
                action_event.action = input::action_page_next;
            else if (action_event.action == input::action_pointer)
                action_event.x = static_cast<uint16_t>(framebuffer.width - 1U - action_event.x);
        }
        const ui::screen_context_t context = {
            .viewport = {framebuffer.width, framebuffer.height},
            .library_count = static_cast<uint8_t>(screen_state.screen == ui::screen_library_search_results ? library_search_count : book_count),
            .page = 0,
            .page_count = 1,
            .spine_index = 0,
            .spine_count = book->spine_count,
            .toc_count = book->toc_count,
            .bookmark_count = 0,
            .wifi_network_count = services::connectivity::snapshot().scan_count,
            .file_browser_count = file_listing.count,
            .file_browser_at_root = services::file_browser::at_root(&file_listing),
        };
        const ui::screen_t previous_screen = screen_state.screen;
        const ui::screen_command_t command = ui::dispatch(&screen_state, &action_event, &context);
        XR_LOGI("menu command=%u screen=%u", static_cast<unsigned>(command),
                static_cast<unsigned>(screen_state.screen));
        if (handle_dialog_ui_command(command, &screen_state, &framebuffer, &display))
            continue;
        if ((command == ui::screen_command_dialog_accept ||
             command == ui::screen_command_dialog_cancel) &&
            screen_state.dialog_action == ui::dialog_action_delete_book)
        {
            const bool accepted = command == ui::screen_command_dialog_accept;
            bool deleted_current = false;
            if (accepted && library_detail_index < library_catalog->count)
            {
                char deleted_path[ui::book_path_length] = {};
                snprintf(deleted_path, sizeof(deleted_path), "%s",
                         library_catalog->entries[library_detail_index].path);
                deleted_current = strcmp(book_path, deleted_path) == 0;
                if (services::book_manager::delete_book(deleted_path) == ESP_OK)
                {
                    book_count = refresh_library_catalog(sd_config.mount_path, library_catalog,
                                                         book_paths, services::library_index::max_books, true);
                    library_search_count = 0U;
                    library_search_query[0] = '\0';
                    if (deleted_current && book_count > 0U &&
                        load_book_file(library_catalog->entries[0].path, book, document))
                        snprintf(book_path, sizeof(book_path), "%s", library_catalog->entries[0].path);
                    else if (deleted_current && book_count == 0U)
                        book_path[0] = '\0';
                }
            }
            screen_state.dialog_action = ui::dialog_action_none;
            screen_state.screen = accepted ? ui::screen_library : ui::screen_book_actions;
            if (accepted)
            {
                screen_state.library_focus = 0U;
                draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                                  book_count, library_search_indices, library_search_count,
                                  library_search_query, false);
            }
            else
            {
                ui::draw_book_actions(&framebuffer, screen_state.book_action_focus,
                                      library_detail_index < library_catalog->count
                                          ? library_catalog->entries[library_detail_index].title
                                          : "EPUB");
            }
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        XR_LOGI("screen command=%u screen=%u", static_cast<unsigned>(command),
                static_cast<unsigned>(screen_state.screen));
        if (command == ui::screen_command_library_search)
        {
            ui::keyboard_begin(&screen_state.keyboard, ui::keyboard_purpose_search, "SEARCH BOOKS",
                               library_search_query, false, ui::keyboard_qwerty);
            screen_state.screen = ui::screen_keyboard;
            ui::draw_keyboard(&framebuffer, &screen_state.keyboard);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_keyboard_submit &&
            screen_state.keyboard.purpose == ui::keyboard_purpose_search)
        {
            snprintf(library_search_query, sizeof(library_search_query), "%s",
                     screen_state.keyboard.text);
            library_search_count = services::library_index::filter(
                library_catalog, library_search_query, library_search_indices,
                services::library_index::max_books);
            screen_state.screen = ui::screen_library_search_results;
            screen_state.library_focus = 0U;
            library_recent_mode = false;
            draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                              book_count, library_search_indices, library_search_count,
                              library_search_query, library_recent_mode);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_keyboard_cancel &&
            screen_state.keyboard.purpose == ui::keyboard_purpose_search)
        {
            screen_state.screen = ui::screen_library;
            draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                              book_count, library_search_indices, library_search_count,
                              library_search_query, library_recent_mode);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_keyboard_submit &&
            screen_state.keyboard.purpose == ui::keyboard_purpose_rename)
        {
            if (library_detail_index < library_catalog->count)
            {
                char old_path[ui::book_path_length] = {};
                char renamed_path[ui::book_path_length] = {};
                snprintf(old_path, sizeof(old_path), "%s",
                         library_catalog->entries[library_detail_index].path);
                const esp_err_t rename_error = services::book_manager::rename_book(
                    old_path, screen_state.keyboard.text, renamed_path, sizeof(renamed_path));
                if (rename_error == ESP_OK)
                {
                    storage::persistence::copy_book_state(old_path, renamed_path);
                    if (strcmp(book_path, old_path) == 0)
                        snprintf(book_path, sizeof(book_path), "%s", renamed_path);
                    book_count = refresh_library_catalog(sd_config.mount_path, library_catalog,
                                                         book_paths, services::library_index::max_books, true);
                    library_detail_index = catalog_index_for_path(library_catalog, renamed_path);
                }
            }
            screen_state.screen = ui::screen_library_details;
            ui::draw_library_details(&framebuffer,
                                     library_detail_index < library_catalog->count
                                         ? &library_catalog->entries[library_detail_index]
                                         : nullptr);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_keyboard_cancel &&
            screen_state.keyboard.purpose == ui::keyboard_purpose_rename)
        {
            screen_state.screen = ui::screen_book_actions;
            ui::draw_book_actions(&framebuffer, screen_state.book_action_focus,
                                  library_detail_index < library_catalog->count
                                      ? library_catalog->entries[library_detail_index].title
                                      : "EPUB");
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }

        if (command == ui::screen_command_library_cycle_sort)
        {
            auto mode = library_catalog->sort_mode;
            mode = mode == services::library_index::sort_title ? services::library_index::sort_author
                 : mode == services::library_index::sort_author ? services::library_index::sort_recent_added
                 : mode == services::library_index::sort_recent_added ? services::library_index::sort_recent_read
                 : services::library_index::sort_title;
            services::library_index::sort(library_catalog, mode);
            services::library_index::save(sd_config.mount_path, library_catalog);
            refresh_book_paths(library_catalog, book_paths, services::library_index::max_books, &book_count);
            if (screen_state.screen == ui::screen_library_search_results)
                library_search_count = services::library_index::filter(
                    library_catalog, library_search_query, library_search_indices,
                    services::library_index::max_books);
            screen_state.library_focus = 0U;
            draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                              book_count, library_search_indices, library_search_count,
                              library_search_query, library_recent_mode);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_show_library_details)
        {
            const uint16_t selected = selected_catalog_index(previous_screen, screen_state.library_focus,
                                                             library_search_indices, library_search_count,
                                                             book_count);
            library_detail_index = selected;
            ui::draw_library_details(&framebuffer,
                                     selected < library_catalog->count ? &library_catalog->entries[selected] : nullptr);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_show_book_actions)
        {
            ui::draw_book_actions(&framebuffer, screen_state.book_action_focus,
                                  library_detail_index < library_catalog->count
                                      ? library_catalog->entries[library_detail_index].title
                                      : "EPUB");
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_book_rename && library_detail_index < library_catalog->count)
        {
            char current_name[96] = {};
            book_name_stem(library_catalog->entries[library_detail_index].path,
                           current_name, sizeof(current_name));
            screen_state.return_screen = ui::screen_book_actions;
            ui::keyboard_begin(&screen_state.keyboard, ui::keyboard_purpose_rename,
                               "RENAME BOOK", current_name, false, ui::keyboard_qwerty);
            screen_state.screen = ui::screen_keyboard;
            ui::draw_keyboard(&framebuffer, &screen_state.keyboard);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_show_recent_library)
        {
            library_recent_mode = true;
            services::library_index::sort(library_catalog, services::library_index::sort_recent_read);
            refresh_book_paths(library_catalog, book_paths, services::library_index::max_books, &book_count);
            screen_state.library_focus = 0U;
            draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                              book_count, library_search_indices, library_search_count,
                              library_search_query, library_recent_mode);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (handle_connectivity_ui_command(command, &screen_state, &framebuffer, &display))
            continue;
        if (command == ui::screen_command_open_reader)
        {
            if ((previous_screen == ui::screen_library || previous_screen == ui::screen_library_search_results || previous_screen == ui::screen_library_details || previous_screen == ui::screen_book_actions) && book_count > 0U)
            {
                const uint16_t selected = (previous_screen == ui::screen_library_details || previous_screen == ui::screen_book_actions)
                                              ? library_detail_index
                                              : selected_catalog_index(previous_screen, screen_state.library_focus,
                                                                       library_search_indices, library_search_count,
                                                                       book_count);
                if (selected < library_catalog->count && load_book_file(library_catalog->entries[selected].path, book, document))
                {
                    snprintf(book_path, sizeof(book_path), "%s", library_catalog->entries[selected].path);
                    services::library_index::mark_read(sd_config.mount_path, library_catalog, book_path);
                    services::library_index::sort(library_catalog, library_catalog->sort_mode);
                    refresh_book_paths(library_catalog, book_paths, services::library_index::max_books, &book_count);
                }
                else
                {
                    screen_state.screen = ui::screen_library;
                    draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                                      book_count, library_search_indices, library_search_count,
                                      library_search_query, library_recent_mode);
                    transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
                    continue;
                }
            }
            if (book_path[0] == '\0')
            {
                screen_state.screen = ui::screen_library;
                draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                                  book_count, library_search_indices, library_search_count,
                                  library_search_query, library_recent_mode);
                transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
                continue;
            }
            open_reader = true;
        }
        else if (command == ui::screen_command_sleep)
            enter_sleep(nullptr, 0, 0);
        else if (command == ui::screen_command_redraw)
        {
            if (screen_state.screen == ui::screen_home)
                ui::draw_home(&framebuffer, sd_card.mounted, book_path[0] != '\0' ? book->title : nullptr, screen_state.home_focus);
            else if (screen_state.screen == ui::screen_library ||
                     screen_state.screen == ui::screen_library_search_results)
                draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                                  book_count, library_search_indices, library_search_count,
                                  library_search_query, library_recent_mode);
            else if (screen_state.screen == ui::screen_settings)
                ui::draw_settings(&framebuffer, screen_state.settings_focus, &settings_values);
            else if (screen_state.screen == ui::screen_storage)
                draw_storage_state(&framebuffer, sd_config.mount_path, sd_card.mounted, static_cast<uint16_t>(book_count));
            else if (screen_state.screen == ui::screen_about)
                draw_about_state(&framebuffer);
            else if (screen_state.screen == ui::screen_dialog)
                ui::draw_dialog(&framebuffer, &screen_state.dialog);
            else if (screen_state.screen == ui::screen_connectivity)
                draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
            else if (screen_state.screen == ui::screen_wifi_networks)
                draw_wifi_networks_state(&framebuffer, screen_state.wifi_network_focus);
            else if (screen_state.screen == ui::screen_keyboard)
                ui::draw_keyboard(&framebuffer, &screen_state.keyboard);
            else if (screen_state.screen == ui::screen_ota)
                draw_ota_state(&framebuffer, screen_state.ota_focus);
            else if (screen_state.screen == ui::screen_book_manager)
            {
                duplicate_book_count = services::book_manager::duplicate_count(library_catalog);
                draw_book_manager_state(&framebuffer, &screen_state, book_count, sd_card.mounted, duplicate_book_count);
            }
            else if (screen_state.screen == ui::screen_file_browser)
                ui::draw_file_browser(&framebuffer, &file_listing, screen_state.file_browser_focus);
            else if (screen_state.screen == ui::screen_book_actions)
                ui::draw_book_actions(&framebuffer, screen_state.book_action_focus,
                                      library_detail_index < library_catalog->count
                                          ? library_catalog->entries[library_detail_index].title
                                          : "EPUB");
            else if (screen_state.screen == ui::screen_book_sync)
                draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
        }
        else if (command == ui::screen_command_show_library)
        {
            library_recent_mode = false;
            draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                              book_count, library_search_indices, library_search_count,
                              library_search_query, library_recent_mode);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_settings)
        {
            ui::draw_settings(&framebuffer, screen_state.settings_focus, &settings_values);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_storage)
        {
            draw_storage_state(&framebuffer, sd_config.mount_path, sd_card.mounted, static_cast<uint16_t>(book_count));
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_about)
        {
            draw_about_state(&framebuffer);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_connectivity)
        {
            draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_ota)
        {
            draw_ota_state(&framebuffer, screen_state.ota_focus);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_book_manager)
        {
            duplicate_book_count = services::book_manager::duplicate_count(library_catalog);
            draw_book_manager_state(&framebuffer, &screen_state, book_count, sd_card.mounted, duplicate_book_count);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_file_browser)
        {
            services::file_browser::open(sd_config.mount_path, sd_config.mount_path, &file_listing);
            screen_state.file_browser_focus = 0U;
            ui::draw_file_browser(&framebuffer, &file_listing, screen_state.file_browser_focus);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_file_browser_back)
        {
            if (services::file_browser::at_root(&file_listing))
            {
                screen_state.screen = ui::screen_book_manager;
                draw_book_manager_state(&framebuffer, &screen_state, book_count, sd_card.mounted, duplicate_book_count);
            }
            else
            {
                services::file_browser::parent(&file_listing);
                screen_state.file_browser_focus = 0U;
                ui::draw_file_browser(&framebuffer, &file_listing, screen_state.file_browser_focus);
            }
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
        }
        else if (command == ui::screen_command_file_browser_open)
        {
            if (screen_state.file_browser_focus < file_listing.count)
            {
                const auto& selected = file_listing.entries[screen_state.file_browser_focus];
                if (selected.directory)
                {
                    services::file_browser::open(file_listing.root, selected.path, &file_listing);
                    screen_state.file_browser_focus = 0U;
                    ui::draw_file_browser(&framebuffer, &file_listing, screen_state.file_browser_focus);
                    transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
                }
                else if (selected.epub && load_book_file(selected.path, book, document))
                {
                    snprintf(book_path, sizeof(book_path), "%s", selected.path);
                    screen_state.screen = ui::screen_reader;
                    open_reader = true;
                }
            }
        }
        else if (command == ui::screen_command_show_book_sync)
        {
            draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_home)
        {
            ui::draw_home(&framebuffer, sd_card.mounted, book_path[0] != '\0' ? book->title : nullptr, screen_state.home_focus);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_ota_action ||
                 command == ui::screen_command_book_manager_action ||
                 command == ui::screen_command_book_sync_action)
        {
            perform_service_action(&screen_state, sd_config.mount_path, library_catalog, book_paths, &book_count,
                                   book_path, 0, 0);
            if (screen_state.screen == ui::screen_connectivity)
                draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
            else if (screen_state.screen == ui::screen_ota)
                draw_ota_state(&framebuffer, screen_state.ota_focus);
            else if (screen_state.screen == ui::screen_book_manager)
            {
                duplicate_book_count = services::book_manager::duplicate_count(library_catalog);
                draw_book_manager_state(&framebuffer, &screen_state, book_count, sd_card.mounted, duplicate_book_count);
            }
            else if (screen_state.screen == ui::screen_file_browser)
                ui::draw_file_browser(&framebuffer, &file_listing, screen_state.file_browser_focus);
            else if (screen_state.screen == ui::screen_book_actions)
                ui::draw_book_actions(&framebuffer, screen_state.book_action_focus,
                                      library_detail_index < library_catalog->count
                                          ? library_catalog->entries[library_detail_index].title
                                          : "EPUB");
            else if (screen_state.screen == ui::screen_book_sync)
                draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
        }
        else if (command == ui::screen_command_show_display_settings)
        {
            ui::draw_display_settings(&framebuffer, screen_state.display_focus, &settings_values);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_reading_settings)
        {
            ui::draw_reading_settings(&framebuffer, screen_state.reading_focus, &settings_values);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_edit_display_setting ||
                 command == ui::screen_command_edit_reading_setting)
        {
            const uint8_t previous_orientation = settings.orientation;
            if (command == ui::screen_command_edit_display_setting)
                apply_display_setting(&settings, screen_state.display_focus,
                                      screen_state.has_pending_value, screen_state.pending_value);
            else
                apply_reading_setting(&settings, screen_state.reading_focus,
                                      screen_state.has_pending_value, screen_state.pending_value);
            if (settings.orientation != previous_orientation)
            {
                drivers::it8951e::set_rotation(&display, settings.orientation);
                gfx::destroy(&framebuffer);
                error = gfx::create(&framebuffer, drivers::it8951e::logical_width(&display),
                                    drivers::it8951e::logical_height(&display));
                if (error != ESP_OK)
                {
                    ESP_LOGE(tag, "Unable to resize framebuffer for orientation: %s",
                             esp_err_to_name(error));
                    return;
                }
            }
            drivers::it8951e::set_inverted(&display, settings.invert_colors != 0U);
            ui::chrome::set_clock_visible(settings.show_clock != 0U);
            storage::persistence::save_settings(&settings);
            settings_values.text_scale = settings.text_scale;
            settings_values.line_spacing = settings.line_spacing;
            settings_values.margin_mode = settings.margin_mode;
            settings_values.paragraph_spacing = settings.paragraph_spacing;
            settings_values.text_alignment = settings.text_alignment;
            settings_values.reverse_page_turn = settings.reverse_page_turn;
            settings_values.refresh_mode = settings.refresh_mode;
            settings_values.orientation = settings.orientation;
            settings_values.invert_colors = settings.invert_colors;
            settings_values.show_clock = settings.show_clock;
            settings_values.sleep_timeout_minutes = settings.sleep_timeout_minutes;
            if (screen_state.screen == ui::screen_display_settings)
                ui::draw_display_settings(&framebuffer, screen_state.display_focus,
                                          &settings_values);
            else
                ui::draw_reading_settings(&framebuffer, screen_state.reading_focus,
                                          &settings_values);
            transfer_dirty(&framebuffer, &display);
        }
    }
    ui::reader_settings_t reader_settings = {
        .text_scale = settings.text_scale,
        .line_spacing = settings.line_spacing,
        .refresh_mode = settings.refresh_mode,
        .margin_mode = settings.margin_mode,
        .paragraph_spacing = settings.paragraph_spacing,
        .text_alignment = settings.text_alignment,
    };
    uint8_t spine_index = 0;
    uint8_t total_pages =
        ui::page_count(document, &reader_settings, framebuffer.width, framebuffer.height);
    uint32_t saved_spine = 0;
    uint32_t saved_page = 0;
    storage::persistence::load_position_for_book(book_path, &saved_spine, &saved_page);
    if (saved_spine < book->spine_count)
        spine_index = static_cast<uint8_t>(saved_spine);
    if (spine_index != 0 && !load_spine(book_path, book, spine_index, book, document))
    {
        spine_index = 0;
    }
    total_pages =
        ui::page_count(document, &reader_settings, framebuffer.width, framebuffer.height);
    uint8_t page = saved_page < total_pages ? static_cast<uint8_t>(saved_page) : 0;
    screen_state.screen = ui::screen_reader;
    show(&framebuffer, &display, book_path, book, document, page, total_pages, &reader_settings);
    input::flush(events);
    ui::bookmark_view_t bookmarks[storage::persistence::max_bookmarks_per_book] = {};
    uint8_t bookmark_count = 0;
    storage::persistence::bookmark_t saved_bookmarks[storage::persistence::max_bookmarks_per_book] = {};
    uint8_t saved_bookmark_count = 0;
    if (storage::persistence::load_bookmarks_for_book(
            book_path, saved_bookmarks, storage::persistence::max_bookmarks_per_book,
            &saved_bookmark_count) == ESP_OK)
    {
        bookmark_count = saved_bookmark_count;
        for (uint8_t index = 0; index < bookmark_count; ++index)
        {
            bookmarks[index] = {true, static_cast<uint8_t>(saved_bookmarks[index].spine),
                                static_cast<uint8_t>(saved_bookmarks[index].page)};
        }
    }
    ui::quick_settings_values_t quick_values = {
        .text_scale = settings.text_scale,
        .line_spacing = settings.line_spacing,
        .margin_mode = settings.margin_mode,
        .paragraph_spacing = settings.paragraph_spacing,
        .text_alignment = settings.text_alignment,
        .reverse_page_turn = settings.reverse_page_turn,
        .refresh_mode = settings.refresh_mode,
        .orientation = settings.orientation,
        .invert_colors = settings.invert_colors,
        .show_clock = settings.show_clock,
        .sleep_timeout_minutes = settings.sleep_timeout_minutes,
    };
    char reader_search_query[96] = {};
    size_t reader_search_offset = 0;
    last_activity = xTaskGetTickCount();
    while (events != nullptr)
    {
        input::event_t event = {};
        const bool service_screen = screen_state.screen == ui::screen_connectivity ||
                                    screen_state.screen == ui::screen_wifi_networks ||
                                    screen_state.screen == ui::screen_ota ||
                                    screen_state.screen == ui::screen_book_sync;
        const TickType_t wait_ticks = service_screen ? pdMS_TO_TICKS(500U) : idle_timeout_ticks(settings);
        if (xQueueReceive(events, &event, wait_ticks) != pdTRUE)
        {
            if (service_screen)
            {
                if (idle_expired(last_activity, settings))
                {
                    enter_sleep(book_path, spine_index, page);
                    continue;
                }
                services::connectivity::poll();
                const services::connectivity::state_t net_state = services::connectivity::snapshot();
                services::book_sync::poll(net_state.connected);
                if (screen_state.screen == ui::screen_wifi_networks && net_state.connected &&
                    screen_state.pending_wifi_ssid[0] != '\0')
                {
                    screen_state.pending_wifi_ssid[0] = '\0';
                    screen_state.screen = ui::screen_connectivity;
                    draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
                    transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
                    continue;
                }
                if (screen_state.screen == ui::screen_connectivity)
                    draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
                else if (screen_state.screen == ui::screen_wifi_networks)
                    draw_wifi_networks_state(&framebuffer, screen_state.wifi_network_focus);
                else if (screen_state.screen == ui::screen_ota)
                    draw_ota_state(&framebuffer, screen_state.ota_focus);
                else if (screen_state.screen == ui::screen_book_sync)
                    draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
                transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
                continue;
            }
            enter_sleep(book_path, spine_index, page);
            continue;
        }
        last_activity = xTaskGetTickCount();
        XR_LOGI("reader input type=%u x=%u y=%u screen=%u", static_cast<unsigned>(event.type),
                static_cast<unsigned>(event.x), static_cast<unsigned>(event.y),
                static_cast<unsigned>(screen_state.screen));
        input::action_event_t action_event = {};
        if (!input::map_event(&event, &action_event))
            continue;
        if (action_event.action == input::action_pointer && settings.orientation != 0)
        {
            const uint16_t physical_x = action_event.x;
            const uint16_t physical_y = action_event.y;
            action_event.x = physical_y;
            action_event.y = static_cast<uint16_t>(display_config.width - 1U - physical_x);
        }
        const ui::screen_context_t context = {
            .viewport = {framebuffer.width, framebuffer.height},
            .library_count = static_cast<uint8_t>(screen_state.screen == ui::screen_library_search_results ? library_search_count : book_count),
            .page = page,
            .page_count = total_pages,
            .spine_index = spine_index,
            .spine_count = book->spine_count,
            .toc_count = book->toc_count,
            .bookmark_count = bookmark_count,
            .wifi_network_count = services::connectivity::snapshot().scan_count,
            .file_browser_count = file_listing.count,
            .file_browser_at_root = services::file_browser::at_root(&file_listing),
        };
        const ui::screen_t previous_screen = screen_state.screen;
        const ui::screen_command_t command = ui::dispatch(&screen_state, &action_event, &context);
        XR_LOGI("reader command=%u screen=%u", static_cast<unsigned>(command),
                static_cast<unsigned>(screen_state.screen));
        if (handle_dialog_ui_command(command, &screen_state, &framebuffer, &display))
            continue;
        if ((command == ui::screen_command_dialog_accept ||
             command == ui::screen_command_dialog_cancel) &&
            screen_state.dialog_action == ui::dialog_action_delete_book)
        {
            const bool accepted = command == ui::screen_command_dialog_accept;
            bool deleted_current = false;
            if (accepted && library_detail_index < library_catalog->count)
            {
                char deleted_path[ui::book_path_length] = {};
                snprintf(deleted_path, sizeof(deleted_path), "%s",
                         library_catalog->entries[library_detail_index].path);
                deleted_current = strcmp(book_path, deleted_path) == 0;
                if (services::book_manager::delete_book(deleted_path) == ESP_OK)
                {
                    book_count = refresh_library_catalog(sd_config.mount_path, library_catalog,
                                                         book_paths, services::library_index::max_books, true);
                    library_search_count = 0U;
                    library_search_query[0] = '\0';
                    if (deleted_current && book_count > 0U &&
                        load_book_file(library_catalog->entries[0].path, book, document))
                        snprintf(book_path, sizeof(book_path), "%s", library_catalog->entries[0].path);
                    else if (deleted_current && book_count == 0U)
                        book_path[0] = '\0';
                }
            }
            screen_state.dialog_action = ui::dialog_action_none;
            screen_state.screen = accepted ? ui::screen_library : ui::screen_book_actions;
            if (accepted)
            {
                screen_state.library_focus = 0U;
                draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                                  book_count, library_search_indices, library_search_count,
                                  library_search_query, false);
            }
            else
            {
                ui::draw_book_actions(&framebuffer, screen_state.book_action_focus,
                                      library_detail_index < library_catalog->count
                                          ? library_catalog->entries[library_detail_index].title
                                          : "EPUB");
            }
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_reader_search)
        {
            ui::keyboard_begin(&screen_state.keyboard, ui::keyboard_purpose_reader_search,
                               "SEARCH IN BOOK", reader_search_query, false, ui::keyboard_qwerty);
            screen_state.screen = ui::screen_keyboard;
            ui::draw_keyboard(&framebuffer, &screen_state.keyboard);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_keyboard_submit &&
            screen_state.keyboard.purpose == ui::keyboard_purpose_reader_search)
        {
            if (strcmp(reader_search_query, screen_state.keyboard.text) != 0)
                reader_search_offset = 0U;
            snprintf(reader_search_query, sizeof(reader_search_query), "%s",
                     screen_state.keyboard.text);
            const uint8_t original_spine = spine_index;
            bool found = false;
            uint8_t found_page = page;
            size_t found_offset = 0;
            if (reader_search_query[0] != '\0')
            {
                // Continue after the previous match in the current chapter first.
                found = ui::find_page(document, reader_search_query, &reader_settings,
                                      framebuffer.width, framebuffer.height,
                                      reader_search_offset, &found_page, &found_offset);
                if (!found)
                {
                    // Search following chapters, then wrap to chapters before the current one.
                    for (uint8_t pass = 0; pass < 2U && !found; ++pass)
                    {
                        const uint8_t begin = pass == 0U ? static_cast<uint8_t>(original_spine + 1U) : 0U;
                        const uint8_t end = pass == 0U ? book->spine_count : original_spine;
                        for (uint8_t target = begin; target < end && !found; ++target)
                        {
                            if (!load_spine(book_path, book, target, book, document))
                                continue;
                            const uint8_t pages = ui::page_count(document, &reader_settings,
                                                                 framebuffer.width, framebuffer.height);
                            (void)pages;
                            found = ui::find_page(document, reader_search_query, &reader_settings,
                                                  framebuffer.width, framebuffer.height, 0U,
                                                  &found_page, &found_offset);
                            if (found)
                                spine_index = target;
                        }
                    }
                    if (!found && reader_search_offset > 0U &&
                        load_spine(book_path, book, original_spine, book, document))
                    {
                        found = ui::find_page(document, reader_search_query, &reader_settings,
                                              framebuffer.width, framebuffer.height, 0U,
                                              &found_page, &found_offset);
                        if (found && found_offset >= reader_search_offset)
                            found = false;
                        if (found)
                            spine_index = original_spine;
                    }
                }
            }
            if (!found && spine_index != original_spine)
                spine_index = original_spine;
            if (!found)
            {
                load_spine(book_path, book, original_spine, book, document);
                total_pages = ui::page_count(document, &reader_settings, framebuffer.width,
                                             framebuffer.height);
                screen_state.screen = ui::screen_reader;
                show(&framebuffer, &display, book_path, book, document, page, total_pages, &reader_settings);
                reader_search_offset = 0U;
            }
            else
            {
                total_pages = ui::page_count(document, &reader_settings, framebuffer.width,
                                             framebuffer.height);
                page = found_page < total_pages ? found_page : 0U;
                reader_search_offset = found_offset + strlen(reader_search_query);
                screen_state.screen = ui::screen_reader;
                show(&framebuffer, &display, book_path, book, document, page, total_pages, &reader_settings);
            }
            continue;
        }
        if (command == ui::screen_command_keyboard_cancel &&
            screen_state.keyboard.purpose == ui::keyboard_purpose_reader_search)
        {
            screen_state.screen = ui::screen_reader;
            show(&framebuffer, &display, book_path, book, document, page, total_pages, &reader_settings);
            continue;
        }
        if (command == ui::screen_command_library_search)
        {
            ui::keyboard_begin(&screen_state.keyboard, ui::keyboard_purpose_search, "SEARCH BOOKS",
                               library_search_query, false, ui::keyboard_qwerty);
            screen_state.screen = ui::screen_keyboard;
            ui::draw_keyboard(&framebuffer, &screen_state.keyboard);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_keyboard_submit &&
            screen_state.keyboard.purpose == ui::keyboard_purpose_search)
        {
            snprintf(library_search_query, sizeof(library_search_query), "%s",
                     screen_state.keyboard.text);
            library_search_count = services::library_index::filter(
                library_catalog, library_search_query, library_search_indices,
                services::library_index::max_books);
            screen_state.screen = ui::screen_library_search_results;
            screen_state.library_focus = 0U;
            library_recent_mode = false;
            draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                              book_count, library_search_indices, library_search_count,
                              library_search_query, library_recent_mode);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_keyboard_cancel &&
            screen_state.keyboard.purpose == ui::keyboard_purpose_search)
        {
            screen_state.screen = ui::screen_library;
            draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                              book_count, library_search_indices, library_search_count,
                              library_search_query, library_recent_mode);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_keyboard_submit &&
            screen_state.keyboard.purpose == ui::keyboard_purpose_rename)
        {
            if (library_detail_index < library_catalog->count)
            {
                char old_path[ui::book_path_length] = {};
                char renamed_path[ui::book_path_length] = {};
                snprintf(old_path, sizeof(old_path), "%s",
                         library_catalog->entries[library_detail_index].path);
                const esp_err_t rename_error = services::book_manager::rename_book(
                    old_path, screen_state.keyboard.text, renamed_path, sizeof(renamed_path));
                if (rename_error == ESP_OK)
                {
                    storage::persistence::copy_book_state(old_path, renamed_path);
                    if (strcmp(book_path, old_path) == 0)
                        snprintf(book_path, sizeof(book_path), "%s", renamed_path);
                    book_count = refresh_library_catalog(sd_config.mount_path, library_catalog,
                                                         book_paths, services::library_index::max_books, true);
                    library_detail_index = catalog_index_for_path(library_catalog, renamed_path);
                }
            }
            screen_state.screen = ui::screen_library_details;
            ui::draw_library_details(&framebuffer,
                                     library_detail_index < library_catalog->count
                                         ? &library_catalog->entries[library_detail_index]
                                         : nullptr);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_keyboard_cancel &&
            screen_state.keyboard.purpose == ui::keyboard_purpose_rename)
        {
            screen_state.screen = ui::screen_book_actions;
            ui::draw_book_actions(&framebuffer, screen_state.book_action_focus,
                                  library_detail_index < library_catalog->count
                                      ? library_catalog->entries[library_detail_index].title
                                      : "EPUB");
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }

        if (command == ui::screen_command_library_cycle_sort)
        {
            auto mode = library_catalog->sort_mode;
            mode = mode == services::library_index::sort_title ? services::library_index::sort_author
                 : mode == services::library_index::sort_author ? services::library_index::sort_recent_added
                 : mode == services::library_index::sort_recent_added ? services::library_index::sort_recent_read
                 : services::library_index::sort_title;
            services::library_index::sort(library_catalog, mode);
            services::library_index::save(sd_config.mount_path, library_catalog);
            refresh_book_paths(library_catalog, book_paths, services::library_index::max_books, &book_count);
            if (screen_state.screen == ui::screen_library_search_results)
                library_search_count = services::library_index::filter(
                    library_catalog, library_search_query, library_search_indices,
                    services::library_index::max_books);
            screen_state.library_focus = 0U;
            draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                              book_count, library_search_indices, library_search_count,
                              library_search_query, library_recent_mode);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_show_library_details)
        {
            const uint16_t selected = selected_catalog_index(previous_screen, screen_state.library_focus,
                                                             library_search_indices, library_search_count,
                                                             book_count);
            library_detail_index = selected;
            ui::draw_library_details(&framebuffer,
                                     selected < library_catalog->count ? &library_catalog->entries[selected] : nullptr);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_show_book_actions)
        {
            ui::draw_book_actions(&framebuffer, screen_state.book_action_focus,
                                  library_detail_index < library_catalog->count
                                      ? library_catalog->entries[library_detail_index].title
                                      : "EPUB");
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_book_rename && library_detail_index < library_catalog->count)
        {
            char current_name[96] = {};
            book_name_stem(library_catalog->entries[library_detail_index].path,
                           current_name, sizeof(current_name));
            screen_state.return_screen = ui::screen_book_actions;
            ui::keyboard_begin(&screen_state.keyboard, ui::keyboard_purpose_rename,
                               "RENAME BOOK", current_name, false, ui::keyboard_qwerty);
            screen_state.screen = ui::screen_keyboard;
            ui::draw_keyboard(&framebuffer, &screen_state.keyboard);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_show_recent_library)
        {
            library_recent_mode = true;
            services::library_index::sort(library_catalog, services::library_index::sort_recent_read);
            refresh_book_paths(library_catalog, book_paths, services::library_index::max_books, &book_count);
            screen_state.library_focus = 0U;
            draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                              book_count, library_search_indices, library_search_count,
                              library_search_query, library_recent_mode);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (handle_connectivity_ui_command(command, &screen_state, &framebuffer, &display))
            continue;
        if (command == ui::screen_command_sleep)
        {
            enter_sleep(book_path, spine_index, page);
            continue;
        }
        if (command == ui::screen_command_show_home)
        {
            ui::draw_home(&framebuffer, sd_card.mounted, book_path[0] != '\0' ? book->title : nullptr, screen_state.home_focus);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_library)
        {
            library_recent_mode = false;
            draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                              book_count, library_search_indices, library_search_count,
                              library_search_query, library_recent_mode);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_settings)
        {
            ui::draw_settings(&framebuffer, screen_state.settings_focus, &settings_values);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_storage)
        {
            draw_storage_state(&framebuffer, sd_config.mount_path, sd_card.mounted, static_cast<uint16_t>(book_count));
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_about)
        {
            draw_about_state(&framebuffer);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_connectivity)
        {
            draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_ota)
        {
            draw_ota_state(&framebuffer, screen_state.ota_focus);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_book_manager)
        {
            duplicate_book_count = services::book_manager::duplicate_count(library_catalog);
            draw_book_manager_state(&framebuffer, &screen_state, book_count, sd_card.mounted, duplicate_book_count);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_file_browser)
        {
            services::file_browser::open(sd_config.mount_path, sd_config.mount_path, &file_listing);
            screen_state.file_browser_focus = 0U;
            ui::draw_file_browser(&framebuffer, &file_listing, screen_state.file_browser_focus);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_file_browser_back)
        {
            if (services::file_browser::at_root(&file_listing))
            {
                screen_state.screen = ui::screen_book_manager;
                draw_book_manager_state(&framebuffer, &screen_state, book_count, sd_card.mounted, duplicate_book_count);
            }
            else
            {
                services::file_browser::parent(&file_listing);
                screen_state.file_browser_focus = 0U;
                ui::draw_file_browser(&framebuffer, &file_listing, screen_state.file_browser_focus);
            }
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_file_browser_open)
        {
            if (screen_state.file_browser_focus < file_listing.count)
            {
                const auto& selected = file_listing.entries[screen_state.file_browser_focus];
                if (selected.directory)
                {
                    services::file_browser::open(file_listing.root, selected.path, &file_listing);
                    screen_state.file_browser_focus = 0U;
                    ui::draw_file_browser(&framebuffer, &file_listing, screen_state.file_browser_focus);
                    transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
                }
                else if (selected.epub)
                {
                    storage::persistence::save_position_for_book(book_path, spine_index, page);
                    if (load_book_file(selected.path, book, document))
                    {
                        snprintf(book_path, sizeof(book_path), "%s", selected.path);
                        spine_index = 0U;
                        uint32_t restored_spine = 0U;
                        uint32_t restored_page = 0U;
                        storage::persistence::load_position_for_book(book_path, &restored_spine,
                                                                     &restored_page);
                        if (restored_spine < book->spine_count)
                            spine_index = static_cast<uint8_t>(restored_spine);
                        if (spine_index != 0U &&
                            !load_spine(book_path, book, spine_index, book, document))
                            spine_index = 0U;
                        total_pages = ui::page_count(document, &reader_settings, framebuffer.width,
                                                     framebuffer.height);
                        page = restored_page < total_pages ? static_cast<uint8_t>(restored_page) : 0U;
                        screen_state.screen = ui::screen_reader;
                        show(&framebuffer, &display, book, document, page, total_pages, &reader_settings);
                    }
                }
            }
            continue;
        }
        if (command == ui::screen_command_show_book_sync)
        {
            draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_ota_action ||
            command == ui::screen_command_book_manager_action ||
            command == ui::screen_command_book_sync_action)
        {
            perform_service_action(&screen_state, sd_config.mount_path, library_catalog, book_paths, &book_count,
                                   book_path, spine_index, page);
            if (screen_state.screen == ui::screen_connectivity)
                draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
            else if (screen_state.screen == ui::screen_ota)
                draw_ota_state(&framebuffer, screen_state.ota_focus);
            else if (screen_state.screen == ui::screen_book_manager)
            {
                duplicate_book_count = services::book_manager::duplicate_count(library_catalog);
                draw_book_manager_state(&framebuffer, &screen_state, book_count, sd_card.mounted, duplicate_book_count);
            }
            else if (screen_state.screen == ui::screen_file_browser)
                ui::draw_file_browser(&framebuffer, &file_listing, screen_state.file_browser_focus);
            else if (screen_state.screen == ui::screen_book_actions)
                ui::draw_book_actions(&framebuffer, screen_state.book_action_focus,
                                      library_detail_index < library_catalog->count
                                          ? library_catalog->entries[library_detail_index].title
                                          : "EPUB");
            else if (screen_state.screen == ui::screen_book_sync)
                draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_open_reader)
        {
            if ((previous_screen == ui::screen_library || previous_screen == ui::screen_library_search_results || previous_screen == ui::screen_library_details || previous_screen == ui::screen_book_actions) && book_count > 0U)
            {
                storage::persistence::save_position_for_book(book_path, spine_index, page);
                const uint16_t selected = (previous_screen == ui::screen_library_details || previous_screen == ui::screen_book_actions)
                                              ? library_detail_index
                                              : selected_catalog_index(previous_screen, screen_state.library_focus,
                                                                       library_search_indices, library_search_count,
                                                                       book_count);
                if (selected >= library_catalog->count || !load_book_file(library_catalog->entries[selected].path, book, document))
                {
                    screen_state.screen = ui::screen_library;
                    draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                                      book_count, library_search_indices, library_search_count,
                                      library_search_query, library_recent_mode);
                    transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
                    continue;
                }
                snprintf(book_path, sizeof(book_path), "%s", library_catalog->entries[selected].path);
                services::library_index::mark_read(sd_config.mount_path, library_catalog, book_path);
                services::library_index::sort(library_catalog, library_catalog->sort_mode);
                refresh_book_paths(library_catalog, book_paths, services::library_index::max_books, &book_count);
                spine_index = 0U;
                uint32_t restored_spine = 0U;
                uint32_t restored_page = 0U;
                storage::persistence::load_position_for_book(book_path, &restored_spine,
                                                             &restored_page);
                if (restored_spine < book->spine_count)
                    spine_index = static_cast<uint8_t>(restored_spine);
                if (spine_index != 0U &&
                    !load_spine(book_path, book, spine_index, book, document))
                    spine_index = 0U;
                total_pages = ui::page_count(document, &reader_settings, framebuffer.width,
                                             framebuffer.height);
                page = restored_page < total_pages ? static_cast<uint8_t>(restored_page) : 0U;

                bookmark_count = 0U;
                storage::persistence::bookmark_t persisted[storage::persistence::max_bookmarks_per_book] = {};
                uint8_t persisted_count = 0U;
                if (storage::persistence::load_bookmarks_for_book(
                        book_path, persisted, storage::persistence::max_bookmarks_per_book,
                        &persisted_count) == ESP_OK)
                {
                    bookmark_count = persisted_count;
                    for (uint8_t index = 0; index < bookmark_count; ++index)
                        bookmarks[index] = {true, static_cast<uint8_t>(persisted[index].spine),
                                            static_cast<uint8_t>(persisted[index].page)};
                }
            }
            screen_state.screen = ui::screen_reader;
            show(&framebuffer, &display, book, document, page, total_pages, &reader_settings);
            continue;
        }
        if (command == ui::screen_command_redraw)
        {
            if (screen_state.screen == ui::screen_home)
                ui::draw_home(&framebuffer, sd_card.mounted, book_path[0] != '\0' ? book->title : nullptr, screen_state.home_focus);
            else if (screen_state.screen == ui::screen_library ||
                     screen_state.screen == ui::screen_library_search_results)
                draw_library_view(&framebuffer, sd_card.mounted, library_catalog, &screen_state,
                                  book_count, library_search_indices, library_search_count,
                                  library_search_query, library_recent_mode);
            else if (screen_state.screen == ui::screen_settings)
                ui::draw_settings(&framebuffer, screen_state.settings_focus, &settings_values);
            else if (screen_state.screen == ui::screen_storage)
                draw_storage_state(&framebuffer, sd_config.mount_path, sd_card.mounted, static_cast<uint16_t>(book_count));
            else if (screen_state.screen == ui::screen_about)
                draw_about_state(&framebuffer);
            else if (screen_state.screen == ui::screen_dialog)
                ui::draw_dialog(&framebuffer, &screen_state.dialog);
            else if (screen_state.screen == ui::screen_connectivity)
                draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
            else if (screen_state.screen == ui::screen_wifi_networks)
                draw_wifi_networks_state(&framebuffer, screen_state.wifi_network_focus);
            else if (screen_state.screen == ui::screen_keyboard)
                ui::draw_keyboard(&framebuffer, &screen_state.keyboard);
            else if (screen_state.screen == ui::screen_ota)
                draw_ota_state(&framebuffer, screen_state.ota_focus);
            else if (screen_state.screen == ui::screen_book_manager)
            {
                duplicate_book_count = services::book_manager::duplicate_count(library_catalog);
                draw_book_manager_state(&framebuffer, &screen_state, book_count, sd_card.mounted, duplicate_book_count);
            }
            else if (screen_state.screen == ui::screen_file_browser)
                ui::draw_file_browser(&framebuffer, &file_listing, screen_state.file_browser_focus);
            else if (screen_state.screen == ui::screen_book_actions)
                ui::draw_book_actions(&framebuffer, screen_state.book_action_focus,
                                      library_detail_index < library_catalog->count
                                          ? library_catalog->entries[library_detail_index].title
                                          : "EPUB");
            else if (screen_state.screen == ui::screen_book_sync)
                draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            else if (screen_state.screen == ui::screen_quick_settings)
                ui::draw_quick_settings(&framebuffer, screen_state.quick_focus, &quick_values);
            else if (screen_state.screen == ui::screen_contents)
                ui::draw_contents(&framebuffer, book, screen_state.contents_focus);
            else if (screen_state.screen == ui::screen_bookmarks)
                ui::draw_bookmarks(&framebuffer, bookmarks, bookmark_count, screen_state.bookmarks_focus);
            else if (screen_state.screen == ui::screen_book_info)
                ui::draw_book_info(&framebuffer, book, spine_index, book->spine_count);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_show_contents)
        {
            ui::draw_contents(&framebuffer, book, screen_state.contents_focus);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_bookmarks)
        {
            ui::draw_bookmarks(&framebuffer, bookmarks, bookmark_count, screen_state.bookmarks_focus);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_book_info)
        {
            ui::draw_book_info(&framebuffer, book, spine_index, book->spine_count);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_add_bookmark)
        {
            bool exists = false;
            for (uint8_t i = 0; i < bookmark_count; ++i)
                if (bookmarks[i].spine == spine_index && bookmarks[i].page == page) exists = true;
            if (!exists && bookmark_count < storage::persistence::max_bookmarks_per_book)
            {
                bookmarks[bookmark_count++] = {true, spine_index, page};
                storage::persistence::bookmark_t persistent[storage::persistence::max_bookmarks_per_book] = {};
                for (uint8_t index = 0; index < bookmark_count; ++index)
                {
                    persistent[index].spine = bookmarks[index].spine;
                    persistent[index].page = bookmarks[index].page;
                }
                storage::persistence::save_bookmarks_for_book(book_path, persistent, bookmark_count);
            }
            ui::draw_quick_settings(&framebuffer, screen_state.quick_focus, &quick_values);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_open_contents_item)
        {
            uint8_t target = screen_state.contents_focus;
            if (book->toc_count && target < book->toc_count) target = book->toc[target].spine_index;
            if (target < book->spine_count && load_spine(book_path, book, target, book, document))
            {
                spine_index = target; page = 0;
                total_pages = ui::page_count(document, &reader_settings, framebuffer.width, framebuffer.height);
                screen_state.screen = ui::screen_reader;
                show(&framebuffer, &display, book, document, page, total_pages, &reader_settings);
            }
            continue;
        }
        if (command == ui::screen_command_open_bookmark)
        {
            if (bookmark_count && screen_state.bookmarks_focus < bookmark_count)
            {
                const auto mark = bookmarks[screen_state.bookmarks_focus];
                if (mark.spine < book->spine_count && load_spine(book_path, book, mark.spine, book, document))
                {
                    spine_index = mark.spine;
                    total_pages = ui::page_count(document, &reader_settings, framebuffer.width, framebuffer.height);
                    page = mark.page < total_pages ? mark.page : 0;
                    screen_state.screen = ui::screen_reader;
                    show(&framebuffer, &display, book, document, page, total_pages, &reader_settings);
                }
            }
            continue;
        }
        if (command == ui::screen_command_open_quick_settings)
        {
            ui::draw_quick_settings(&framebuffer, screen_state.quick_focus, &quick_values);
            transfer_dirty(&framebuffer, &display);
            input::flush(events);
            continue;
        }
        if (command == ui::screen_command_close_quick_settings)
        {
            show(&framebuffer, &display, book, document, page, total_pages, &reader_settings);
            input::flush(events);
            continue;
        }
        if (command == ui::screen_command_show_display_settings)
        {
            ui::draw_display_settings(&framebuffer, screen_state.display_focus, &quick_values);
            transfer_dirty(&framebuffer, &display);
            input::flush(events);
            continue;
        }
        if (command == ui::screen_command_show_reading_settings)
        {
            ui::draw_reading_settings(&framebuffer, screen_state.reading_focus, &quick_values);
            transfer_dirty(&framebuffer, &display);
            input::flush(events);
            continue;
        }
        if (command == ui::screen_command_edit_display_setting ||
            command == ui::screen_command_edit_reading_setting)
        {
            const uint8_t previous_orientation = settings.orientation;
            if (command == ui::screen_command_edit_display_setting)
                apply_display_setting(&settings, screen_state.display_focus,
                                      screen_state.has_pending_value, screen_state.pending_value);
            else
                apply_reading_setting(&settings, screen_state.reading_focus,
                                      screen_state.has_pending_value, screen_state.pending_value);
            if (settings.orientation != previous_orientation)
            {
                drivers::it8951e::set_rotation(&display, settings.orientation);
                gfx::destroy(&framebuffer);
                error = gfx::create(&framebuffer, drivers::it8951e::logical_width(&display),
                                    drivers::it8951e::logical_height(&display));
                if (error != ESP_OK)
                {
                    ESP_LOGE(tag, "Unable to resize framebuffer for orientation: %s",
                             esp_err_to_name(error));
                    return;
                }
            }
            storage::persistence::save_settings(&settings);
            reader_settings.text_scale = settings.text_scale;
            reader_settings.line_spacing = settings.line_spacing;
            reader_settings.refresh_mode = settings.refresh_mode;
            reader_settings.margin_mode = settings.margin_mode;
            reader_settings.paragraph_spacing = settings.paragraph_spacing;
            reader_settings.text_alignment = settings.text_alignment;
            quick_values.text_scale = settings.text_scale;
            quick_values.line_spacing = settings.line_spacing;
            quick_values.margin_mode = settings.margin_mode;
            quick_values.paragraph_spacing = settings.paragraph_spacing;
            quick_values.text_alignment = settings.text_alignment;
            quick_values.reverse_page_turn = settings.reverse_page_turn;
            quick_values.refresh_mode = settings.refresh_mode;
            quick_values.orientation = settings.orientation;
            quick_values.invert_colors = settings.invert_colors;
            quick_values.show_clock = settings.show_clock;
            quick_values.sleep_timeout_minutes = settings.sleep_timeout_minutes;
            drivers::it8951e::set_inverted(&display, settings.invert_colors != 0U);
            ui::chrome::set_clock_visible(settings.show_clock != 0U);
            total_pages = ui::page_count(document, &reader_settings, framebuffer.width,
                                         framebuffer.height);
            if (page >= total_pages)
                page = static_cast<uint8_t>(total_pages - 1);
            if (screen_state.screen == ui::screen_display_settings)
                ui::draw_display_settings(&framebuffer, screen_state.display_focus, &quick_values);
            else if (screen_state.screen == ui::screen_reading_settings)
                ui::draw_reading_settings(&framebuffer, screen_state.reading_focus, &quick_values);
            else
                show(&framebuffer, &display, book, document, page, total_pages, &reader_settings);
            transfer_dirty(&framebuffer, &display);
            input::flush(events);
            continue;
        }
        if (command == ui::screen_command_page_forward ||
            command == ui::screen_command_page_backward ||
            command == ui::screen_command_chapter_forward ||
            command == ui::screen_command_chapter_backward)
        {
            if (command == ui::screen_command_chapter_forward)
            {
                if (!load_spine(book_path, book, static_cast<uint8_t>(spine_index + 1), book,
                                document))
                    continue;
                ++spine_index;
                page = 0;
                total_pages = ui::page_count(document, &reader_settings, framebuffer.width,
                                             framebuffer.height);
            }
            else if (command == ui::screen_command_chapter_backward)
            {
                if (!load_spine(book_path, book, static_cast<uint8_t>(spine_index - 1), book,
                                document))
                    continue;
                --spine_index;
                total_pages = ui::page_count(document, &reader_settings, framebuffer.width,
                                             framebuffer.height);
                page = static_cast<uint8_t>(total_pages - 1);
            }
            else
            {
                page = static_cast<uint8_t>(static_cast<int16_t>(page) +
                                            (command == ui::screen_command_page_forward
                                                 ? static_cast<int16_t>(1)
                                                 : static_cast<int16_t>(-1)));
            }
            // Start rendering/refreshing before committing the reading position to NVS so
            // flash persistence is not part of the touch-to-display latency.
            show(&framebuffer, &display, book, document, page, total_pages, &reader_settings);
            storage::persistence::save_position_for_book(book_path, spine_index, page);
            input::flush(events);
        }
    }
}

} // namespace app
} // namespace xreader

extern "C" void app_main(void)
{
    xreader::app::run();
}
