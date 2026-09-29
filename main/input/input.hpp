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

enum key_t : uint8_t
{
    key_none,
    key_up,
    key_down,
    key_left,
    key_right,
    key_select,
    key_back,
    key_menu,
    key_page_next,
    key_page_prev,
    key_home,
    key_power,
};

enum event_type_t : uint8_t
{
    event_touch_down,
    event_touch_move,
    event_touch_up,
    event_rotary_clockwise,
    event_rotary_counterclockwise,
    event_button_down,
    event_button_up,
    event_key_down,
    event_key_up,
    event_key_repeat,
};

struct event_t
{
    event_type_t type;
    uint16_t x;
    uint16_t y;
    key_t key = key_none;
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
bool enqueue_key(QueueHandle_t events, key_t key, bool pressed);
bool enqueue_key_repeat(QueueHandle_t events, key_t key);
void flush(QueueHandle_t events);

} // namespace input
} // namespace xreader
