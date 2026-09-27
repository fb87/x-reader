#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace gfx
{

struct framebuffer_t
{
    uint8_t* pixels;
    uint8_t* front_pixels;
    uint16_t width;
    uint16_t height;
    uint16_t dirty_left;
    uint16_t dirty_top;
    uint16_t dirty_right;
    uint16_t dirty_bottom;
};

size_t size(uint16_t width, uint16_t height);
esp_err_t create(framebuffer_t* framebuffer, uint16_t width, uint16_t height);
void destroy(framebuffer_t* framebuffer);
esp_err_t present(framebuffer_t* framebuffer);
void clear(framebuffer_t* framebuffer, uint8_t value);
void set_pixel(framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint8_t value);
void fill_rect(framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
               uint8_t value);
void draw_rect(framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
               uint8_t value);
bool take_dirty(framebuffer_t* framebuffer, uint16_t* x, uint16_t* y, uint16_t* width,
                uint16_t* height);
esp_err_t copy_region_4bpp(const framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width,
                           uint16_t height, uint8_t* output, size_t output_size);

} // namespace gfx
} // namespace xreader
