#include "xteink_buttons.hpp"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/task.h"

#include "xteink_board.hpp"
#include "xteink_button_decode.hpp"
#include "xteink_pins.hpp"

namespace xreader
{
namespace board
{
namespace xteink
{

namespace
{
static const char* const tag = "xteink_buttons";

struct task_context_t
{
    QueueHandle_t events = nullptr;
    button_config_t config = {};
    adc_oneshot_unit_handle_t adc = nullptr;
    input::key_t candidate = input::key_none;
    input::key_t active = input::key_none;
    uint8_t stable = 0;
    bool power_candidate = false;
    bool power_active = false;
    uint8_t power_stable = 0;
    TickType_t power_press_tick = 0;
    TickType_t navigation_press_tick = 0;
    TickType_t navigation_repeat_tick = 0;
};

static task_context_t task_context = {};

static input::key_t read_navigation_key(task_context_t* context)
{
    int ladder1 = 4095;
    int ladder2 = 4095;
    if (adc_oneshot_read(context->adc, button_ladder1_channel, &ladder1) != ESP_OK)
        ladder1 = 4095;
    if (adc_oneshot_read(context->adc, button_ladder2_channel, &ladder2) != ESP_OK)
        ladder2 = 4095;

    // Only one key is expected at a time. Prefer the two-key ladder first because
    // its pressed values are very far apart and therefore least ambiguous.
    const input::key_t ladder2_key = decode_ladder2(ladder2);
    if (ladder2_key != input::key_none)
        return ladder2_key;
    return decode_ladder1(ladder1);
}

static void update_navigation(task_context_t* context, input::key_t key)
{
    if (key != context->candidate)
    {
        context->candidate = key;
        context->stable = 1;
        return;
    }
    if (context->stable < context->config.stable_samples)
        ++context->stable;
    if (context->stable < context->config.stable_samples || key == context->active)
        return;

    if (context->active != input::key_none)
        input::enqueue_key(context->events, context->active, false);
    context->active = key;
    if (context->active != input::key_none)
    {
        input::enqueue_key(context->events, context->active, true);
        context->navigation_press_tick = xTaskGetTickCount();
        context->navigation_repeat_tick = context->navigation_press_tick;
    }
}

static bool repeatable(input::key_t key)
{
    return key == input::key_up || key == input::key_down || key == input::key_left ||
           key == input::key_right || key == input::key_page_next || key == input::key_page_prev;
}

static void update_repeat(task_context_t* context)
{
    if (!repeatable(context->active) || context->config.repeat_interval_ms == 0U)
        return;
    const TickType_t now = xTaskGetTickCount();
    const uint32_t held_ms =
        static_cast<uint32_t>((now - context->navigation_press_tick) * portTICK_PERIOD_MS);
    const uint32_t since_repeat_ms =
        static_cast<uint32_t>((now - context->navigation_repeat_tick) * portTICK_PERIOD_MS);
    if (held_ms < context->config.repeat_delay_ms ||
        since_repeat_ms < context->config.repeat_interval_ms)
        return;
    input::enqueue_key_repeat(context->events, context->active);
    context->navigation_repeat_tick = now;
}

static void update_power(task_context_t* context)
{
    const bool pressed = gpio_get_level(power_button_pin) == 0;
    if (pressed != context->power_candidate)
    {
        context->power_candidate = pressed;
        context->power_stable = 1;
        return;
    }
    if (context->power_stable < context->config.stable_samples)
        ++context->power_stable;
    if (context->power_stable < context->config.stable_samples || pressed == context->power_active)
        return;

    context->power_active = pressed;
    if (pressed)
    {
        context->power_press_tick = xTaskGetTickCount();
        input::enqueue_key(context->events, input::key_power, true);
        return;
    }

    const uint32_t held_ms = static_cast<uint32_t>(
        (xTaskGetTickCount() - context->power_press_tick) * portTICK_PERIOD_MS);
    if (held_ms >= context->config.power_long_press_ms)
    {
        input::enqueue_key(context->events, input::key_power, false);
    }
}

static void task(void* argument)
{
    task_context_t* context = static_cast<task_context_t*>(argument);
    uint32_t diagnostic_tick = 0;
    while (true)
    {
        update_navigation(context, read_navigation_key(context));
        update_repeat(context);
        update_power(context);
        // TEMPORARY: raw ADC trace to diagnose real buttons producing no
        // events after a soft reset. Remove once resolved.
        if (++diagnostic_tick % 25U == 0U)
        {
            int ladder1 = 4095;
            int ladder2 = 4095;
            const esp_err_t error1 =
                adc_oneshot_read(context->adc, button_ladder1_channel, &ladder1);
            const esp_err_t error2 =
                adc_oneshot_read(context->adc, button_ladder2_channel, &ladder2);
            ESP_LOGI(tag, "adc ladder1=%d(%s) ladder2=%d(%s)", ladder1, esp_err_to_name(error1),
                     ladder2, esp_err_to_name(error2));
        }
        vTaskDelay(pdMS_TO_TICKS(context->config.poll_interval_ms));
    }
}
} // namespace

input::key_t decode_ladder1(int raw)
{
    return decode_ladder1_value(raw);
}

input::key_t decode_ladder2(int raw)
{
    return decode_ladder2_value(raw);
}

esp_err_t start_buttons(QueueHandle_t events, const button_config_t* config)
{
    if (events == nullptr)
        return ESP_ERR_INVALID_ARG;

    const button_config_t selected = config == nullptr ? button_config_t{} : *config;
    if (selected.poll_interval_ms == 0 || selected.stable_samples == 0)
        return ESP_ERR_INVALID_ARG;

    // KNOWN HARDWARE LIMITATION, confirmed on real hardware: after a soft
    // (EN-pin) reset -- which is how every `idf.py flash` and every
    // esptool "hard-reset" ends -- these two ADC channels can read a flat
    // max value regardless of button state, requiring a full power-off/on
    // to recover. Reconfirmed with adc_oneshot_read() itself reporting
    // ESP_OK throughout (not a software error being swallowed), and with an
    // explicit gpio_reset_pin() on both channels' pins before ADC channel
    // config (ruled out as a fix -- same symptom persisted). This points to
    // ADC1's own analog bias/reference state surviving an EN-pin reset in a
    // way a true power cycle clears, not anything this driver code controls.
    // No software workaround found yet; if hit again, the user needs a full
    // power cycle (unplug/replug USB), not just a reset.

    // ESP32-C3 only has one ADC1 unit; board::xteink::battery_voltage_mv()
    // needs it too, so both share the one lazily-created handle instead of
    // each calling adc_oneshot_new_unit() (which fails the second time).
    esp_err_t error = board::xteink::acquire_adc1(&task_context.adc);
    if (error != ESP_OK)
        return error;

    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    error = adc_oneshot_config_channel(task_context.adc, button_ladder1_channel, &channel_config);
    if (error == ESP_OK)
        error =
            adc_oneshot_config_channel(task_context.adc, button_ladder2_channel, &channel_config);
    if (error != ESP_OK)
    {
        task_context.adc = nullptr;
        return error;
    }

    const gpio_config_t power_config = {
        .pin_bit_mask = 1ULL << power_button_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    error = gpio_config(&power_config);
    if (error != ESP_OK)
    {
        task_context.adc = nullptr;
        return error;
    }

    task_context.events = events;
    task_context.config = selected;
    task_context.candidate = input::key_none;
    task_context.active = input::key_none;
    task_context.stable = 0;
    task_context.power_candidate = gpio_get_level(power_button_pin) == 0;
    task_context.power_active = task_context.power_candidate;
    task_context.power_stable = selected.stable_samples;
    task_context.power_press_tick = xTaskGetTickCount();
    task_context.navigation_press_tick = task_context.power_press_tick;
    task_context.navigation_repeat_tick = task_context.power_press_tick;

    if (xTaskCreate(task, "xteink_keys", 3072, &task_context, 5, nullptr) != pdPASS)
    {
        // Don't delete task_context.adc here -- it's the shared ADC1 handle
        // (see acquire_adc1()), which board::xteink::battery_voltage_mv() may
        // also be holding onto.
        task_context.adc = nullptr;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(tag, "XTeink X4 keys started (%lums poll, %u samples)",
             static_cast<unsigned long>(selected.poll_interval_ms),
             static_cast<unsigned>(selected.stable_samples));
    return ESP_OK;
}

} // namespace xteink
} // namespace board
} // namespace xreader
