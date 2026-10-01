// Implements the drivers::it8951e:: API (declared in it8951e.hpp) backed by
// the SSD1677 driver, compiled only for XREADER_BOARD_XTEINK. This lets
// main.cpp's ~70 display call sites (all written against it8951e's 4bpp
// windowed-refresh API) work unchanged on a board with a completely
// different, full-screen-only, 1bpp panel protocol.
//
// The it8951e::device_t the caller holds is a stand-in -- there is only ever
// one physical panel, so the real ssd1677 state and a persistent 1bpp shadow
// framebuffer both live in a single static instance here. write_image_4bpp
// threshold-converts its (possibly partial) rectangle into that shadow
// buffer; refresh() always uploads the whole shadow buffer, reconstructing
// full-panel state from a sequence of partial writes the same way a real
// partial-update panel would show it, just without the speed benefit (see
// ssd1677.hpp's refresh_mode_t comment -- partial windowed refresh can be
// added later the same way it8951e.cpp does it).
#include "it8951e.hpp"

#include <string.h>

#include "board/xteink/xteink_pins.hpp"
#include "drivers/ssd1677/ssd1677.hpp"
#include "esp_heap_caps.h"

namespace xreader
{
namespace drivers
{
namespace it8951e
{

namespace
{
static ssd1677::device_t real_device = {};
static uint8_t* shadow = nullptr; // 1bpp, MSB-first, full panel, persists across calls
static size_t shadow_size = 0;
static bool inverted = false;

// Same 4bpp packing gfx::framebuffer_t and gfx::set_pixel use: two pixels per
// byte, high nibble first (even x), 0x0 = ink/black .. 0xF = paper/white.
static uint8_t nibble_at(const uint8_t* pixels, uint16_t row_width, uint16_t x, uint16_t y)
{
    const size_t row_bytes = (static_cast<size_t>(row_width) + 1U) / 2U;
    const uint8_t byte = pixels[static_cast<size_t>(y) * row_bytes + x / 2U];
    return (x & 1U) == 0U ? static_cast<uint8_t>(byte >> 4) : static_cast<uint8_t>(byte & 0x0FU);
}

// UI code throughout main/ui/*.cpp draws subtle focus/selection highlights as
// a near-white shade (e.g. 0x0d vs background 0x0f) -- a real, visible
// distinction on M5Paper's 16-level grayscale panel, but a flat nibble>=8
// threshold collapses every light gray to the same solid white on this
// 1-bit panel, making those highlights invisible. A 4x4 ordered (Bayer)
// dither renders intermediate grays as a visible stipple instead, without
// touching any of the ~20 screens that draw these highlights. Pure black
// (0x0) and pure white (0xF) -- the overwhelming majority of actual pixels:
// text, rules, borders -- are left exactly as a flat threshold would render
// them (no stray dots in solid regions).
static const uint8_t bayer_4x4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

// `selected ? 0x0d : 0x0f`-style selected-row backgrounds (home.cpp,
// library.cpp, bookmarks.cpp, book_manager.cpp, and most other list screens)
// are *just barely* off pure white by design -- a subtle highlight on real
// 16-level grayscale, but faithfully dithering that small a difference
// produces only a ~12% dot density, which reads as "still basically white"
// at a glance. Boost nibbles in that near-white band to a bolder, clearly-
// visible density; true mid-tones (already dense enough once dithered) and
// the two true extremes (pure black/white, the overwhelming majority of
// actual pixels: text, rules, borders) are left untouched.
static uint8_t boost_near_white_for_dither(uint8_t nibble)
{
    if (nibble >= 10U && nibble < 15U)
        return static_cast<uint8_t>(nibble - 6U); // 10..14 -> 4..8
    return nibble;
}

static bool dithered_white(uint8_t nibble, uint16_t x, uint16_t y)
{
    const uint8_t boosted = boost_near_white_for_dither(nibble);
    if (boosted >= 15U)
        return true;
    if (boosted == 0U)
        return false;
    // Scale 0-15 nibble range to the 0-15 Bayer threshold range (1:1) and
    // compare against this pixel's position in the 4x4 tile.
    return boosted > bayer_4x4[y % 4U][x % 4U];
}

static void set_shadow_bit(uint16_t panel_width, uint16_t x, uint16_t y, bool white)
{
    const size_t row_bytes = (static_cast<size_t>(panel_width) + 7U) / 8U;
    const size_t index = static_cast<size_t>(y) * row_bytes + x / 8U;
    const uint8_t mask = static_cast<uint8_t>(0x80U >> (x % 8U));
    if (white)
        shadow[index] = static_cast<uint8_t>(shadow[index] | mask);
    else
        shadow[index] = static_cast<uint8_t>(shadow[index] & ~mask);
}
} // namespace

size_t framebuffer_size(uint16_t width, uint16_t height)
{
    return (static_cast<size_t>(width) * height + 1U) / 2U;
}

esp_err_t init(device_t* device, const config_t* config)
{
    if (device == nullptr || config == nullptr)
        return ESP_ERR_INVALID_ARG;

    const ssd1677::config_t ssd_config = {
        .spi_host = config->spi_host,
        .sck_pin = config->sck_pin,
        .mosi_pin = config->mosi_pin,
        .miso_pin = config->miso_pin,
        .cs_pin = config->cs_pin,
        .dc_pin = board::xteink::epd_dc_pin,
        .reset_pin = board::xteink::epd_reset_pin,
        .busy_pin = config->busy_pin,
        .width = config->width,
        .height = config->height,
        .spi_frequency_hz = config->spi_frequency_hz,
    };
    esp_err_t error = ssd1677::init(&real_device, &ssd_config);
    if (error != ESP_OK)
        return error;

    // No PSRAM on this board -- internal RAM only. At 48000 bytes for the
    // full 800x480 1bpp panel this fits comfortably alongside the (also
    // internal-RAM-only, see framebuffer.cpp) 4bpp UI framebuffer.
    shadow_size = ssd1677::framebuffer_size(config->width, config->height);
    shadow = static_cast<uint8_t*>(heap_caps_malloc(shadow_size, MALLOC_CAP_8BIT));
    if (shadow == nullptr)
        return ESP_ERR_NO_MEM;
    memset(shadow, 0xFF, shadow_size); // all white

    device->width = config->width;
    device->height = config->height;
    device->rotation = config->rotation;
    device->inverted = false;
    return ESP_OK;
}

esp_err_t write_image_4bpp(device_t* device, const uint8_t* pixels, uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height)
{
    if (device == nullptr || pixels == nullptr || shadow == nullptr)
        return ESP_ERR_INVALID_ARG;
    for (uint16_t row = 0; row < height; ++row)
    {
        for (uint16_t column = 0; column < width; ++column)
        {
            const uint8_t nibble = nibble_at(pixels, width, column, row);
            const uint16_t pixel_x = static_cast<uint16_t>(x + column);
            const uint16_t pixel_y = static_cast<uint16_t>(y + row);
            const uint8_t effective_nibble =
                inverted ? static_cast<uint8_t>(15U - nibble) : nibble;
            const bool white = dithered_white(effective_nibble, pixel_x, pixel_y);
            uint16_t native_x = pixel_x;
            uint16_t native_y = pixel_y;
            if (device->rotation != 0)
            {
                // Panel is wired native-landscape (800x480); the UI renders
                // into a logical portrait (480x800) framebuffer when rotated
                // (see logical_width/height below). A plain transpose
                // (native_x = logical_y, native_y = logical_x) is a mirror
                // image, not a rotation -- confirmed on real hardware (came
                // up flipped left-right). Flipping native_y (fed by
                // logical_x, the portrait image's horizontal axis) turns it
                // into a proper 90-degree rotation.
                native_x = static_cast<uint16_t>(y + row);
                native_y = static_cast<uint16_t>(device->height - 1U - (x + column));
            }
            set_shadow_bit(device->width, native_x, native_y, white);
        }
    }
    return ESP_OK;
}

esp_err_t refresh(device_t* device, uint16_t /*x*/, uint16_t /*y*/, uint16_t /*width*/,
                  uint16_t /*height*/, refresh_mode_t mode)
{
    if (device == nullptr || shadow == nullptr)
        return ESP_ERR_INVALID_ARG;
    // main.cpp defaults almost every screen-to-screen navigation redraw to
    // gc16 (its "full quality" mode), which on this panel's full waveform
    // means a visible black/white flash every time -- fine occasionally, but
    // the whole UI flashing on every menu press reads as broken. Fast mode's
    // RED-RAM diff is always safe to use here (init() deterministically
    // writes RED RAM to white at startup, never left as undefined garbage),
    // so gc16 maps to fast most of the time, with a true full refresh only
    // periodically to actually clear any accumulated ghosting. Everything
    // else (du/du4/a2 -- menu highlights/page turns) stays fast: unlike real
    // IT8951 hardware, this panel only renders 1bpp, so du4's real 4-level
    // grayscale LUT would buy nothing here -- highlight contrast on this
    // panel comes from the dithering done above in write_image_4bpp(), not
    // from the controller's refresh mode, so there's no reason to pay half's
    // always-re-power-the-rails cost for it.
    static constexpr uint16_t full_refresh_interval = 15;
    static uint16_t gc16_count = 0;
    ssd1677::refresh_mode_t ssd_mode;
    if (mode == refresh_gc16)
    {
        ++gc16_count;
        ssd_mode = (gc16_count % full_refresh_interval == 1) ? ssd1677::refresh_full
                                                             : ssd1677::refresh_fast;
    }
    else
    {
        ssd_mode = ssd1677::refresh_fast;
    }
    esp_err_t error = ssd1677::write_image_1bpp(&real_device, shadow, ssd_mode);
    if (error != ESP_OK)
        return error;
    return ssd1677::refresh(&real_device, shadow, ssd_mode);
}

void set_rotation(device_t* device, uint8_t rotation)
{
    if (device != nullptr)
        device->rotation = static_cast<uint8_t>(rotation & 1U);
}

void set_inverted(device_t* device, bool value)
{
    inverted = value;
    if (device != nullptr)
        device->inverted = value;
}

uint16_t logical_width(const device_t* device)
{
    if (device == nullptr)
        return 0;
    return device->rotation == 0 ? device->width : device->height;
}

uint16_t logical_height(const device_t* device)
{
    if (device == nullptr)
        return 0;
    return device->rotation == 0 ? device->height : device->width;
}

} // namespace it8951e
} // namespace drivers
} // namespace xreader
