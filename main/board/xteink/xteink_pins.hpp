#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"

namespace xreader
{
namespace board
{
namespace xteink
{

static constexpr uint16_t display_width = 800;
static constexpr uint16_t display_height = 480;

static constexpr spi_host_device_t spi_host = SPI2_HOST;
static constexpr gpio_num_t spi_sck_pin = GPIO_NUM_8;
static constexpr gpio_num_t spi_mosi_pin = GPIO_NUM_10;
static constexpr gpio_num_t spi_miso_pin = GPIO_NUM_7;

static constexpr gpio_num_t epd_cs_pin = GPIO_NUM_21;
static constexpr gpio_num_t epd_dc_pin = GPIO_NUM_4;
static constexpr gpio_num_t epd_reset_pin = GPIO_NUM_5;
static constexpr gpio_num_t epd_busy_pin = GPIO_NUM_6;
static constexpr gpio_num_t sd_cs_pin = GPIO_NUM_12;

static constexpr gpio_num_t power_button_pin = GPIO_NUM_3;
static constexpr gpio_num_t battery_adc_pin = GPIO_NUM_0;
static constexpr gpio_num_t usb_detect_pin = GPIO_NUM_20;

static constexpr adc_unit_t button_adc_unit = ADC_UNIT_1;
static constexpr adc_channel_t button_ladder1_channel = ADC_CHANNEL_1; // GPIO1
static constexpr adc_channel_t button_ladder2_channel = ADC_CHANNEL_2; // GPIO2

} // namespace xteink
} // namespace board
} // namespace xreader
