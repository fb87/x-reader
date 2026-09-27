#include "input.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace xreader
{
namespace input
{

namespace
{

struct task_context_t
{
    config_t config;
    QueueHandle_t events;
    uint8_t rotary_state;
    int8_t rotary_quarters;
    bool button_state;
    bool touch_active;
    uint16_t touch_x;
    uint16_t touch_y;
};

static task_context_t task_context = {};

static void send(task_context_t* context, event_t event)
{
    xQueueSend(context->events, &event, 0);
}

static void poll_rotary(task_context_t* context)
{
    static constexpr int8_t transition_table[16] = {
        0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0,
    };
    const uint8_t right = static_cast<uint8_t>(gpio_get_level(context->config.rotary_right_pin));
    const uint8_t left = static_cast<uint8_t>(gpio_get_level(context->config.rotary_left_pin));
    const uint8_t state = static_cast<uint8_t>((right << 1) | left);
    const uint8_t transition = static_cast<uint8_t>((context->rotary_state << 2) | state);
    context->rotary_quarters += transition_table[transition];
    context->rotary_state = state;

    if (context->rotary_quarters >= 4)
    {
        send(context, {event_rotary_clockwise, 0, 0});
        context->rotary_quarters = 0;
    }
    else if (context->rotary_quarters <= -4)
    {
        send(context, {event_rotary_counterclockwise, 0, 0});
        context->rotary_quarters = 0;
    }
}

static void poll_button(task_context_t* context)
{
    const bool pressed = gpio_get_level(context->config.rotary_press_pin) == 0;
    if (pressed == context->button_state)
    {
        return;
    }
    context->button_state = pressed;
    send(context, {pressed ? event_button_down : event_button_up, 0, 0});
}

static void poll_touch(task_context_t* context)
{
    drivers::gt911::state_t state = {};
    if (drivers::gt911::read(context->config.touch, &state) != ESP_OK)
    {
        return;
    }

    if (state.count == 0)
    {
        if (context->touch_active)
        {
            send(context, {event_touch_up, context->touch_x, context->touch_y});
            context->touch_active = false;
        }
        return;
    }

    if (!context->touch_active)
        send(context, {event_touch_down, state.points[0].x, state.points[0].y});
    context->touch_x = state.points[0].x;
    context->touch_y = state.points[0].y;
    context->touch_active = true;
}

static void task(void* argument)
{
    task_context_t* context = static_cast<task_context_t*>(argument);
    while (true)
    {
        poll_rotary(context);
        poll_button(context);
        poll_touch(context);
        vTaskDelay(pdMS_TO_TICKS(context->config.poll_interval_ms));
    }
}

} // namespace

esp_err_t start(const config_t* config, QueueHandle_t events)
{
    if (config == nullptr || config->touch == nullptr || config->poll_interval_ms == 0 ||
        events == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const gpio_config_t input_gpio_config = {
        .pin_bit_mask = (1ULL << config->rotary_right_pin) | (1ULL << config->rotary_press_pin) |
                        (1ULL << config->rotary_left_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t error = gpio_config(&input_gpio_config);
    if (error != ESP_OK)
    {
        return error;
    }

    task_context.config = *config;
    task_context.events = events;
    task_context.rotary_state = static_cast<uint8_t>(
        (gpio_get_level(config->rotary_right_pin) << 1) | gpio_get_level(config->rotary_left_pin));
    task_context.button_state = gpio_get_level(config->rotary_press_pin) == 0;
    task_context.rotary_quarters = 0;
    task_context.touch_active = false;
    task_context.touch_x = 0;
    task_context.touch_y = 0;

    if (xTaskCreate(task, "xreader_input", 4096, &task_context, 5, nullptr) != pdPASS)
    {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

} // namespace input
} // namespace xreader
