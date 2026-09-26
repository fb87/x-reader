#include "m5paper_board.hpp"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "m5paper_pins.hpp"

namespace xreader
{
namespace board
{
namespace m5paper
{

static const char* const tag = "m5paper_board";

esp_err_t power_on()
{
    const gpio_config_t power_config = {
        .pin_bit_mask =
            (1ULL << main_power_pin) | (1ULL << external_power_pin) | (1ULL << epd_power_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t error = gpio_config(&power_config);
    if (error != ESP_OK)
    {
        return error;
    }

    error = gpio_set_level(main_power_pin, 1);
    if (error != ESP_OK)
    {
        return error;
    }
    error = gpio_set_level(external_power_pin, 1);
    if (error != ESP_OK)
    {
        return error;
    }
    error = gpio_set_level(epd_power_pin, 1);
    if (error != ESP_OK)
    {
        return error;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(tag, "M5Paper power rails enabled");
    return ESP_OK;
}

esp_err_t power_off()
{
    esp_err_t error = gpio_set_level(epd_power_pin, 0);
    if (error == ESP_OK)
        error = gpio_set_level(external_power_pin, 0);
    if (error == ESP_OK)
        error = gpio_set_level(main_power_pin, 0);
    return error;
}

esp_err_t enter_deep_sleep(uint64_t wakeup_us)
{
    if (wakeup_us == 0)
        return ESP_ERR_INVALID_ARG;
    esp_err_t error = power_off();
    if (error != ESP_OK)
        return error;
    error = esp_sleep_enable_timer_wakeup(wakeup_us);
    if (error == ESP_OK)
        esp_deep_sleep_start();
    return error;
}

} // namespace m5paper
} // namespace board
} // namespace xreader
