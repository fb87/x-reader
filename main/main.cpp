#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
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
#include "services/book_manager.hpp"
#include "services/book_sync.hpp"
#include "services/connectivity.hpp"
#include "services/ota.hpp"
#include "storage/book_loader.hpp"
#include "storage/persistence.hpp"
#include "storage/sdcard/sdcard.hpp"
#include "ui/book_info.hpp"
#include "ui/book_manager.hpp"
#include "ui/book_sync.hpp"
#include "ui/bookmarks.hpp"
#include "ui/chrome.hpp"
#include "ui/connectivity.hpp"
#include "ui/contents.hpp"
#include "ui/hardware_test.hpp"
#include "ui/home.hpp"
#include "ui/keyboard.hpp"
#include "ui/library.hpp"
#include "ui/navigation.hpp"
#include "ui/ota.hpp"
#include "ui/quick_settings.hpp"
#include "ui/reader.hpp"
#include "ui/screen.hpp"
#include "ui/settings.hpp"
#include "ui/wifi_networks.hpp"

namespace xreader
{
namespace app
{

static const char* const tag = "xreader";

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
                      const epub::book_t* book, const epub::document_t* document, uint8_t page,
                      uint8_t page_count, const ui::reader_settings_t* settings)
{
    [[maybe_unused]] const int64_t draw_start = XR_TIME_US();
    XR_LOGI("draw start page=%u/%u", static_cast<unsigned>(page + 1),
            static_cast<unsigned>(page_count));
    ui::draw_reader(framebuffer, book, document, page, page_count, settings);
    XR_LOGI("draw done elapsed_us=%lld", static_cast<long long>(XR_TIME_US() - draw_start));
    const drivers::it8951e::refresh_mode_t refresh_mode =
        settings != nullptr && settings->refresh_mode != 0 ? drivers::it8951e::refresh_du
                                                           : drivers::it8951e::refresh_gc16;
    return transfer_dirty(framebuffer, display, refresh_mode);
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

static void cycle_setting(storage::persistence::settings_t* settings, ui::quick_setting_t setting)
{
    if (settings == nullptr)
        return;
    if (setting == ui::quick_setting_text_size)
        settings->text_scale = settings->text_scale == 1 ? 2 : 1;
    else if (setting == ui::quick_setting_line_spacing)
        settings->line_spacing = settings->line_spacing == 0 ? 1 : 0;
    else if (setting == ui::quick_setting_refresh_mode)
        settings->refresh_mode = settings->refresh_mode == 0 ? 1 : 0;
    else if (setting == ui::quick_setting_orientation)
        settings->orientation = settings->orientation == 0 ? 1 : 0;
    else if (setting == ui::quick_setting_sleep_timeout)
        settings->sleep_timeout_minutes = settings->sleep_timeout_minutes == 30
                                              ? 60
                                              : (settings->sleep_timeout_minutes == 60 ? 120 : 30);
}

static ui::quick_setting_t quick_setting_for(ui::settings_item_t item)
{
    switch (item)
    {
    case ui::settings_text_size:
        return ui::quick_setting_text_size;
    case ui::settings_line_spacing:
        return ui::quick_setting_line_spacing;
    case ui::settings_refresh_mode:
        return ui::quick_setting_refresh_mode;
    case ui::settings_orientation:
        return ui::quick_setting_orientation;
    case ui::settings_sleep_timeout:
        return ui::quick_setting_sleep_timeout;
    default:
        return ui::quick_setting_text_size;
    }
}

static void draw_connectivity_state(gfx::framebuffer_t* framebuffer, ui::connectivity_item_t focus)
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
    const uint8_t count =
        services::connectivity::scan_results(results, services::connectivity::max_scan_results);
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

static bool handle_connectivity_ui_command(ui::screen_command_t command,
                                           ui::screen_state_t* screen_state,
                                           gfx::framebuffer_t* framebuffer,
                                           drivers::it8951e::device_t* display)
{
    if (screen_state == nullptr || framebuffer == nullptr || display == nullptr)
        return false;

    if (command == ui::screen_command_connectivity_action)
    {
        if (screen_state->connectivity_focus == ui::connectivity_wifi)
        {
            const services::connectivity::state_t network = services::connectivity::snapshot();
            services::connectivity::set_enabled(!network.enabled);
        }
        else if (screen_state->connectivity_focus == ui::connectivity_forget_network)
            services::connectivity::forget_network();
        else if (screen_state->connectivity_focus == ui::connectivity_sync_server)
        {
            const services::book_sync::state_t sync = services::book_sync::snapshot();
            ui::keyboard_begin(&screen_state->keyboard, ui::keyboard_purpose_sync_server,
                               "SYNC SERVER", sync.server, false, ui::keyboard_qwerty);
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
        services::connectivity::scan_result_t results[services::connectivity::max_scan_results] =
            {};
        const uint8_t count =
            services::connectivity::scan_results(results, services::connectivity::max_scan_results);
        const uint8_t selected = screen_state->wifi_network_focus;
        if (selected < count && selected < 6U)
        {
            snprintf(screen_state->pending_wifi_ssid, sizeof(screen_state->pending_wifi_ssid), "%s",
                     results[selected].ssid);
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
            screen_state->screen = ui::screen_connectivity;
            draw_connectivity_state(framebuffer, screen_state->connectivity_focus);
        }
        transfer_dirty(framebuffer, display, drivers::it8951e::refresh_du);
        return true;
    }

    if (command == ui::screen_command_keyboard_cancel)
    {
        if (screen_state->screen == ui::screen_wifi_networks)
            draw_wifi_networks_state(framebuffer, screen_state->wifi_network_focus);
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
                 ota_state.update_available);
}

static void draw_book_sync_state(gfx::framebuffer_t* framebuffer, ui::book_sync_item_t focus)
{
    const services::connectivity::state_t network = services::connectivity::snapshot();
    const services::book_sync::state_t sync = services::book_sync::snapshot();
    ui::draw_book_sync(framebuffer, focus, network.connected, sync.sync_books, sync.sync_progress,
                       sync.server[0] != '\0',
                       sync.last_sync[0] == '\0' ? "NEVER" : sync.last_sync);
}

static void perform_service_action(const ui::screen_state_t* screen_state, const char* mount_path,
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
        if (book_paths != nullptr && book_count != nullptr)
            *book_count = ui::find_books(mount_path, book_paths, 8);
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

static void run()
{
    esp_err_t error = storage::persistence::init();
    if (error != ESP_OK)
        ESP_LOGW(tag, "NVS initialization failed: %s", esp_err_to_name(error));
    storage::persistence::settings_t settings = {};
    storage::persistence::default_settings(&settings);
    storage::persistence::load_settings(&settings);
    // Keep the current hardware rollout in portrait while the orientation UI settles.
    settings.orientation = 1;
    services::connectivity::init();
    if (services::connectivity::snapshot().ssid[0] != '\0')
        services::connectivity::set_enabled(true);
    services::ota::init();
    services::book_sync::init();
    ui::quick_settings_values_t settings_values = {
        .text_scale = settings.text_scale,
        .line_spacing = settings.line_spacing,
        .refresh_mode = settings.refresh_mode,
        .orientation = settings.orientation,
        .sleep_timeout_minutes = settings.sleep_timeout_minutes,
    };
    error = board::m5paper::power_on();
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "M5Paper power initialization failed: %s", esp_err_to_name(error));
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
    char book_paths[8][ui::book_path_length] = {};
    size_t book_count = sd_card.mounted ? ui::find_books(sd_config.mount_path, book_paths, 8) : 0;
    char book_path[ui::book_path_length] = {};
    esp_err_t load_result = ESP_ERR_NOT_FOUND;
    bool have_book = false;
    for (size_t book_index = 0; book_index < book_count && !have_book; ++book_index)
    {
        if (storage::book_loader::start(book_paths[book_index]) != ESP_OK)
            continue;
        for (uint32_t wait_ms = 0; wait_ms < 15000 && !have_book; wait_ms += 50)
        {
            if (storage::book_loader::poll(book, document, &load_result))
                have_book = load_result == ESP_OK;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (have_book)
            strcpy(book_path, book_paths[book_index]);
        else
            ESP_LOGW(tag, "book unavailable: %s: %s", book_paths[book_index],
                     esp_err_to_name(load_result));
    }
    if (!have_book)
    {
        ESP_LOGW(tag, "no valid EPUB book available: %s", esp_err_to_name(load_result));
        heap_caps_free(book);
        heap_caps_free(document);
        gfx::destroy(&framebuffer);
        board::m5paper::power_off();
        return;
    }
    ui::draw_home(&framebuffer, sd_card.mounted, book->title, screen_state.home_focus);
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
    while (!open_reader)
    {
        input::event_t event = {};
        const bool service_screen = screen_state.screen == ui::screen_connectivity ||
                                    screen_state.screen == ui::screen_wifi_networks ||
                                    screen_state.screen == ui::screen_ota ||
                                    screen_state.screen == ui::screen_book_sync;
        const TickType_t wait_ticks =
            pdMS_TO_TICKS(service_screen ? 500U : settings.sleep_timeout_minutes * 60U * 1000U);
        if (xQueueReceive(events, &event, wait_ticks) != pdTRUE)
        {
            if (service_screen)
            {
                services::connectivity::poll();
                const services::connectivity::state_t net_state =
                    services::connectivity::snapshot();
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
            board::m5paper::enter_deep_sleep(1000ULL * 60ULL * 60ULL);
            continue;
        }
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
        const ui::screen_context_t context = {
            .viewport = {framebuffer.width, framebuffer.height},
            .library_count = static_cast<uint8_t>(book_count),
            .page = 0,
            .page_count = 1,
            .spine_index = 0,
            .spine_count = book->spine_count,
            .toc_count = book->toc_count,
            .bookmark_count = 0,
            .wifi_network_count = services::connectivity::snapshot().scan_count,
        };
        const ui::screen_command_t command = ui::dispatch(&screen_state, &action_event, &context);
        XR_LOGI("screen command=%u screen=%u", static_cast<unsigned>(command),
                static_cast<unsigned>(screen_state.screen));
        if (handle_connectivity_ui_command(command, &screen_state, &framebuffer, &display))
            continue;
        if (command == ui::screen_command_open_reader)
            open_reader = true;
        else if (command == ui::screen_command_sleep)
            board::m5paper::enter_deep_sleep(1000ULL * 60ULL * 60ULL);
        else if (command == ui::screen_command_redraw)
        {
            if (screen_state.screen == ui::screen_home)
                ui::draw_home(&framebuffer, sd_card.mounted, book->title, screen_state.home_focus);
            else if (screen_state.screen == ui::screen_library)
                ui::draw_library_list(&framebuffer, sd_card.mounted, book_paths, book_count,
                                      screen_state.library_focus);
            else if (screen_state.screen == ui::screen_settings)
                ui::draw_settings(&framebuffer, screen_state.settings_focus, &settings_values);
            else if (screen_state.screen == ui::screen_connectivity)
                draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
            else if (screen_state.screen == ui::screen_wifi_networks)
                draw_wifi_networks_state(&framebuffer, screen_state.wifi_network_focus);
            else if (screen_state.screen == ui::screen_keyboard)
                ui::draw_keyboard(&framebuffer, &screen_state.keyboard);
            else if (screen_state.screen == ui::screen_ota)
                draw_ota_state(&framebuffer, screen_state.ota_focus);
            else if (screen_state.screen == ui::screen_book_manager)
                ui::draw_book_manager(&framebuffer, screen_state.book_manager_focus,
                                      static_cast<uint16_t>(book_count), sd_card.mounted);
            else if (screen_state.screen == ui::screen_book_sync)
                draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
        }
        else if (command == ui::screen_command_show_library)
        {
            ui::draw_library_list(&framebuffer, sd_card.mounted, book_paths, book_count,
                                  screen_state.library_focus);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_settings)
        {
            ui::draw_settings(&framebuffer, screen_state.settings_focus, &settings_values);
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
            ui::draw_book_manager(&framebuffer, screen_state.book_manager_focus,
                                  static_cast<uint16_t>(book_count), sd_card.mounted);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_book_sync)
        {
            draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_show_home)
        {
            ui::draw_home(&framebuffer, sd_card.mounted, book->title, screen_state.home_focus);
            transfer_dirty(&framebuffer, &display);
        }
        else if (command == ui::screen_command_ota_action ||
                 command == ui::screen_command_book_manager_action ||
                 command == ui::screen_command_book_sync_action)
        {
            perform_service_action(&screen_state, sd_config.mount_path, book_paths, &book_count,
                                   book_path, 0, 0);
            if (screen_state.screen == ui::screen_connectivity)
                draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
            else if (screen_state.screen == ui::screen_ota)
                draw_ota_state(&framebuffer, screen_state.ota_focus);
            else if (screen_state.screen == ui::screen_book_manager)
                ui::draw_book_manager(&framebuffer, screen_state.book_manager_focus,
                                      static_cast<uint16_t>(book_count), sd_card.mounted);
            else if (screen_state.screen == ui::screen_book_sync)
                draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
        }
        else if (command == ui::screen_command_edit_setting)
        {
            if (screen_state.settings_focus != ui::settings_back)
            {
                const uint8_t previous_orientation = settings.orientation;
                cycle_setting(&settings, quick_setting_for(screen_state.settings_focus));
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
                settings_values.text_scale = settings.text_scale;
                settings_values.line_spacing = settings.line_spacing;
                settings_values.refresh_mode = settings.refresh_mode;
                settings_values.orientation = settings.orientation;
                settings_values.sleep_timeout_minutes = settings.sleep_timeout_minutes;
            }
            if (screen_state.screen == ui::screen_settings)
            {
                ui::draw_settings(&framebuffer, screen_state.settings_focus, &settings_values);
                transfer_dirty(&framebuffer, &display);
            }
        }
    }
    ui::reader_settings_t reader_settings = {
        .text_scale = settings.text_scale,
        .line_spacing = settings.line_spacing,
        .refresh_mode = settings.refresh_mode,
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
    total_pages = ui::page_count(document, &reader_settings, framebuffer.width, framebuffer.height);
    uint8_t page = saved_page < total_pages ? static_cast<uint8_t>(saved_page) : 0;
    screen_state.screen = ui::screen_reader;
    show(&framebuffer, &display, book, document, page, total_pages, &reader_settings);
    input::flush(events);
    ui::bookmark_view_t bookmarks[storage::persistence::max_bookmarks_per_book] = {};
    uint8_t bookmark_count = 0;
    storage::persistence::bookmark_t saved_bookmarks[storage::persistence::max_bookmarks_per_book] =
        {};
    uint8_t saved_bookmark_count = 0;
    if (storage::persistence::load_bookmarks_for_book(book_path, saved_bookmarks,
                                                      storage::persistence::max_bookmarks_per_book,
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
        .refresh_mode = settings.refresh_mode,
        .orientation = settings.orientation,
        .sleep_timeout_minutes = settings.sleep_timeout_minutes,
    };
    while (events != nullptr)
    {
        input::event_t event = {};
        const bool service_screen = screen_state.screen == ui::screen_connectivity ||
                                    screen_state.screen == ui::screen_wifi_networks ||
                                    screen_state.screen == ui::screen_ota ||
                                    screen_state.screen == ui::screen_book_sync;
        const TickType_t wait_ticks =
            pdMS_TO_TICKS(service_screen ? 500U : settings.sleep_timeout_minutes * 60U * 1000U);
        if (xQueueReceive(events, &event, wait_ticks) != pdTRUE)
        {
            if (service_screen)
            {
                services::connectivity::poll();
                const services::connectivity::state_t net_state =
                    services::connectivity::snapshot();
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
            board::m5paper::enter_deep_sleep(1000ULL * 60ULL * 60ULL);
            continue;
        }
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
            .library_count = static_cast<uint8_t>(book_count),
            .page = page,
            .page_count = total_pages,
            .spine_index = spine_index,
            .spine_count = book->spine_count,
            .toc_count = book->toc_count,
            .bookmark_count = bookmark_count,
            .wifi_network_count = services::connectivity::snapshot().scan_count,
        };
        const ui::screen_command_t command = ui::dispatch(&screen_state, &action_event, &context);
        XR_LOGI("reader command=%u screen=%u", static_cast<unsigned>(command),
                static_cast<unsigned>(screen_state.screen));
        if (handle_connectivity_ui_command(command, &screen_state, &framebuffer, &display))
            continue;
        if (command == ui::screen_command_sleep)
        {
            board::m5paper::enter_deep_sleep(1000ULL * 60ULL * 60ULL);
            continue;
        }
        if (command == ui::screen_command_show_home)
        {
            ui::draw_home(&framebuffer, sd_card.mounted, book->title, screen_state.home_focus);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_library)
        {
            ui::draw_library_list(&framebuffer, sd_card.mounted, book_paths, book_count,
                                  screen_state.library_focus);
            transfer_dirty(&framebuffer, &display);
            continue;
        }
        if (command == ui::screen_command_show_settings)
        {
            ui::draw_settings(&framebuffer, screen_state.settings_focus, &settings_values);
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
            ui::draw_book_manager(&framebuffer, screen_state.book_manager_focus,
                                  static_cast<uint16_t>(book_count), sd_card.mounted);
            transfer_dirty(&framebuffer, &display);
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
            perform_service_action(&screen_state, sd_config.mount_path, book_paths, &book_count,
                                   book_path, spine_index, page);
            if (screen_state.screen == ui::screen_connectivity)
                draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
            else if (screen_state.screen == ui::screen_ota)
                draw_ota_state(&framebuffer, screen_state.ota_focus);
            else if (screen_state.screen == ui::screen_book_manager)
                ui::draw_book_manager(&framebuffer, screen_state.book_manager_focus,
                                      static_cast<uint16_t>(book_count), sd_card.mounted);
            else if (screen_state.screen == ui::screen_book_sync)
                draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_open_reader)
        {
            screen_state.screen = ui::screen_reader;
            show(&framebuffer, &display, book, document, page, total_pages, &reader_settings);
            continue;
        }
        if (command == ui::screen_command_redraw)
        {
            if (screen_state.screen == ui::screen_home)
                ui::draw_home(&framebuffer, sd_card.mounted, book->title, screen_state.home_focus);
            else if (screen_state.screen == ui::screen_library)
                ui::draw_library_list(&framebuffer, sd_card.mounted, book_paths, book_count,
                                      screen_state.library_focus);
            else if (screen_state.screen == ui::screen_settings)
                ui::draw_settings(&framebuffer, screen_state.settings_focus, &settings_values);
            else if (screen_state.screen == ui::screen_connectivity)
                draw_connectivity_state(&framebuffer, screen_state.connectivity_focus);
            else if (screen_state.screen == ui::screen_wifi_networks)
                draw_wifi_networks_state(&framebuffer, screen_state.wifi_network_focus);
            else if (screen_state.screen == ui::screen_keyboard)
                ui::draw_keyboard(&framebuffer, &screen_state.keyboard);
            else if (screen_state.screen == ui::screen_ota)
                draw_ota_state(&framebuffer, screen_state.ota_focus);
            else if (screen_state.screen == ui::screen_book_manager)
                ui::draw_book_manager(&framebuffer, screen_state.book_manager_focus,
                                      static_cast<uint16_t>(book_count), sd_card.mounted);
            else if (screen_state.screen == ui::screen_book_sync)
                draw_book_sync_state(&framebuffer, screen_state.book_sync_focus);
            else if (screen_state.screen == ui::screen_quick_settings)
                ui::draw_quick_settings(&framebuffer, screen_state.quick_focus, &quick_values);
            else if (screen_state.screen == ui::screen_contents)
                ui::draw_contents(&framebuffer, book, screen_state.contents_focus);
            else if (screen_state.screen == ui::screen_bookmarks)
                ui::draw_bookmarks(&framebuffer, bookmarks, bookmark_count,
                                   screen_state.bookmarks_focus);
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
            ui::draw_bookmarks(&framebuffer, bookmarks, bookmark_count,
                               screen_state.bookmarks_focus);
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
                if (bookmarks[i].spine == spine_index && bookmarks[i].page == page)
                    exists = true;
            if (!exists && bookmark_count < storage::persistence::max_bookmarks_per_book)
            {
                bookmarks[bookmark_count++] = {true, spine_index, page};
                storage::persistence::bookmark_t
                    persistent[storage::persistence::max_bookmarks_per_book] = {};
                for (uint8_t index = 0; index < bookmark_count; ++index)
                {
                    persistent[index].spine = bookmarks[index].spine;
                    persistent[index].page = bookmarks[index].page;
                }
                storage::persistence::save_bookmarks_for_book(book_path, persistent,
                                                              bookmark_count);
            }
            ui::draw_quick_settings(&framebuffer, screen_state.quick_focus, &quick_values);
            transfer_dirty(&framebuffer, &display, drivers::it8951e::refresh_du);
            continue;
        }
        if (command == ui::screen_command_open_contents_item)
        {
            uint8_t target = screen_state.contents_focus;
            if (book->toc_count && target < book->toc_count)
                target = book->toc[target].spine_index;
            if (target < book->spine_count && load_spine(book_path, book, target, book, document))
            {
                spine_index = target;
                page = 0;
                total_pages = ui::page_count(document, &reader_settings, framebuffer.width,
                                             framebuffer.height);
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
                if (mark.spine < book->spine_count &&
                    load_spine(book_path, book, mark.spine, book, document))
                {
                    spine_index = mark.spine;
                    total_pages = ui::page_count(document, &reader_settings, framebuffer.width,
                                                 framebuffer.height);
                    page = mark.page < total_pages ? mark.page : 0;
                    screen_state.screen = ui::screen_reader;
                    show(&framebuffer, &display, book, document, page, total_pages,
                         &reader_settings);
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
        if (command == ui::screen_command_edit_setting)
        {
            const uint8_t previous_orientation = settings.orientation;
            cycle_setting(&settings, screen_state.quick_focus);
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
            quick_values.text_scale = settings.text_scale;
            quick_values.line_spacing = settings.line_spacing;
            quick_values.refresh_mode = settings.refresh_mode;
            quick_values.orientation = settings.orientation;
            quick_values.sleep_timeout_minutes = settings.sleep_timeout_minutes;
            total_pages =
                ui::page_count(document, &reader_settings, framebuffer.width, framebuffer.height);
            if (page >= total_pages)
                page = static_cast<uint8_t>(total_pages - 1);
            if (screen_state.screen == ui::screen_quick_settings)
                ui::draw_quick_settings(&framebuffer, screen_state.quick_focus, &quick_values);
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
