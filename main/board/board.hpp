#pragma once

#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace board
{

enum display_controller_t
{
    display_controller_unknown,
    display_controller_it8951e,
    display_controller_ssd1677,
};

enum storage_bus_t
{
    storage_bus_unknown,
    storage_bus_spi,
};

struct capabilities_t
{
    bool configured;
    uint16_t display_width;
    uint16_t display_height;
    display_controller_t display_controller;
    storage_bus_t storage_bus;
    bool has_touch;
    bool has_rotary;
    bool has_sd;
    bool supports_power_control;
    bool supports_deep_sleep;
};

esp_err_t get_capabilities(capabilities_t* capabilities);

} // namespace board
} // namespace xreader
