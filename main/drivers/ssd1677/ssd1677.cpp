#include "ssd1677.hpp"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Protocol ported from the Vaulco/x4-firmware EInkDisplay reference driver
// (MIT-licensed, github.com/Vaulco/x4-firmware) for the same GDEQ0426T82
// 800x480 panel and pin-out this board uses. See docs' ssd1677 guide fetched
// during bring-up for the full command reference.

namespace xreader
{
namespace drivers
{
namespace ssd1677
{

namespace
{

static const char* const tag = "ssd1677";

static constexpr uint8_t cmd_driver_output_control = 0x01;
static constexpr uint8_t cmd_booster_soft_start = 0x0C;
static constexpr uint8_t cmd_deep_sleep = 0x10;
static constexpr uint8_t cmd_data_entry_mode = 0x11;
static constexpr uint8_t cmd_soft_reset = 0x12;
static constexpr uint8_t cmd_temp_sensor_control = 0x18;
static constexpr uint8_t cmd_write_temperature = 0x1A;
static constexpr uint8_t cmd_master_activation = 0x20;
static constexpr uint8_t cmd_display_update_control1 = 0x21;
static constexpr uint8_t cmd_display_update_control2 = 0x22;
static constexpr uint8_t cmd_write_ram_bw = 0x24;
static constexpr uint8_t cmd_write_ram_red = 0x26;
static constexpr uint8_t cmd_border_waveform = 0x3C;
static constexpr uint8_t cmd_set_ram_x_range = 0x44;
static constexpr uint8_t cmd_set_ram_y_range = 0x45;
static constexpr uint8_t cmd_auto_write_bw_ram = 0x46;
static constexpr uint8_t cmd_auto_write_red_ram = 0x47;
static constexpr uint8_t cmd_set_ram_x_counter = 0x4E;
static constexpr uint8_t cmd_set_ram_y_counter = 0x4F;

static constexpr uint32_t wait_timeout_ms = 10000;
static constexpr size_t transfer_chunk_size = 4096;

static esp_err_t wait_ready(const device_t* device)
{
    const TickType_t start = xTaskGetTickCount();
    while (gpio_get_level(device->busy_pin) != 0)
    {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(wait_timeout_ms))
        {
            ESP_LOGE(tag, "SSD1677 busy timeout");
            return ESP_ERR_TIMEOUT;
        }
        // At this project's 100Hz tick rate, pdMS_TO_TICKS(1) truncates to 0,
        // which makes vTaskDelay() a same-priority yield instead of an actual
        // block -- during a multi-second refresh that starves the IDLE task
        // long enough to trip the watchdog. Block at least one real tick.
        vTaskDelay(1);
    }
    return ESP_OK;
}

static esp_err_t transmit(device_t* device, const uint8_t* data, size_t size)
{
    spi_transaction_t transaction = {};
    transaction.length = size * 8;
    transaction.tx_buffer = data;
    esp_err_t error = gpio_set_level(device->cs_pin, 0);
    if (error == ESP_OK)
        error = spi_device_polling_transmit(device->spi, &transaction);
    const esp_err_t release_error = gpio_set_level(device->cs_pin, 1);
    return error == ESP_OK ? release_error : error;
}

static esp_err_t send_command(device_t* device, uint8_t command)
{
    esp_err_t error = gpio_set_level(device->dc_pin, 0);
    if (error != ESP_OK)
        return error;
    return transmit(device, &command, 1);
}

static esp_err_t send_data(device_t* device, uint8_t value)
{
    esp_err_t error = gpio_set_level(device->dc_pin, 1);
    if (error != ESP_OK)
        return error;
    return transmit(device, &value, 1);
}

static esp_err_t send_data_buffer(device_t* device, const uint8_t* data, size_t size)
{
    esp_err_t error = gpio_set_level(device->dc_pin, 1);
    if (error != ESP_OK)
        return error;
    uint8_t* transfer = static_cast<uint8_t*>(
        heap_caps_malloc(transfer_chunk_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (transfer == nullptr)
        return ESP_ERR_NO_MEM;
    const int64_t start = esp_timer_get_time();
    size_t chunks = 0;
    for (size_t offset = 0; offset < size && error == ESP_OK;)
    {
        const size_t remaining = size - offset;
        const size_t chunk = remaining < transfer_chunk_size ? remaining : transfer_chunk_size;
        memcpy(transfer, &data[offset], chunk);
        error = transmit(device, transfer, chunk);
        offset += chunk;
        ++chunks;
    }
    const int64_t elapsed_us = esp_timer_get_time() - start;
    ESP_LOGI(tag, "pixel upload: %u bytes in %u chunks, %lld ms", static_cast<unsigned>(size),
             static_cast<unsigned>(chunks), static_cast<long long>(elapsed_us / 1000));
    heap_caps_free(transfer);
    return error;
}

// Sets the RAM window to the full panel and resets the write counters to its
// origin. Y is reversed (gates run bottom-to-top on this panel) -- see the
// ssd1677 guide's "RAM Operations" section.
static esp_err_t set_full_ram_area(device_t* device)
{
    const uint16_t width = device->width;
    const uint16_t height = device->height;
    esp_err_t error = send_command(device, cmd_data_entry_mode);
    if (error == ESP_OK)
        error = send_data(device, 0x01); // X increment, Y decrement (reversed gates)

    if (error == ESP_OK)
        error = send_command(device, cmd_set_ram_x_range);
    if (error == ESP_OK)
        error = send_data(device, 0x00);
    if (error == ESP_OK)
        error = send_data(device, 0x00);
    if (error == ESP_OK)
        error = send_data(device, static_cast<uint8_t>((width - 1U) % 256U));
    if (error == ESP_OK)
        error = send_data(device, static_cast<uint8_t>((width - 1U) / 256U));

    const uint16_t y_end = static_cast<uint16_t>(height - 1U);
    if (error == ESP_OK)
        error = send_command(device, cmd_set_ram_y_range);
    if (error == ESP_OK)
        error = send_data(device, static_cast<uint8_t>(y_end % 256U));
    if (error == ESP_OK)
        error = send_data(device, static_cast<uint8_t>(y_end / 256U));
    if (error == ESP_OK)
        error = send_data(device, 0x00);
    if (error == ESP_OK)
        error = send_data(device, 0x00);

    if (error == ESP_OK)
        error = send_command(device, cmd_set_ram_x_counter);
    if (error == ESP_OK)
        error = send_data(device, 0x00);
    if (error == ESP_OK)
        error = send_data(device, 0x00);

    if (error == ESP_OK)
        error = send_command(device, cmd_set_ram_y_counter);
    if (error == ESP_OK)
        error = send_data(device, static_cast<uint8_t>(y_end % 256U));
    if (error == ESP_OK)
        error = send_data(device, static_cast<uint8_t>(y_end / 256U));
    return error;
}

static void reset_display(device_t* device)
{
    gpio_set_level(device->reset_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(device->reset_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(device->reset_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
}

} // namespace

size_t framebuffer_size(uint16_t width, uint16_t height)
{
    return (static_cast<size_t>(width) * height + 7U) / 8U;
}

esp_err_t init(device_t* device, const config_t* config)
{
    if (device == nullptr || config == nullptr || config->width == 0 || config->height == 0)
        return ESP_ERR_INVALID_ARG;

    const gpio_config_t input_config = {
        .pin_bit_mask = 1ULL << config->busy_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t error = gpio_config(&input_config);
    if (error != ESP_OK)
        return error;

    const gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << config->cs_pin) | (1ULL << config->dc_pin) |
                        (1ULL << config->reset_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    error = gpio_config(&output_config);
    if (error != ESP_OK)
        return error;
    error = gpio_set_level(config->cs_pin, 1);
    if (error != ESP_OK)
        return error;
    error = gpio_set_level(config->dc_pin, 1);
    if (error != ESP_OK)
        return error;

    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = config->mosi_pin;
    bus_config.miso_io_num = config->miso_pin;
    bus_config.sclk_io_num = config->sck_pin;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = transfer_chunk_size;
    error = spi_bus_initialize(config->spi_host, &bus_config, SPI_DMA_CH_AUTO);
    if (error != ESP_OK)
        return error;

    spi_device_interface_config_t device_config = {};
    device_config.clock_speed_hz = static_cast<int>(config->spi_frequency_hz);
    device_config.mode = 0;
    device_config.spics_io_num = -1;
    device_config.queue_size = 1;
    error = spi_bus_add_device(config->spi_host, &device_config, &device->spi);
    if (error != ESP_OK)
    {
        spi_bus_free(config->spi_host);
        return error;
    }

    device->cs_pin = config->cs_pin;
    device->dc_pin = config->dc_pin;
    device->reset_pin = config->reset_pin;
    device->busy_pin = config->busy_pin;
    device->width = config->width;
    device->height = config->height;
    device->screen_on = false;

    reset_display(device);
    error = send_command(device, cmd_soft_reset);
    if (error == ESP_OK)
        error = wait_ready(device);
    if (error == ESP_OK)
        error = send_command(device, cmd_temp_sensor_control);
    if (error == ESP_OK)
        error = send_data(device, 0x80); // internal sensor

    // Booster soft-start values from the reference driver, tuned for this
    // specific panel (GDEQ0426T82).
    if (error == ESP_OK)
        error = send_command(device, cmd_booster_soft_start);
    static constexpr uint8_t booster[] = {0xAE, 0xC7, 0xC3, 0xC0, 0x40};
    for (size_t index = 0; error == ESP_OK && index < sizeof(booster); ++index)
        error = send_data(device, booster[index]);

    if (error == ESP_OK)
        error = send_command(device, cmd_driver_output_control);
    const uint16_t gates = static_cast<uint16_t>(device->height - 1U);
    if (error == ESP_OK)
        error = send_data(device, static_cast<uint8_t>(gates % 256U));
    if (error == ESP_OK)
        error = send_data(device, static_cast<uint8_t>(gates / 256U));
    if (error == ESP_OK)
        error = send_data(device, 0x02);

    if (error == ESP_OK)
        error = send_command(device, cmd_border_waveform);
    if (error == ESP_OK)
        error = send_data(device, 0x01);

    if (error == ESP_OK)
        error = set_full_ram_area(device);

    if (error == ESP_OK)
        error = send_command(device, cmd_auto_write_bw_ram);
    if (error == ESP_OK)
        error = send_data(device, 0xF7);
    if (error == ESP_OK)
        error = wait_ready(device);
    if (error == ESP_OK)
        error = send_command(device, cmd_auto_write_red_ram);
    if (error == ESP_OK)
        error = send_data(device, 0xF7);
    if (error == ESP_OK)
        error = wait_ready(device);

    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "SSD1677 initialization failed: %s", esp_err_to_name(error));
        return error;
    }

    ESP_LOGI(tag, "SSD1677 initialized at %ux%u", device->width, device->height);
    return ESP_OK;
}

esp_err_t write_image_1bpp(device_t* device, const uint8_t* pixels, refresh_mode_t mode)
{
    if (device == nullptr || pixels == nullptr)
        return ESP_ERR_INVALID_ARG;
    const size_t size = framebuffer_size(device->width, device->height);

    esp_err_t error = set_full_ram_area(device);
    if (error == ESP_OK)
        error = send_command(device, cmd_write_ram_bw);
    if (error == ESP_OK)
        error = send_data_buffer(device, pixels, size);

    // Fast refresh diffs the new BW frame against whatever RED RAM already
    // holds -- which refresh() left as the *previously displayed* frame --
    // so RED must NOT be overwritten here, or there would be nothing left to
    // diff against. Full/half refresh don't diff, so keeping RED in sync here
    // too is harmless (refresh() re-syncs it again after either way).
    if (mode != refresh_fast)
    {
        if (error == ESP_OK)
            error = set_full_ram_area(device);
        if (error == ESP_OK)
            error = send_command(device, cmd_write_ram_red);
        if (error == ESP_OK)
            error = send_data_buffer(device, pixels, size);
    }
    return error;
}

esp_err_t refresh(device_t* device, const uint8_t* pixels, refresh_mode_t mode)
{
    if (device == nullptr || pixels == nullptr)
        return ESP_ERR_INVALID_ARG;

    static constexpr uint8_t ctrl1_bypass_red = 0x40;
    static constexpr uint8_t ctrl1_normal = 0x00;

    esp_err_t error = send_command(device, cmd_display_update_control1);
    if (error == ESP_OK)
        error = send_data(device, mode == refresh_fast ? ctrl1_normal : ctrl1_bypass_red);
    if (error == ESP_OK)
        error = send_data(device, 0x00);

    // bit | hex | name          | effect
    // 7   | 80  | CLOCK_ON      | start internal oscillator
    // 6   | 40  | ANALOG_ON     | enable analog power rails
    // 5   | 20  | TEMP_LOAD     | load temperature value
    // 4   | 10  | LUT_LOAD      | load waveform LUT
    // 3   | 08  | MODE_SELECT   | mode 1/2
    // 2   | 04  | DISPLAY_START | run display
    // Matches the open-x4-epaper community-sdk EInkDisplay driver for this
    // same panel: full/half always re-power the rails (half additionally
    // writes an explicit high temperature for a lighter/faster waveform);
    // fast only re-powers if the screen was off, trading that latency for
    // the between-refresh power draw of leaving the rails up.
    uint8_t display_mode = 0x00;
    if (!device->screen_on)
    {
        device->screen_on = true;
        display_mode |= 0xC0; // CLOCK_ON | ANALOG_ON
    }
    if (mode == refresh_full)
    {
        display_mode |= 0x34; // TEMP_LOAD | LUT_LOAD | DISPLAY_START
    }
    else if (mode == refresh_half)
    {
        if (error == ESP_OK)
            error = send_command(device, cmd_write_temperature);
        if (error == ESP_OK)
            error = send_data(device, 0x5A);
        display_mode |= 0xD4; // CLOCK_ON | ANALOG_ON | LUT_LOAD | DISPLAY_START
    }
    else
    {
        display_mode |= 0x1C; // LUT_LOAD | MODE_SELECT | DISPLAY_START
    }

    if (error == ESP_OK)
        error = send_command(device, cmd_display_update_control2);
    if (error == ESP_OK)
        error = send_data(device, display_mode);

    if (error == ESP_OK)
        error = send_command(device, cmd_master_activation);
    if (error == ESP_OK)
        error = wait_ready(device);

    // Sync RED RAM to the frame just displayed so the next fast refresh (if
    // any) has the right previous-frame reference to diff against.
    if (error == ESP_OK)
        error = set_full_ram_area(device);
    if (error == ESP_OK)
        error = send_command(device, cmd_write_ram_red);
    if (error == ESP_OK)
        error = send_data_buffer(device, pixels, framebuffer_size(device->width, device->height));
    return error;
}

void deep_sleep(device_t* device)
{
    if (device == nullptr)
        return;
    send_command(device, cmd_display_update_control1);
    send_data(device, 0x40);
    send_command(device, cmd_display_update_control2);
    send_data(device, 0x03);
    send_command(device, cmd_master_activation);
    wait_ready(device);
    send_command(device, cmd_deep_sleep);
    send_data(device, 0x01);
}

} // namespace ssd1677
} // namespace drivers
} // namespace xreader
