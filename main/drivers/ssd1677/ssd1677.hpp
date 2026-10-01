#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

namespace xreader
{
namespace drivers
{
namespace ssd1677
{

// Three refresh qualities, ported from the open-x4-epaper community-sdk's
// EInkDisplay driver (same SSD1677 + GDEQ0426T82 panel): full (best quality,
// always re-powers the analog rails), half (balanced quality/speed, also
// always re-powers), and fast (lowest latency -- only re-powers if the
// screen was off, and diffs against RED RAM's previous frame instead of a
// fresh LUT application). Always the full panel area -- no partial-window
// RAM addressing yet, see write_image_1bpp()'s comment.
enum refresh_mode_t : uint8_t
{
    refresh_full,
    refresh_half,
    refresh_fast,
};

struct config_t
{
    spi_host_device_t spi_host;
    gpio_num_t sck_pin;
    gpio_num_t mosi_pin;
    // Not used by this (write-only) display device itself, but this SPI bus
    // is shared with the SD card, which does need MISO -- spi_bus_config_t's
    // pin assignments are bus-wide, so leaving this unrouted here would deafen
    // every other device on the bus, not just this one.
    gpio_num_t miso_pin;
    gpio_num_t cs_pin;
    gpio_num_t dc_pin;
    gpio_num_t reset_pin;
    gpio_num_t busy_pin;
    uint16_t width;
    uint16_t height;
    uint32_t spi_frequency_hz;
};

struct device_t
{
    spi_device_handle_t spi;
    gpio_num_t cs_pin;
    gpio_num_t dc_pin;
    gpio_num_t reset_pin;
    gpio_num_t busy_pin;
    uint16_t width;
    uint16_t height;
    // Tracks whether the panel's analog rails/clock are currently powered, so
    // back-to-back fast refreshes can skip re-powering them -- the actual
    // latency win fast mode exists for. Sticky across calls; deep_sleep()
    // powers everything down and does not currently clear this (the panel is
    // reset on wake, see board bring-up), matching the reference driver.
    bool screen_on;
};

size_t framebuffer_size(uint16_t width, uint16_t height);
esp_err_t init(device_t* device, const config_t* config);
// pixels: 1 bit per pixel, MSB first, packed rows, width*height/8 bytes.
// 1 = white, 0 = black (SSD1677's native BW RAM polarity). Always the full
// panel area -- see the refresh_mode_t comment above. Writes BW RAM with the
// new frame always; writes RED RAM too unless mode is refresh_fast, in which
// case RED is left holding whatever refresh() last synced it to (the
// previously *displayed* frame), which is what fast mode diffs against.
esp_err_t write_image_1bpp(device_t* device, const uint8_t* pixels, refresh_mode_t mode);
// Triggers the mode-appropriate update sequence, then re-syncs RED RAM to
// `pixels` (the frame just displayed) so a subsequent fast refresh has the
// correct previous-frame reference to diff against.
esp_err_t refresh(device_t* device, const uint8_t* pixels, refresh_mode_t mode);
void deep_sleep(device_t* device);

} // namespace ssd1677
} // namespace drivers
} // namespace xreader
