#include "it8951e.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace xreader
{
namespace drivers
{
namespace it8951e
{

namespace
{

static const char* const tag = "it8951e";

static constexpr uint16_t command_write = 0x6000;
static constexpr uint16_t tcon_system_run = 0x0001;
static constexpr uint16_t tcon_register_read = 0x0010;
static constexpr uint16_t tcon_register_write = 0x0011;
static constexpr uint16_t tcon_load_image_area = 0x0021;
static constexpr uint16_t tcon_load_image_end = 0x0022;
static constexpr uint16_t display_buffer_area = 0x0037;
static constexpr uint16_t vcom = 0x0039;

static constexpr uint16_t i80_control_register = 0x0004;
static constexpr uint16_t lisar_register = 0x0208;
static constexpr uint32_t default_memory_address = 0x001236e0;
static constexpr uint32_t wait_timeout_ms = 3000;
static constexpr size_t transfer_chunk_size = 4096;

static esp_err_t wait_ready(const device_t* device)
{
    const TickType_t start = xTaskGetTickCount();
    while (gpio_get_level(device->busy_pin) == 0)
    {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(wait_timeout_ms))
        {
            ESP_LOGE(tag, "IT8951E busy timeout");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_OK;
}

static esp_err_t transmit(device_t* device, const uint8_t* tx_data, uint8_t* rx_data, size_t size)
{
    spi_transaction_t transaction = {};
    transaction.length = size * 8;
    transaction.tx_buffer = tx_data;
    transaction.rx_buffer = rx_data;
    esp_err_t error = gpio_set_level(device->cs_pin, 0);
    if (error == ESP_OK)
    {
        error = spi_device_polling_transmit(device->spi, &transaction);
    }
    const esp_err_t release_error = gpio_set_level(device->cs_pin, 1);
    return error == ESP_OK ? release_error : error;
}

static void encode_word(uint8_t* buffer, uint16_t value)
{
    buffer[0] = static_cast<uint8_t>(value >> 8);
    buffer[1] = static_cast<uint8_t>(value & 0xff);
}

static esp_err_t write_command(device_t* device, uint16_t command)
{
    esp_err_t error = wait_ready(device);
    if (error != ESP_OK)
    {
        return error;
    }

    uint8_t buffer[4] = {};
    encode_word(buffer, command_write);
    encode_word(buffer + 2, command);
    return transmit(device, buffer, nullptr, sizeof(buffer));
}

static esp_err_t write_word(device_t* device, uint16_t value)
{
    esp_err_t error = wait_ready(device);
    if (error != ESP_OK)
    {
        return error;
    }

    uint8_t buffer[4] = {};
    encode_word(buffer, 0);
    encode_word(buffer + 2, value);
    return transmit(device, buffer, nullptr, sizeof(buffer));
}

static esp_err_t write_register(device_t* device, uint16_t address, uint16_t value)
{
    esp_err_t error = write_command(device, tcon_register_write);
    if (error != ESP_OK)
    {
        return error;
    }
    error = write_word(device, address);
    if (error != ESP_OK)
    {
        return error;
    }
    return write_word(device, value);
}

static esp_err_t set_target_memory_address(device_t* device)
{
    esp_err_t error = write_register(device, static_cast<uint16_t>(lisar_register + 2),
                                     device->device_memory_high);
    if (error != ESP_OK)
    {
        return error;
    }
    return write_register(device, lisar_register, device->device_memory_low);
}

static esp_err_t set_image_area(device_t* device, uint16_t x, uint16_t y, uint16_t width,
                                uint16_t height)
{
    const uint16_t args[] = {
        static_cast<uint16_t>((1U << 8) | (2U << 4) | device->rotation), x, y, width, height,
    };

    esp_err_t error = write_command(device, tcon_load_image_area);
    for (size_t index = 0; error == ESP_OK && index < sizeof(args) / sizeof(args[0]); ++index)
    {
        error = write_word(device, args[index]);
    }
    return error;
}

static esp_err_t write_pixel_data(device_t* device, const uint8_t* pixels, size_t size)
{
    if ((size & 1U) != 0)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t offset = 0; offset < size; offset += 2)
    {
        const uint8_t transfer[4] = {
            0x00,
            0x00,
            static_cast<uint8_t>(~pixels[offset]),
            static_cast<uint8_t>(~pixels[offset + 1]),
        };
        const esp_err_t error = transmit(device, transfer, nullptr, sizeof(transfer));
        if (error != ESP_OK)
        {
            return error;
        }
        if ((offset & 0x1ffU) == 0 && device->watchdog_user != nullptr)
            (void)esp_task_wdt_reset_user(device->watchdog_user);
    }
    return ESP_OK;
}

} // namespace

size_t framebuffer_size(uint16_t width, uint16_t height)
{
    return (static_cast<size_t>(width) * height + 1) / 2;
}

esp_err_t init(device_t* device, const config_t* config)
{
    if (device == nullptr || config == nullptr || config->width == 0 || config->height == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const gpio_config_t busy_config = {
        .pin_bit_mask = 1ULL << config->busy_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t error = gpio_config(&busy_config);
    if (error != ESP_OK)
    {
        return error;
    }

    const gpio_config_t cs_config = {
        .pin_bit_mask = 1ULL << config->cs_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    error = gpio_config(&cs_config);
    if (error != ESP_OK)
    {
        return error;
    }
    error = gpio_set_level(config->cs_pin, 1);
    if (error != ESP_OK)
    {
        return error;
    }

    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = config->mosi_pin;
    bus_config.miso_io_num = config->miso_pin;
    bus_config.sclk_io_num = config->sck_pin;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = transfer_chunk_size;
    error = spi_bus_initialize(config->spi_host, &bus_config, SPI_DMA_CH_AUTO);
    if (error != ESP_OK)
    {
        return error;
    }

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
    device->busy_pin = config->busy_pin;
    device->width = config->width;
    device->height = config->height;
    device->rotation = config->rotation;
    device->device_memory_low = static_cast<uint16_t>(default_memory_address);
    device->device_memory_high = static_cast<uint16_t>(default_memory_address >> 16);
    error = write_command(device, tcon_system_run);
    if (error == ESP_OK)
    {
        error = write_register(device, i80_control_register, 0x0001);
    }
    if (error == ESP_OK)
    {
        error = write_command(device, vcom);
    }
    if (error == ESP_OK)
    {
        error = write_word(device, 0x0001);
    }
    if (error == ESP_OK)
    {
        error = write_word(device, 2300);
    }
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "IT8951E initialization failed: %s", esp_err_to_name(error));
        return error;
    }

    ESP_LOGI(tag, "IT8951E initialized at %ux%u", device->width, device->height);
    return ESP_OK;
}

esp_err_t write_image_4bpp(device_t* device, const uint8_t* pixels, uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height)
{
    if (device == nullptr || pixels == nullptr || width == 0 || height == 0 || (width & 3U) != 0 ||
        x + width > device->width || y + height > device->height)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t error = esp_task_wdt_add_user(tag, &device->watchdog_user);
    if (error != ESP_OK)
        device->watchdog_user = nullptr;
    error = set_target_memory_address(device);
    if (error == ESP_OK)
    {
        error = set_image_area(device, x, y, width, height);
    }
    if (error == ESP_OK)
    {
        error = write_pixel_data(device, pixels, framebuffer_size(width, height));
    }
    if (error == ESP_OK)
    {
        error = write_command(device, tcon_load_image_end);
    }
    if (device->watchdog_user != nullptr)
    {
        (void)esp_task_wdt_delete_user(device->watchdog_user);
        device->watchdog_user = nullptr;
    }
    return error;
}

esp_err_t refresh(device_t* device, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                  refresh_mode_t mode)
{
    if (device == nullptr || width == 0 || height == 0 || x + width > device->width ||
        y + height > device->height)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t target_x = x;
    uint16_t target_y = y;
    uint16_t target_width = width;
    uint16_t target_height = height;
    if (device->rotation == 1)
    {
        target_x = y;
        target_y = static_cast<uint16_t>(device->width - width - x);
        target_width = height;
        target_height = width;
    }

    const uint16_t args[] = {
        target_x,
        target_y,
        target_width,
        target_height,
        static_cast<uint16_t>(mode),
        device->device_memory_low,
        device->device_memory_high,
    };

    esp_err_t error = write_command(device, display_buffer_area);
    for (size_t index = 0; error == ESP_OK && index < sizeof(args) / sizeof(args[0]); ++index)
    {
        error = write_word(device, args[index]);
    }
    if (error == ESP_OK)
    {
        error = wait_ready(device);
    }
    return error;
}

} // namespace it8951e
} // namespace drivers
} // namespace xreader
