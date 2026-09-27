#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "drivers/gt911/gt911.hpp"

namespace xreader
{
namespace input
{

enum event_type_t : uint8_t
{
    event_touch_down,
    event_touch_move,
    event_touch_up,
    event_rotary_clockwise,
    event_rotary_counterclockwise,
    event_button_down,
    event_button_up,
};

struct event_t
{
    event_type_t type;
    uint16_t x;
    uint16_t y;
};

struct config_t
{
    gpio_num_t rotary_right_pin;
    gpio_num_t rotary_press_pin;
    gpio_num_t rotary_left_pin;
    drivers::gt911::device_t* touch;
    uint32_t poll_interval_ms;
    uint16_t touch_width;
    uint16_t touch_height;
    uint8_t touch_rotation;
};

esp_err_t start(const config_t* config, QueueHandle_t events);
void flush(QueueHandle_t events);

} // namespace input
} // namespace xreader
