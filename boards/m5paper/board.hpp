#pragma once

#include "esp_err.h"

/**
 * @brief M5Paper power/battery/sleep control, ported verbatim from
 * the original `xreader::board::m5paper` implementation.
 * UNVERIFIED IN THIS SANDBOX: no ESP-IDF toolchain is available here.
 */
namespace board::m5paper {

esp_err_t power_on();
esp_err_t power_off();
esp_err_t enter_deep_sleep(uint64_t wakeup_us);
esp_err_t battery_init();
esp_err_t battery_voltage_mv(uint16_t* millivolts);
uint8_t battery_percent(uint16_t millivolts);

}  // namespace board::m5paper
