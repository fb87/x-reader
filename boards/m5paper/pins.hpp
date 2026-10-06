#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

/**
 * @brief M5Paper pin assignment, ported verbatim from
 * `xreader::board::m5paper` (port/m5paper/board/m5paper/m5paper_pins.hpp).
 * UNVERIFIED IN THIS SANDBOX: no ESP-IDF toolchain is available here.
 */
namespace board::m5paper::pins {

inline constexpr spi_host_device_t epd_spi_host = SPI3_HOST;

inline constexpr gpio_num_t epd_sck_pin = GPIO_NUM_14;
inline constexpr gpio_num_t epd_mosi_pin = GPIO_NUM_12;
inline constexpr gpio_num_t epd_miso_pin = GPIO_NUM_13;
inline constexpr gpio_num_t epd_cs_pin = GPIO_NUM_15;
inline constexpr gpio_num_t epd_busy_pin = GPIO_NUM_27;
inline constexpr gpio_num_t sd_cs_pin = GPIO_NUM_4;

inline constexpr gpio_num_t main_power_pin = GPIO_NUM_2;
inline constexpr gpio_num_t external_power_pin = GPIO_NUM_5;
inline constexpr gpio_num_t epd_power_pin = GPIO_NUM_23;
inline constexpr gpio_num_t touch_sda_pin = GPIO_NUM_21;
inline constexpr gpio_num_t touch_scl_pin = GPIO_NUM_22;
inline constexpr gpio_num_t touch_int_pin = GPIO_NUM_36;
inline constexpr gpio_num_t rotary_right_pin = GPIO_NUM_39;
inline constexpr gpio_num_t rotary_press_pin = GPIO_NUM_38;
inline constexpr gpio_num_t rotary_left_pin = GPIO_NUM_37;
inline constexpr gpio_num_t battery_adc_pin = GPIO_NUM_35;

inline constexpr uint16_t panel_width = 960;   ///< native IT8951 panel width.
inline constexpr uint16_t panel_height = 540;  ///< native IT8951 panel height.
inline constexpr uint8_t panel_rotation = 1;   ///< old app_main.cpp always used rotation=1.

}  // namespace board::m5paper::pins
