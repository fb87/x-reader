#include "debug_console.hpp"

#if XREADER_DEBUG_CONSOLE

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(XREADER_BOARD_XTEINK)
#include "driver/usb_serial_jtag.h"
#else
#include "driver/uart.h"
#endif
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"

#include "input/input.hpp"
#include "ui/screen.hpp"

namespace xreader
{
namespace services
{
namespace debug_console
{

namespace
{
static const char* const tag = "xrcon";

#if !defined(XREADER_BOARD_XTEINK)
static constexpr uart_port_t console_port = UART_NUM_0;
#endif
static constexpr size_t line_capacity = 512;
static constexpr size_t receive_chunk = 384;

// XTeink X4 has no UART0 wired out to the host -- its only USB connection is
// the ESP32-C3's native USB-Serial-JTAG peripheral, which is what the log
// output already goes over (see CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG).
// M5Paper's USB-UART bridge chip is wired to UART0, so it keeps using the
// uart.h driver directly.
static int console_read_bytes(uint8_t* buffer, size_t length, TickType_t ticks_to_wait)
{
#if defined(XREADER_BOARD_XTEINK)
    return usb_serial_jtag_read_bytes(buffer, length, ticks_to_wait);
#else
    return uart_read_bytes(console_port, buffer, length, ticks_to_wait);
#endif
}

static QueueHandle_t active_events = nullptr;
static const ui::screen_state_t* active_state = nullptr;
static uint16_t viewport_width = 0;
static uint16_t viewport_height = 0;

// State for the `put` file-upload command.  A transfer stays open across lines
// until the terminating "." line, so it has to outlive a single command.
static FILE* upload_file = nullptr;

static void reply(const char* format, ...) __attribute__((format(printf, 1, 2)));

static void reply(const char* format, ...)
{
    char message[256];
    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    if (written < 0)
        return;
    ESP_LOGI(tag, "%s", message);
}

static bool send_event(input::event_t event)
{
    QueueHandle_t events = active_events;
    if (events == nullptr)
        return false;
    return xQueueSend(events, &event, pdMS_TO_TICKS(200)) == pdTRUE;
}

static bool parse_u16(const char* text, uint16_t* value)
{
    if (text == nullptr || text[0] == '\0' || value == nullptr)
        return false;
    char* end = nullptr;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 || parsed > 65535)
        return false;
    *value = static_cast<uint16_t>(parsed);
    return true;
}

struct key_name_t
{
    const char* name;
    input::key_t key;
};

static const key_name_t key_names[] = {
    {"up", input::key_up},       {"down", input::key_down},      {"left", input::key_left},
    {"right", input::key_right}, {"select", input::key_select},  {"back", input::key_back},
    {"menu", input::key_menu},   {"next", input::key_page_next}, {"prev", input::key_page_prev},
    {"home", input::key_home},   {"power", input::key_power},
};

static bool lookup_key(const char* name, input::key_t* key)
{
    if (name == nullptr || key == nullptr)
        return false;
    for (size_t index = 0; index < sizeof(key_names) / sizeof(key_names[0]); ++index)
    {
        if (strcmp(key_names[index].name, name) == 0)
        {
            *key = key_names[index].key;
            return true;
        }
    }
    return false;
}

static void command_ls(const char* path)
{
    DIR* directory = opendir(path);
    if (directory == nullptr)
    {
        reply("ls %s ERR", path);
        return;
    }
    unsigned count = 0;
    const struct dirent* entry = readdir(directory);
    while (entry != nullptr)
    {
        char full[320];
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
        struct stat status = {};
        const long size = stat(full, &status) == 0 ? static_cast<long>(status.st_size) : -1;
        reply("ls %s %s %ld", path, entry->d_name, size);
        ++count;
        entry = readdir(directory);
    }
    closedir(directory);
    reply("ls %s DONE %u", path, count);
}

static void command_put_begin(const char* path)
{
    if (upload_file != nullptr)
    {
        fclose(upload_file);
        upload_file = nullptr;
    }
    upload_file = fopen(path, "wb");
    reply("put %s %s", path, upload_file == nullptr ? "ERR" : "READY");
}

// Returns true when the line was consumed as upload payload.
static bool feed_upload(const char* line)
{
    if (upload_file == nullptr)
        return false;
    if (strcmp(line, ".") == 0)
    {
        fflush(upload_file);
        fclose(upload_file);
        upload_file = nullptr;
        reply("put DONE");
        return true;
    }
    unsigned char decoded[line_capacity];
    size_t decoded_length = 0;
    const int status =
        mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_length,
                              reinterpret_cast<const unsigned char*>(line), strlen(line));
    if (status != 0)
    {
        reply("put ERR base64 %d", status);
        fclose(upload_file);
        upload_file = nullptr;
        return true;
    }
    if (decoded_length != 0 && fwrite(decoded, 1, decoded_length, upload_file) != decoded_length)
    {
        reply("put ERR write");
        fclose(upload_file);
        upload_file = nullptr;
    }
    return true;
}

static void handle_command(char* line)
{
    if (feed_upload(line))
        return;

    char* saved = nullptr;
    const char* verb = strtok_r(line, " \t", &saved);
    if (verb == nullptr)
        return;

    if (strcmp(verb, "ping") == 0)
    {
        reply("pong");
        return;
    }
    if (strcmp(verb, "state") == 0)
    {
        const ui::screen_state_t* state = active_state;
        if (state == nullptr)
        {
            reply("state ERR");
            return;
        }
        reply("state screen=%u return=%u vw=%u vh=%u home=%u library=%u settings=%u quick=%u "
              "contents=%u bookmarks=%u conn=%u ota=%u bmgr=%u bact=%u fbrowse=%u sync=%u wifi=%u "
              "footer=%d",
              static_cast<unsigned>(state->screen), static_cast<unsigned>(state->return_screen),
              static_cast<unsigned>(viewport_width), static_cast<unsigned>(viewport_height),
              static_cast<unsigned>(state->home_focus), static_cast<unsigned>(state->library_focus),
              static_cast<unsigned>(state->settings_focus),
              static_cast<unsigned>(state->quick_focus),
              static_cast<unsigned>(state->contents_focus),
              static_cast<unsigned>(state->bookmarks_focus),
              static_cast<unsigned>(state->connectivity_focus),
              static_cast<unsigned>(state->ota_focus),
              static_cast<unsigned>(state->book_manager_focus),
              static_cast<unsigned>(state->book_action_focus),
              static_cast<unsigned>(state->file_browser_focus),
              static_cast<unsigned>(state->book_sync_focus),
              static_cast<unsigned>(state->wifi_network_focus),
              static_cast<int>(ui::footer_highlight(state)));
        return;
    }
    if (strcmp(verb, "tap") == 0 || strcmp(verb, "down") == 0 || strcmp(verb, "up") == 0 ||
        strcmp(verb, "move") == 0)
    {
        uint16_t x = 0;
        uint16_t y = 0;
        if (!parse_u16(strtok_r(nullptr, " \t", &saved), &x) ||
            !parse_u16(strtok_r(nullptr, " \t", &saved), &y))
        {
            reply("%s ERR args", verb);
            return;
        }
        bool sent = true;
        if (strcmp(verb, "tap") == 0)
        {
            sent = send_event({input::event_touch_down, x, y, input::key_none});
            vTaskDelay(pdMS_TO_TICKS(30));
            sent = send_event({input::event_touch_up, x, y, input::key_none}) && sent;
        }
        else if (strcmp(verb, "down") == 0)
            sent = send_event({input::event_touch_down, x, y, input::key_none});
        else if (strcmp(verb, "up") == 0)
            sent = send_event({input::event_touch_up, x, y, input::key_none});
        else
            sent = send_event({input::event_touch_move, x, y, input::key_none});
        reply("%s %u %u %s", verb, static_cast<unsigned>(x), static_cast<unsigned>(y),
              sent ? "OK" : "ERR");
        return;
    }
    if (strcmp(verb, "key") == 0)
    {
        const char* name = strtok_r(nullptr, " \t", &saved);
        input::key_t key = input::key_none;
        if (!lookup_key(name, &key))
        {
            reply("key ERR name");
            return;
        }
        const bool sent = send_event({input::event_key_up, 0, 0, key});
        reply("key %s %s", name, sent ? "OK" : "ERR");
        return;
    }
    if (strcmp(verb, "rot") == 0)
    {
        const char* direction = strtok_r(nullptr, " \t", &saved);
        if (direction == nullptr)
        {
            reply("rot ERR args");
            return;
        }
        // dcw/dccw inject the already-resolved "double click" gesture directly
        // (same precedent as btnlong below), since real double-click detection
        // lives in poll_rotary()'s timing state, which console-injected events
        // bypass entirely -- two separate "rot cw" commands would never trigger it.
        input::event_type_t type;
        if (strcmp(direction, "cw") == 0)
            type = input::event_rotary_clockwise;
        else if (strcmp(direction, "ccw") == 0)
            type = input::event_rotary_counterclockwise;
        else if (strcmp(direction, "dcw") == 0)
            type = input::event_rotary_double_clockwise;
        else if (strcmp(direction, "dccw") == 0)
            type = input::event_rotary_double_counterclockwise;
        else
        {
            reply("rot ERR args");
            return;
        }
        reply("rot %s %s", direction, send_event({type, 0, 0, input::key_none}) ? "OK" : "ERR");
        return;
    }
    if (strcmp(verb, "btn") == 0)
    {
        reply("btn %s", send_event({input::event_button_up, 0, 0, input::key_none}) ? "OK" : "ERR");
        return;
    }
    if (strcmp(verb, "btnlong") == 0)
    {
        reply("btnlong %s",
              send_event({input::event_button_long_press, 0, 0, input::key_none}) ? "OK" : "ERR");
        return;
    }
    if (strcmp(verb, "ls") == 0)
    {
        const char* path = strtok_r(nullptr, " \t", &saved);
        command_ls(path == nullptr ? "/sdcard" : path);
        return;
    }
    if (strcmp(verb, "mkdir") == 0)
    {
        const char* path = strtok_r(nullptr, " \t", &saved);
        if (path == nullptr)
        {
            reply("mkdir ERR args");
            return;
        }
        reply("mkdir %s %s", path, mkdir(path, 0777) == 0 ? "OK" : "ERR");
        return;
    }
    if (strcmp(verb, "rm") == 0)
    {
        const char* path = strtok_r(nullptr, " \t", &saved);
        if (path == nullptr)
        {
            reply("rm ERR args");
            return;
        }
        reply("rm %s %s", path, remove(path) == 0 ? "OK" : "ERR");
        return;
    }
    if (strcmp(verb, "put") == 0)
    {
        const char* path = strtok_r(nullptr, " \t", &saved);
        if (path == nullptr)
        {
            reply("put ERR args");
            return;
        }
        command_put_begin(path);
        return;
    }
    if (strcmp(verb, "reboot") == 0)
    {
        reply("reboot");
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
    }
    reply("ERR unknown %s", verb);
}

static void console_task(void* argument)
{
    (void)argument;
    static char line[line_capacity];
    size_t length = 0;
    uint8_t chunk[receive_chunk];
    while (true)
    {
        const int received = console_read_bytes(chunk, sizeof(chunk), pdMS_TO_TICKS(100));
        for (int index = 0; index < received; ++index)
        {
            const char character = static_cast<char>(chunk[index]);
            if (character == '\r')
                continue;
            if (character == '\n')
            {
                line[length] = '\0';
                if (length != 0)
                    handle_command(line);
                length = 0;
                continue;
            }
            if (length + 1U < sizeof(line))
                line[length++] = character;
        }
    }
}

} // namespace

void set_event_queue(QueueHandle_t events)
{
    active_events = events;
}

void set_ui_state(const void* screen_state, uint16_t width, uint16_t height)
{
    active_state = static_cast<const ui::screen_state_t*>(screen_state);
    viewport_width = width;
    viewport_height = height;
}

esp_err_t start()
{
#if defined(XREADER_BOARD_XTEINK)
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    config.rx_buffer_size = 2048;
    const esp_err_t error = usb_serial_jtag_driver_install(&config);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
        return error;
#else
    const uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {},
    };
    esp_err_t error = uart_driver_install(console_port, 2048, 0, 0, nullptr, 0);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
        return error;
    error = uart_param_config(console_port, &config);
    if (error != ESP_OK)
        return error;
#endif
    if (xTaskCreate(console_task, "xrcon", 4096, nullptr, 4, nullptr) != pdPASS)
        return ESP_ERR_NO_MEM;
    ESP_LOGI(tag, "debug console ready");
    return ESP_OK;
}

} // namespace debug_console
} // namespace services
} // namespace xreader

#endif // XREADER_DEBUG_CONSOLE
