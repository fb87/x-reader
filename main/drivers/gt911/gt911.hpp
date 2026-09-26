#pragma once

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

namespace xreader
{
namespace drivers
{
namespace gt911
{

static constexpr uint8_t max_points = 5;

struct config_t
{
    i2c_port_num_t port;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t frequency_hz;
};

struct point_t
{
    uint8_t id;
    uint16_t x;
    uint16_t y;
    uint16_t size;
};

struct state_t
{
    uint8_t count;
    point_t points[max_points];
};

struct device_t
{
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    uint8_t address;
};

esp_err_t init(device_t* device, const config_t* config);
esp_err_t read(device_t* device, state_t* state);

} // namespace gt911
} // namespace drivers
} // namespace xreader
