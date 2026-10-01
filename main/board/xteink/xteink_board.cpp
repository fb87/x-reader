#include "xteink_board.hpp"

#include "esp_adc/adc_oneshot.h"
#include "esp_sleep.h"

#include "xteink_pins.hpp"

namespace xreader
{
namespace board
{
namespace xteink
{

namespace
{
static adc_oneshot_unit_handle_t shared_adc1 = nullptr;
static bool battery_channel_configured = false;
static constexpr adc_channel_t battery_channel = ADC_CHANNEL_0; // GPIO0 on ESP32-C3

esp_err_t battery_adc_init()
{
    adc_oneshot_unit_handle_t adc = nullptr;
    esp_err_t error = acquire_adc1(&adc);
    if (error != ESP_OK)
        return error;
    if (battery_channel_configured)
        return ESP_OK;
    const adc_oneshot_chan_cfg_t channel = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    error = adc_oneshot_config_channel(adc, battery_channel, &channel);
    if (error == ESP_OK)
        battery_channel_configured = true;
    return error;
}
} // namespace

esp_err_t acquire_adc1(adc_oneshot_unit_handle_t* handle)
{
    if (handle == nullptr)
        return ESP_ERR_INVALID_ARG;
    if (shared_adc1 == nullptr)
    {
        const adc_oneshot_unit_init_cfg_t unit = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        const esp_err_t error = adc_oneshot_new_unit(&unit, &shared_adc1);
        if (error != ESP_OK)
            return error;
    }
    *handle = shared_adc1;
    return ESP_OK;
}

esp_err_t battery_voltage_mv(uint16_t* millivolts)
{
    if (millivolts == nullptr)
        return ESP_ERR_INVALID_ARG;
    esp_err_t error = battery_adc_init();
    if (error != ESP_OK)
        return error;

    uint32_t sum = 0;
    static constexpr uint8_t sample_count = 8;
    for (uint8_t sample = 0; sample < sample_count; ++sample)
    {
        int raw = 0;
        error = adc_oneshot_read(shared_adc1, battery_channel, &raw);
        if (error != ESP_OK)
            return error;
        sum += static_cast<uint32_t>(raw);
    }
    const uint32_t average = sum / sample_count;
    // Placeholder 2:1-divider, 3.6V-fullscale conversion matching M5Paper's
    // until this board's actual divider ratio is measured on hardware.
    const uint32_t mv = (average * 7200U + 2047U) / 4095U;
    *millivolts = static_cast<uint16_t>(mv > 65535U ? 65535U : mv);
    return ESP_OK;
}

uint8_t battery_percent(uint16_t millivolts)
{
    struct point_t
    {
        uint16_t mv;
        uint8_t percent;
    };
    static constexpr point_t curve[] = {
        {3300, 0},  {3400, 2},  {3500, 5},  {3600, 10}, {3700, 25},
        {3800, 45}, {3900, 65}, {4000, 80}, {4100, 90}, {4200, 100},
    };
    if (millivolts <= curve[0].mv)
        return curve[0].percent;
    for (size_t index = 1; index < sizeof(curve) / sizeof(curve[0]); ++index)
    {
        if (millivolts <= curve[index].mv)
        {
            const uint16_t span = static_cast<uint16_t>(curve[index].mv - curve[index - 1].mv);
            const uint16_t offset = static_cast<uint16_t>(millivolts - curve[index - 1].mv);
            const uint8_t delta =
                static_cast<uint8_t>(curve[index].percent - curve[index - 1].percent);
            return static_cast<uint8_t>(curve[index - 1].percent +
                                        (static_cast<uint32_t>(offset) * delta) / span);
        }
    }
    return 100;
}

esp_err_t enter_deep_sleep(uint64_t wakeup_us)
{
    if (wakeup_us != 0)
    {
        const esp_err_t error = esp_sleep_enable_timer_wakeup(wakeup_us);
        if (error != ESP_OK)
            return error;
    }
    // The power button is active-low (see xteink_buttons.cpp's pull-up'd
    // read) and is the only physical control that should wake this board --
    // without this, deep sleep is a dead end until the USB cable is
    // physically unplugged and replugged (the native USB-Serial-JTAG
    // peripheral powers down too, so even the serial console disappears).
    // This file is compiled (for type-checking) on both board targets, but
    // esp_deep_sleep_enable_gpio_wakeup() only exists on chips with
    // SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP (esp32c3 among them; plain esp32
    // does not have it) -- harmless to skip on M5Paper, which never calls
    // this XTeink-only code path anyway.
#if SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
    const esp_err_t gpio_error =
        esp_deep_sleep_enable_gpio_wakeup(1ULL << power_button_pin, ESP_GPIO_WAKEUP_GPIO_LOW);
    if (gpio_error != ESP_OK)
        return gpio_error;
#endif
    esp_deep_sleep_start();
    return ESP_OK;
}

esp_err_t get_capabilities(capabilities_t* capabilities)
{
    if (capabilities == nullptr)
        return ESP_ERR_INVALID_ARG;
    *capabilities = {
        .configured = true,
        .display_width = display_width,
        .display_height = display_height,
        .display_controller = display_controller_ssd1677,
        .storage_bus = storage_bus_spi,
        .has_touch = false,
        .has_rotary = false,
        .has_sd = true,
        .supports_power_control = true,
        .supports_deep_sleep = true,
    };
    return ESP_OK;
}

} // namespace xteink
} // namespace board
} // namespace xreader
