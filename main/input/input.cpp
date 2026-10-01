#include "input.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace xreader
{
namespace input
{

namespace
{

static constexpr uint8_t debounce_samples = 5;
#if XREADER_DIAGNOSTICS
static const char* const tag = "input";

static const char* event_name(event_type_t type)
{
    switch (type)
    {
    case event_touch_down:
        return "touch_down";
    case event_touch_up:
        return "touch_up";
    case event_rotary_clockwise:
        return "rotary_clockwise";
    case event_rotary_counterclockwise:
        return "rotary_counterclockwise";
    case event_rotary_double_clockwise:
        return "rotary_double_clockwise";
    case event_rotary_double_counterclockwise:
        return "rotary_double_counterclockwise";
    case event_button_down:
        return "button_down";
    case event_button_up:
        return "button_up";
    case event_button_long_press:
        return "button_long_press";
    default:
        return "unknown";
    }
}
#endif

static void touch_coordinates(const config_t* config, uint16_t raw_x, uint16_t raw_y, uint16_t* x,
                              uint16_t* y)
{
    if (config->touch_rotation == 1)
    {
        *x = raw_y;
        *y = raw_x <= config->touch_width ? static_cast<uint16_t>(config->touch_width - raw_x) : 0;
    }
    else
    {
        *x = raw_x;
        *y = raw_y;
    }
}

// Debounced state for one momentary button, shared by the centre/power button
// and the two "rotary" pins.  Named generically because it is reused for all
// three physical buttons below.
struct button_debounce_t
{
    bool state;
    bool candidate;
    uint8_t stable_samples;
};

struct task_context_t
{
    config_t config;
    QueueHandle_t events;
    button_debounce_t press_button;
    TickType_t press_start_tick;
    bool press_long_fired;
    button_debounce_t right_button;
    button_debounce_t left_button;
    TickType_t last_clockwise_tick;
    TickType_t last_counterclockwise_tick;
    bool touch_active;
    uint16_t touch_x;
    uint16_t touch_y;
};

static task_context_t task_context = {};

static void send(task_context_t* context, event_t event)
{
    xQueueSend(context->events, &event, 0);
#if XREADER_DIAGNOSTICS
    ESP_LOGI(tag, "emit %s x=%u y=%u", event_name(event.type), static_cast<unsigned>(event.x),
             static_cast<unsigned>(event.y));
#endif
}

// Returns true exactly once, on the sample where a debounced press-to-release
// transition completes (i.e. a short click).  A debounced press-to-press "no
// change" or a still-bouncing read returns false.
static bool debounce_click(button_debounce_t* button, bool pressed)
{
    if (pressed != button->candidate)
    {
        button->candidate = pressed;
        button->stable_samples = 0;
        return false;
    }
    if (button->stable_samples < debounce_samples)
        ++button->stable_samples;
    if (button->stable_samples >= debounce_samples && pressed != button->state)
    {
        button->state = pressed;
        return !pressed;
    }
    return false;
}

// The M5Paper board does not have a continuous rotary encoder: "rotary right"
// and "rotary left" are two independent momentary buttons (confirmed on
// hardware -- a quadrature gray-code decoder, which is what this function
// used to run, requires a true encoder's alternating two-phase signal and a
// plain button press does not reliably produce one, which is why "rotating"
// appeared to do nothing).  Each fires its rotary event on release, matching
// how the centre button already fires event_button_up on release below.
// Two clicks of the same direction within this window are a distinct "double"
// gesture (jump focus into/out of the bottom action bar). The double event is
// layered on top of the normal click rather than replacing it, so a plain
// single click never waits around to see if a second one is coming.
static constexpr uint32_t double_press_ms = 400;

static void poll_rotary(task_context_t* context)
{
    const bool right_pressed = gpio_get_level(context->config.rotary_right_pin) == 0;
    if (debounce_click(&context->right_button, right_pressed))
    {
        const TickType_t now = xTaskGetTickCount();
        send(context, {event_rotary_clockwise, 0, 0});
        if (static_cast<uint32_t>(now - context->last_clockwise_tick) <=
            pdMS_TO_TICKS(double_press_ms))
            send(context, {event_rotary_double_clockwise, 0, 0});
        context->last_clockwise_tick = now;
    }

    const bool left_pressed = gpio_get_level(context->config.rotary_left_pin) == 0;
    if (debounce_click(&context->left_button, left_pressed))
    {
        const TickType_t now = xTaskGetTickCount();
        send(context, {event_rotary_counterclockwise, 0, 0});
        if (static_cast<uint32_t>(now - context->last_counterclockwise_tick) <=
            pdMS_TO_TICKS(double_press_ms))
            send(context, {event_rotary_double_counterclockwise, 0, 0});
        context->last_counterclockwise_tick = now;
    }
}

static constexpr uint32_t long_press_ms = 700;

static void poll_button(task_context_t* context)
{
    const bool pressed = gpio_get_level(context->config.rotary_press_pin) == 0;
    const bool was_pressed = context->press_button.state;
    if (pressed != context->press_button.candidate)
    {
        context->press_button.candidate = pressed;
        context->press_button.stable_samples = 0;
    }
    else if (context->press_button.stable_samples < debounce_samples)
    {
        ++context->press_button.stable_samples;
        if (context->press_button.stable_samples >= debounce_samples &&
            pressed != context->press_button.state)
        {
            context->press_button.state = pressed;
            if (pressed)
            {
                context->press_start_tick = xTaskGetTickCount();
                context->press_long_fired = false;
            }
        }
    }

    if (context->press_button.state && !was_pressed)
        send(context, {event_button_down, 0, 0});

    if (context->press_button.state && !context->press_long_fired &&
        static_cast<uint32_t>(xTaskGetTickCount() - context->press_start_tick) >=
            pdMS_TO_TICKS(long_press_ms))
    {
        context->press_long_fired = true;
        send(context, {event_button_long_press, 0, 0});
    }
    else if (was_pressed && !context->press_button.state)
    {
        // A long press already delivered its own action; releasing afterwards
        // must not also fire the short-press action.
        if (!context->press_long_fired)
            send(context, {event_button_up, 0, 0});
    }
}

static void poll_touch(task_context_t* context)
{
    if (context->config.touch == nullptr)
        return;
    drivers::gt911::state_t state = {};
    if (drivers::gt911::read(context->config.touch, &state) != ESP_OK || !state.ready)
    {
        return;
    }

    // GT911 already reports debounced touch state.  A release may be published only
    // once, so waiting for several identical samples adds latency and can lose the
    // release entirely.  Dispatch each fresh state transition immediately.
    const bool active = state.count > 0;
    if (active)
    {
        touch_coordinates(&context->config, state.points[0].x, state.points[0].y, &context->touch_x,
                          &context->touch_y);
    }
    if (active == context->touch_active)
    {
        return;
    }
    context->touch_active = active;
    send(context, {active ? event_touch_down : event_touch_up, context->touch_x, context->touch_y});
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
    if (config == nullptr || config->poll_interval_ms == 0 || events == nullptr)
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
    task_context.press_button.state = gpio_get_level(config->rotary_press_pin) == 0;
    task_context.press_button.candidate = task_context.press_button.state;
    task_context.press_button.stable_samples = debounce_samples;
    task_context.press_start_tick = xTaskGetTickCount();
    task_context.press_long_fired = false;
    task_context.right_button.state = gpio_get_level(config->rotary_right_pin) == 0;
    task_context.right_button.candidate = task_context.right_button.state;
    task_context.right_button.stable_samples = debounce_samples;
    task_context.left_button.state = gpio_get_level(config->rotary_left_pin) == 0;
    task_context.left_button.candidate = task_context.left_button.state;
    task_context.left_button.stable_samples = debounce_samples;
    task_context.last_clockwise_tick = xTaskGetTickCount();
    task_context.last_counterclockwise_tick = task_context.last_clockwise_tick;
    task_context.touch_active = false;
    task_context.touch_x = 0;
    task_context.touch_y = 0;

    if (xTaskCreate(task, "xreader_input", 4096, &task_context, 5, nullptr) != pdPASS)
    {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool enqueue_key(QueueHandle_t events, key_t key, bool pressed)
{
    if (events == nullptr || key == key_none)
        return false;
    const event_t event = {pressed ? event_key_down : event_key_up, 0, 0, key};
    return xQueueSend(events, &event, 0) == pdTRUE;
}

bool enqueue_key_repeat(QueueHandle_t events, key_t key)
{
    if (events == nullptr || key == key_none)
        return false;
    const event_t event = {event_key_repeat, 0, 0, key};
    return xQueueSend(events, &event, 0) == pdTRUE;
}

void flush(QueueHandle_t events)
{
    if (events == nullptr)
        return;
    xQueueReset(events);
    // Keep the physical touch state across queue flushes. Resetting it while a
    // finger is still down can synthesize a second touch_down on the next report.
    task_context.press_button.candidate = gpio_get_level(task_context.config.rotary_press_pin) == 0;
    task_context.press_button.state = task_context.press_button.candidate;
    task_context.press_button.stable_samples = debounce_samples;
    task_context.press_start_tick = xTaskGetTickCount();
    task_context.press_long_fired = false;
    task_context.right_button.candidate = gpio_get_level(task_context.config.rotary_right_pin) == 0;
    task_context.right_button.state = task_context.right_button.candidate;
    task_context.right_button.stable_samples = debounce_samples;
    task_context.left_button.candidate = gpio_get_level(task_context.config.rotary_left_pin) == 0;
    task_context.left_button.state = task_context.left_button.candidate;
    task_context.left_button.stable_samples = debounce_samples;
    task_context.last_clockwise_tick = xTaskGetTickCount();
    task_context.last_counterclockwise_tick = task_context.last_clockwise_tick;
}

} // namespace input
} // namespace xreader
