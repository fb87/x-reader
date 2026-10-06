#include "board.hpp"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pins.hpp"

namespace board::m5paper {

namespace {
const char* const tag = "m5paper_board";
adc_oneshot_unit_handle_t battery_adc = nullptr;
constexpr adc_channel_t battery_channel = ADC_CHANNEL_7;  // GPIO35 / ADC1_CH7
}  // namespace

esp_err_t power_on() {
  const gpio_config_t power_config = {
      .pin_bit_mask = (1ULL << pins::main_power_pin) | (1ULL << pins::external_power_pin) |
                     (1ULL << pins::epd_power_pin),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  esp_err_t error = gpio_config(&power_config);
  if (error != ESP_OK) return error;

  error = gpio_set_level(pins::main_power_pin, 1);
  if (error != ESP_OK) return error;
  error = gpio_set_level(pins::external_power_pin, 1);
  if (error != ESP_OK) return error;
  error = gpio_set_level(pins::epd_power_pin, 1);
  if (error != ESP_OK) return error;

  // Keep startup below the ESP-IDF main-task watchdog window; the panel and
  // touch drivers perform their own settling delays during initialization.
  vTaskDelay(pdMS_TO_TICKS(100));
  ESP_LOGI(tag, "M5Paper power rails enabled");
  return ESP_OK;
}

esp_err_t power_off() {
  esp_err_t error = gpio_set_level(pins::epd_power_pin, 0);
  if (error == ESP_OK) error = gpio_set_level(pins::external_power_pin, 0);
  if (error == ESP_OK) error = gpio_set_level(pins::main_power_pin, 0);
  return error;
}

esp_err_t enter_deep_sleep(uint64_t wakeup_us) {
  // GPIO38 is the M5Paper centre/power key and is RTC-capable on the ESP32.
  // Keep a timer fallback as well so a device with a sticky key can recover.
  esp_err_t error = esp_sleep_enable_ext0_wakeup(pins::rotary_press_pin, 0);
  if (error != ESP_OK) return error;
  if (wakeup_us != 0) {
    error = esp_sleep_enable_timer_wakeup(wakeup_us);
    if (error != ESP_OK) return error;
  }
  error = power_off();
  if (error == ESP_OK) esp_deep_sleep_start();
  return error;
}

esp_err_t battery_init() {
  if (battery_adc != nullptr) return ESP_OK;
  const adc_oneshot_unit_init_cfg_t unit = {
      .unit_id = ADC_UNIT_1,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  esp_err_t error = adc_oneshot_new_unit(&unit, &battery_adc);
  if (error != ESP_OK) return error;
  const adc_oneshot_chan_cfg_t channel = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_12,
  };
  error = adc_oneshot_config_channel(battery_adc, battery_channel, &channel);
  if (error != ESP_OK) {
    adc_oneshot_del_unit(battery_adc);
    battery_adc = nullptr;
  }
  return error;
}

esp_err_t battery_voltage_mv(uint16_t* millivolts) {
  if (millivolts == nullptr) return ESP_ERR_INVALID_ARG;
  esp_err_t error = battery_init();
  if (error != ESP_OK) return error;

  uint32_t sum = 0;
  constexpr uint8_t sample_count = 8;
  for (uint8_t sample = 0; sample < sample_count; ++sample) {
    int raw = 0;
    error = adc_oneshot_read(battery_adc, battery_channel, &raw);
    if (error != ESP_OK) return error;
    sum += static_cast<uint32_t>(raw);
  }
  const uint32_t average = sum / sample_count;

  // M5Paper's reference implementation uses a 2:1 battery divider and a
  // nominal 3.6 V ADC full-scale calibration. Keep the same nominal
  // conversion here without pulling a large calibration layer into startup.
  const uint32_t mv = (average * 7200U + 2047U) / 4095U;
  *millivolts = static_cast<uint16_t>(mv > 65535U ? 65535U : mv);
  return ESP_OK;
}

uint8_t battery_percent(uint16_t millivolts) {
  struct point_t {
    uint16_t mv;
    uint8_t percent;
  };
  constexpr point_t curve[] = {
      {3300, 0},  {3400, 2},  {3500, 5},  {3600, 10}, {3700, 25},
      {3800, 45}, {3900, 65}, {4000, 80}, {4100, 90}, {4200, 100},
  };
  if (millivolts <= curve[0].mv) return curve[0].percent;
  for (size_t index = 1; index < sizeof(curve) / sizeof(curve[0]); ++index) {
    if (millivolts <= curve[index].mv) {
      const uint16_t span = static_cast<uint16_t>(curve[index].mv - curve[index - 1].mv);
      const uint16_t offset = static_cast<uint16_t>(millivolts - curve[index - 1].mv);
      const uint8_t delta = static_cast<uint8_t>(curve[index].percent - curve[index - 1].percent);
      return static_cast<uint8_t>(curve[index - 1].percent +
                                  (static_cast<uint32_t>(offset) * delta) / span);
    }
  }
  return 100;
}

}  // namespace board::m5paper
