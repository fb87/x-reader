extern "C" {
#include "app.h"
#include "app_internal.h"
#include "xr/xr.h"
}

#include <cstring>
#include <cinttypes>
#include <climits>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdio>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board/m5paper/m5paper_board.hpp"
#include "board/m5paper/m5paper_pins.hpp"
#include "drivers/gt911/gt911.hpp"
#include "drivers/it8951e/it8951e.hpp"
#include "inflate.hpp"

namespace {

/* The app uses portrait logical coordinates; IT8951 rotates onto 960x540. */
constexpr int width = 540;
constexpr int height = 960;
constexpr int stride = width / 2;
constexpr size_t framebuffer_bytes = static_cast<size_t>(stride) * height;
static const char* const tag = "xreader_m5paper";
static sdmmc_card_t* sd_card = nullptr;
static FILE* active_epub = nullptr;
static xr_storage_t active_epub_storage{};

struct port_t {
    xr_display_t display{};
    xr_platform_t platform{};
    xr_shell_t shell{};
    xreader::drivers::it8951e::device_t epd{};
    xreader::drivers::gt911::device_t touch{};
    uint8_t* framebuffer = nullptr;
    bool touch_down = false;
    uint16_t touch_x = 0;
    uint16_t touch_y = 0;
    bool rotary_right = false;
    bool rotary_left = false;
    bool rotary_press = false;
};

static uint32_t now_ms(void*)
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

static void wall_time(void*, int* hour, int* minute)
{
    *hour = -1;
    *minute = -1;
}

static int battery_percent(void*)
{
    uint16_t millivolts = 0;
    if (xreader::board::m5paper::battery_voltage_mv(&millivolts) != ESP_OK)
        return -1;
    return xreader::board::m5paper::battery_percent(millivolts);
}

static void display_update(xr_display_t* display, xr_rect_t area, xr_refresh_t mode)
{
    auto* port = static_cast<port_t*>(display->ctx);
    if (port == nullptr || area.w <= 0 || area.h <= 0)
        return;

    int x = area.x > 0 ? area.x : 0;
    int y = area.y > 0 ? area.y : 0;
    int right = area.x + area.w < width ? area.x + area.w : width;
    int bottom = area.y + area.h < height ? area.y + area.h : height;
    x &= ~3;
    right = (right + 3) & ~3;
    if (right > width)
        right = width;
    if (right <= x || bottom <= y)
        return;

    const uint16_t transfer_width = static_cast<uint16_t>(right - x);
    const uint16_t transfer_height = static_cast<uint16_t>(bottom - y);
    const size_t transfer_bytes = static_cast<size_t>(transfer_width / 2) * transfer_height;
    uint8_t* transfer = static_cast<uint8_t*>(heap_caps_malloc(
        transfer_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (transfer == nullptr)
        transfer = static_cast<uint8_t*>(heap_caps_malloc(transfer_bytes, MALLOC_CAP_8BIT));
    if (transfer == nullptr)
        return;

    for (uint16_t row = 0; row < transfer_height; ++row) {
        const uint8_t* source = port->framebuffer + static_cast<size_t>(y + row) * stride + x / 2;
        std::memcpy(transfer + static_cast<size_t>(row) * transfer_width / 2, source,
                    transfer_width / 2);
    }

    const auto panel_mode = mode == XR_REFRESH_FAST
        ? xreader::drivers::it8951e::refresh_du
        : xreader::drivers::it8951e::refresh_gc16;
    if (xreader::drivers::it8951e::write_image_4bpp(
            &port->epd, transfer, static_cast<uint16_t>(x), static_cast<uint16_t>(y),
            transfer_width, transfer_height) == ESP_OK) {
        (void)xreader::drivers::it8951e::refresh(
            &port->epd, static_cast<uint16_t>(x), static_cast<uint16_t>(y),
            transfer_width, transfer_height, panel_mode);
    }
    heap_caps_free(transfer);
}

static const xr_display_ops_t display_ops = { display_update, nullptr };
static const xr_platform_ops_t platform_ops = { now_ms, wall_time, battery_percent };

static void dispatch_key(port_t* port, xr_key_t key)
{
    xr_event_t event = xr_ev_key(key);
    xr_shell_dispatch(&port->shell, &event);
}

static void poll_input(port_t* port)
{
    xreader::drivers::gt911::state_t state{};
    if (xreader::drivers::gt911::read(&port->touch, &state) == ESP_OK && state.ready) {
        const bool active = state.count != 0;
        if (active) {
            port->touch_x = state.points[0].x < width ? state.points[0].x : width - 1;
            port->touch_y = state.points[0].y < height ? state.points[0].y : height - 1;
        }
        if (active != port->touch_down) {
            port->touch_down = active;
            if (active) {
                xr_event_t event = xr_ev_tap(port->touch_x, port->touch_y);
                xr_shell_dispatch(&port->shell, &event);
            }
        }
    }

    const bool right = gpio_get_level(xreader::board::m5paper::rotary_right_pin) == 0;
    const bool left = gpio_get_level(xreader::board::m5paper::rotary_left_pin) == 0;
    const bool press = gpio_get_level(xreader::board::m5paper::rotary_press_pin) == 0;
    if (port->rotary_right && !right) dispatch_key(port, XR_KEY_DOWN);
    if (port->rotary_left && !left) dispatch_key(port, XR_KEY_UP);
    if (port->rotary_press && !press) dispatch_key(port, XR_KEY_OK);
    port->rotary_right = right;
    port->rotary_left = left;
    port->rotary_press = press;
}

static void init_inputs()
{
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << xreader::board::m5paper::rotary_right_pin) |
                        (1ULL << xreader::board::m5paper::rotary_left_pin) |
                        (1ULL << xreader::board::m5paper::rotary_press_pin),
        .mode = GPIO_MODE_INPUT,
        /* GPIO37-39 are input-only on ESP32 and have no internal pull-ups;
         * the M5Paper board provides the required external bias. */
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
}

static bool list_sd_directory(void*, const char* path,
                              bool (*callback)(const char*, bool, void*), void* user)
{
    DIR* dir = opendir(path);
    if (!dir) {
        ESP_LOGW(tag, "cannot open SD directory: %s", path);
        return false;
    }
    unsigned entries = 0, directories = 0;
    while (struct dirent* item = readdir(dir)) {
        if (item->d_name[0] == '.') continue;
        char child[256];
        const int written = std::snprintf(child, sizeof child, "%s/%s", path, item->d_name);
        if (written < 0 || static_cast<size_t>(written) >= sizeof child) continue;
        struct stat file_stat{};
        bool is_directory = item->d_type == DT_DIR ||
                            (stat(child, &file_stat) == 0 && S_ISDIR(file_stat.st_mode));
        if (!is_directory && item->d_type == DT_UNKNOWN) {
            DIR* probe = opendir(child);
            if (probe) { closedir(probe); is_directory = true; }
        }
        ++entries;
        if (is_directory) ++directories;
        if (!callback(item->d_name, is_directory, user)) break;
    }
    closedir(dir);
    ESP_LOGI(tag, "scanned %s: %u entries, %u directories", path, entries, directories);
    return true;
}

static bool epub_read(void* context, uint32_t offset, void* destination, uint32_t size)
{
    FILE* file = static_cast<FILE*>(context);
    return offset <= static_cast<uint32_t>(LONG_MAX) &&
           fseek(file, static_cast<long>(offset), SEEK_SET) == 0 &&
           fread(destination, 1, size, file) == size;
}

static bool epub_inflate(void*, const xr_storage_t* storage, uint32_t source_offset,
                         uint32_t source_size, void* destination,
                         uint32_t destination_size)
{
    if (source_size == 0) return destination_size == 0;
    auto* source = static_cast<uint8_t*>(heap_caps_malloc(
        source_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!source) {
        ESP_LOGE(tag, "cannot allocate %" PRIu32 " bytes for DEFLATE input", source_size);
        return false;
    }
    const bool read = storage->read(storage->context, source_offset, source, source_size);
    size_t output_size = 0;
    const esp_err_t error = read
        ? xreader::inflate::decode(source, source_size, static_cast<uint8_t*>(destination),
                                   destination_size, &output_size)
        : ESP_FAIL;
    heap_caps_free(source);
    return error == ESP_OK && output_size == destination_size;
}

static bool load_selected_epub(const char* relative_path, const char* title)
{
    if (active_epub) {
        fclose(active_epub);
        active_epub = nullptr;
        active_epub_storage = {};
    }

    char path[512];
    const int written = std::snprintf(path, sizeof path, "/sdcard/%s", relative_path);
    if (written < 0 || static_cast<size_t>(written) >= sizeof path) return false;
    FILE* file = fopen(path, "rb");
    if (!file) {
        ESP_LOGE(tag, "cannot open EPUB: %s", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    const long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    active_epub = file;
    active_epub_storage = {
        active_epub, static_cast<uint32_t>(size), epub_read, epub_inflate,
    };
    if (app_load_epub(&active_epub_storage, title)) return true;
    fclose(active_epub);
    active_epub = nullptr;
    active_epub_storage = {};
    ESP_LOGE(tag, "cannot parse EPUB: %s", path);
    return false;
}

static void mount_and_scan_sd(void)
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = xreader::board::m5paper::epd_spi_host;
    sdspi_device_config_t device_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    device_config.host_id = xreader::board::m5paper::epd_spi_host;
    device_config.gpio_cs = xreader::board::m5paper::sd_cs_pin;
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 8, .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false, .use_one_fat = false,
    };
    if (esp_vfs_fat_sdspi_mount("/sdcard", &host, &device_config, &mount_config, &sd_card) == ESP_OK) {
        ESP_LOGI(tag, "SD card mounted");
        app_register_storage("/sdcard", list_sd_directory, nullptr);
        app_scan_library("/sdcard", list_sd_directory, nullptr);
        ESP_LOGI(tag, "SD EPUB library entries: %d", g_app.book_count);
    } else {
        ESP_LOGW(tag, "SD card mount failed");
    }
}

} // namespace

extern "C" void app_main(void)
{
    port_t port{};
    ESP_ERROR_CHECK(xreader::board::m5paper::power_on());
    init_inputs();

    const xreader::drivers::it8951e::config_t epd_config = {
        .spi_host = xreader::board::m5paper::epd_spi_host,
        .sck_pin = xreader::board::m5paper::epd_sck_pin,
        .mosi_pin = xreader::board::m5paper::epd_mosi_pin,
        .miso_pin = xreader::board::m5paper::epd_miso_pin,
        .cs_pin = xreader::board::m5paper::epd_cs_pin,
        .busy_pin = xreader::board::m5paper::epd_busy_pin,
        .width = xreader::board::m5paper::display_width,
        .height = xreader::board::m5paper::display_height,
        .rotation = 1,
        .spi_frequency_hz = 10000000,
    };
    ESP_ERROR_CHECK(xreader::drivers::it8951e::init(&port.epd, &epd_config));

    port.framebuffer = static_cast<uint8_t*>(heap_caps_calloc(
        1, framebuffer_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    ESP_ERROR_CHECK(port.framebuffer != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
    port.display = { &display_ops, width, height, XR_PIXFMT_GRAY4, port.framebuffer,
                     stride, 4, &port };
    port.platform = { &platform_ops, &port };

    const xreader::drivers::gt911::config_t touch_config = {
        .port = I2C_NUM_0,
        .sda_pin = xreader::board::m5paper::touch_sda_pin,
        .scl_pin = xreader::board::m5paper::touch_scl_pin,
        .frequency_hz = 100000,
    };
    ESP_ERROR_CHECK(xreader::drivers::gt911::init(&port.touch, &touch_config));

    xr_shell_init(&port.shell, &port.display, &port.platform, app_theme());
    app_start(&port.shell);
    app_set_epub_loader(load_selected_epub);
    /* Show the boot screen before the potentially slow SD-card walk. */
    xr_shell_tick(&port.shell);
    xr_shell_flush(&port.shell);
    mount_and_scan_sd();

    for (;;) {
        poll_input(&port);
        xr_shell_tick(&port.shell);
        xr_shell_flush(&port.shell);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
