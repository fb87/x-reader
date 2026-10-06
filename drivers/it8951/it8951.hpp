#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_task_wdt.h"

/**
 * @brief IT8951 e-ink controller driver, ported from
 * `xreader::drivers::it8951e` (port/m5paper/drivers/it8951e/it8951e.hpp).
 *
 * UNVERIFIED IN THIS SANDBOX: no ESP-IDF toolchain is available here, so
 * this header cannot be compiled or tested in this migration. Logic is
 * preserved exactly from the old driver, including the hardware-verified
 * 90-degree rotation coordinate remap and the chunked/DMA-safe pixel
 * upload (see it8951.cpp) -- this must be validated on real M5Paper
 * hardware before this driver is considered done, per docs/DESIGN.md s32.
 *
 * Driver, not board: knows the IT8951 protocol, not the ebook reader or
 * the M5Paper's specific pin assignment (that's boards/m5paper/board.hpp).
 */
namespace drivers::it8951 {

enum refresh_mode_t : uint16_t {
  refresh_init = 0,
  refresh_du = 1,
  refresh_gc16 = 2,
  refresh_gl16 = 3,
  refresh_glr16 = 4,
  refresh_gld16 = 5,
  refresh_du4 = 6,
  refresh_a2 = 7,
};

struct config_t {
  spi_host_device_t spi_host;
  gpio_num_t sck_pin;
  gpio_num_t mosi_pin;
  gpio_num_t miso_pin;
  gpio_num_t cs_pin;
  gpio_num_t busy_pin;
  uint16_t width;
  uint16_t height;
  uint8_t rotation;
  uint32_t spi_frequency_hz;
};

struct device_t {
  spi_device_handle_t spi;
  gpio_num_t cs_pin;
  gpio_num_t busy_pin;
  uint16_t width;
  uint16_t height;
  uint8_t rotation;
  bool inverted;
  uint16_t device_memory_low;
  uint16_t device_memory_high;
  esp_task_wdt_user_handle_t watchdog_user;
};

size_t framebuffer_size(uint16_t width, uint16_t height);
esp_err_t init(device_t* device, const config_t* config);
esp_err_t write_image_4bpp(device_t* device, const uint8_t* pixels, uint16_t x, uint16_t y,
                          uint16_t width, uint16_t height);
esp_err_t refresh(device_t* device, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                 refresh_mode_t mode);
void set_rotation(device_t* device, uint8_t rotation);
/** Optional user preference: send the inverse of every nibble so the panel shows
 * light text on a dark page. */
void set_inverted(device_t* device, bool inverted);
uint16_t logical_width(const device_t* device);
uint16_t logical_height(const device_t* device);

}  // namespace drivers::it8951
