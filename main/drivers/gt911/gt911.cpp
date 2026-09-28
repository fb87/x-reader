#include "gt911.hpp"

#include <string.h>

#include "esp_log.h"

namespace xreader
{
namespace drivers
{
namespace gt911
{

namespace
{

static const char* const tag = "gt911";
static constexpr uint16_t status_register = 0x814e;
static constexpr uint16_t point_register = 0x8150;
static constexpr uint8_t primary_address = 0x14;
static constexpr uint8_t alternate_address = 0x5d;

static void encode_register(uint8_t* buffer, uint16_t address)
{
    buffer[0] = static_cast<uint8_t>(address >> 8);
    buffer[1] = static_cast<uint8_t>(address & 0xff);
}

static esp_err_t read_register(device_t* device, uint16_t address, uint8_t* data, size_t size)
{
    uint8_t register_bytes[2] = {};
    encode_register(register_bytes, address);
    return i2c_master_transmit_receive(device->device, register_bytes, sizeof(register_bytes), data,
                                       size, 100);
}

static esp_err_t write_register(device_t* device, uint16_t address, uint8_t value)
{
    uint8_t buffer[3] = {};
    encode_register(buffer, address);
    buffer[2] = value;
    return i2c_master_transmit(device->device, buffer, sizeof(buffer), 100);
}

static esp_err_t add_device(device_t* device, uint8_t address, uint32_t frequency_hz)
{
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = frequency_hz,
        .scl_wait_us = 0,
        .flags = {.disable_ack_check = false},
    };
    device->address = address;
    return i2c_master_bus_add_device(device->bus, &device_config, &device->device);
}

} // namespace

esp_err_t init(device_t* device, const config_t* config)
{
    if (device == nullptr || config == nullptr || config->frequency_hz == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = config->port,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {.enable_internal_pullup = true, .allow_pd = false},
    };
    esp_err_t error = i2c_new_master_bus(&bus_config, &device->bus);
    if (error != ESP_OK)
    {
        return error;
    }

    error = add_device(device, primary_address, config->frequency_hz);
    if (error != ESP_OK)
    {
        return error;
    }

    uint8_t status = 0;
    error = read_register(device, status_register, &status, sizeof(status));
    if (error != ESP_OK)
    {
        error = i2c_master_bus_rm_device(device->device);
        device->device = nullptr;
        if (error != ESP_OK)
        {
            return error;
        }
        error = add_device(device, alternate_address, config->frequency_hz);
        if (error != ESP_OK)
        {
            return error;
        }
        error = read_register(device, status_register, &status, sizeof(status));
    }
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "GT911 not found: %s", esp_err_to_name(error));
        return error;
    }

    ESP_LOGI(tag, "GT911 initialized at 0x%02x", device->address);
    return ESP_OK;
}

esp_err_t read(device_t* device, state_t* state)
{
    if (device == nullptr || device->device == nullptr || state == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }

    memset(state, 0, sizeof(*state));
    uint8_t status = 0;
    esp_err_t error = read_register(device, status_register, &status, sizeof(status));
    if (error != ESP_OK)
    {
        return error;
    }
    if ((status & 0x80U) == 0)
    {
        // No new GT911 report.  This is not a release event; preserve the current
        // input state until the controller publishes a fresh sample.
        return ESP_OK;
    }
    state->ready = true;

    const uint8_t count = status & 0x0fU;
    if (count > max_points)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }
    state->count = count;

    uint8_t point_data[max_points * 8] = {};
    if (count > 0)
    {
        error = read_register(device, point_register, point_data, count * 8);
        if (error != ESP_OK)
        {
            return error;
        }
    }

    for (uint8_t index = 0; index < count; ++index)
    {
        const uint8_t* point = point_data + index * 8;
        state->points[index].x = static_cast<uint16_t>(point[1] << 8) | point[0];
        state->points[index].y = static_cast<uint16_t>(point[3] << 8) | point[2];
        state->points[index].size = static_cast<uint16_t>(point[5] << 8) | point[4];
        state->points[index].id = point[7];
    }

    return write_register(device, status_register, 0);
}

} // namespace gt911
} // namespace drivers
} // namespace xreader
