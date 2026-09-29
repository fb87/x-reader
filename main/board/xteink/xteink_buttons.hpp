#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "input/input.hpp"

namespace xreader
{
namespace board
{
namespace xteink
{

// Pure decoders are exposed so the resistor-ladder thresholds can be host-tested.
input::key_t decode_ladder1(int raw);
input::key_t decode_ladder2(int raw);

struct button_config_t
{
    uint32_t poll_interval_ms = 20;
    uint8_t stable_samples = 2;
    uint32_t power_long_press_ms = 800;
};

esp_err_t start_buttons(QueueHandle_t events, const button_config_t* config = nullptr);

} // namespace xteink
} // namespace board
} // namespace xreader
