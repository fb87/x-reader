#pragma once

#include "esp_adc/adc_oneshot.h"

#include "board/board.hpp"

#ifndef XREADER_XTEINK_CONFIGURED
#define XREADER_XTEINK_CONFIGURED 0
#endif

namespace xreader
{
namespace board
{
namespace xteink
{

esp_err_t get_capabilities(capabilities_t* capabilities);

// ESP32-C3 has exactly one ADC1 unit, and adc_oneshot_new_unit() fails with
// "already in use" if called twice for it -- battery reading and the button
// ladder decoder both need ADC1, so they share this one lazily-created
// handle instead of each creating their own.
esp_err_t acquire_adc1(adc_oneshot_unit_handle_t* handle);

esp_err_t battery_voltage_mv(uint16_t* millivolts);
uint8_t battery_percent(uint16_t millivolts);
// XTeink X4 has no dedicated power-rail GPIOs to sequence (unlike M5Paper);
// the panel and SD card are always powered when the chip is. Timer wake only
// for this first pass -- ESP32-C3 GPIO deep-sleep wake needs API that hasn't
// been verified against this project's pinned IDF revision yet.
esp_err_t enter_deep_sleep(uint64_t wakeup_us);

} // namespace xteink
} // namespace board
} // namespace xreader
