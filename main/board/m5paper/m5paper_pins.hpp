#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

namespace xreader
{
namespace board
{
namespace m5paper
{

static constexpr spi_host_device_t epd_spi_host = SPI3_HOST;

static constexpr gpio_num_t epd_sck_pin = GPIO_NUM_14;
static constexpr gpio_num_t epd_mosi_pin = GPIO_NUM_12;
static constexpr gpio_num_t epd_miso_pin = GPIO_NUM_13;
static constexpr gpio_num_t epd_cs_pin = GPIO_NUM_15;
static constexpr gpio_num_t epd_busy_pin = GPIO_NUM_27;
static constexpr gpio_num_t sd_cs_pin = GPIO_NUM_4;

static constexpr gpio_num_t main_power_pin = GPIO_NUM_2;
static constexpr gpio_num_t external_power_pin = GPIO_NUM_5;
static constexpr gpio_num_t epd_power_pin = GPIO_NUM_23;
static constexpr gpio_num_t touch_sda_pin = GPIO_NUM_21;
static constexpr gpio_num_t touch_scl_pin = GPIO_NUM_22;
static constexpr gpio_num_t touch_int_pin = GPIO_NUM_36;
static constexpr gpio_num_t rotary_right_pin = GPIO_NUM_39;
static constexpr gpio_num_t rotary_press_pin = GPIO_NUM_38;
static constexpr gpio_num_t rotary_left_pin = GPIO_NUM_37;
static constexpr gpio_num_t battery_adc_pin = GPIO_NUM_35;

static constexpr uint16_t display_width = 960;
static constexpr uint16_t display_height = 540;
static constexpr uint8_t display_rotation = 0;

} // namespace m5paper
} // namespace board
} // namespace xreader
