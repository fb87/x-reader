#include "framebuffer.hpp"

#include <string.h>

#include "esp_heap_caps.h"

namespace xreader
{
namespace gfx
{

size_t size(uint16_t width, uint16_t height)
{
    return (static_cast<size_t>(width) * height + 1) / 2;
}

esp_err_t create(framebuffer_t* framebuffer, uint16_t width, uint16_t height)
{
    if (framebuffer == nullptr || width == 0 || height == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    framebuffer->pixels = static_cast<uint8_t*>(
        heap_caps_malloc(size(width, height), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (framebuffer->pixels == nullptr)
    {
        return ESP_ERR_NO_MEM;
    }
    framebuffer->width = width;
    framebuffer->height = height;
    return ESP_OK;
}

void destroy(framebuffer_t* framebuffer)
{
    if (framebuffer == nullptr)
    {
        return;
    }
    heap_caps_free(framebuffer->pixels);
    framebuffer->pixels = nullptr;
    framebuffer->width = 0;
    framebuffer->height = 0;
}

void clear(framebuffer_t* framebuffer, uint8_t value)
{
    if (framebuffer == nullptr || framebuffer->pixels == nullptr)
    {
        return;
    }
    const uint8_t packed = static_cast<uint8_t>((value & 0x0f) | ((value & 0x0f) << 4));
    memset(framebuffer->pixels, packed, size(framebuffer->width, framebuffer->height));
}

void set_pixel(framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint8_t value)
{
    if (framebuffer == nullptr || framebuffer->pixels == nullptr || x >= framebuffer->width ||
        y >= framebuffer->height)
    {
        return;
    }

    const size_t offset = (static_cast<size_t>(y) * framebuffer->width + x) / 2;
    if ((x & 1U) == 0)
    {
        framebuffer->pixels[offset] =
            static_cast<uint8_t>((framebuffer->pixels[offset] & 0x0f) | ((value & 0x0f) << 4));
    }
    else
    {
        framebuffer->pixels[offset] =
            static_cast<uint8_t>((framebuffer->pixels[offset] & 0xf0) | (value & 0x0f));
    }
}

void fill_rect(framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
               uint8_t value)
{
    if (framebuffer == nullptr || x >= framebuffer->width || y >= framebuffer->height)
    {
        return;
    }

    const uint16_t right = (width > framebuffer->width - x) ? framebuffer->width : x + width;
    const uint16_t bottom = (height > framebuffer->height - y) ? framebuffer->height : y + height;
    for (uint16_t pixel_y = y; pixel_y < bottom; ++pixel_y)
    {
        for (uint16_t pixel_x = x; pixel_x < right; ++pixel_x)
        {
            set_pixel(framebuffer, pixel_x, pixel_y, value);
        }
    }
}

void draw_rect(framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
               uint8_t value)
{
    if (width == 0 || height == 0)
    {
        return;
    }
    fill_rect(framebuffer, x, y, width, 1, value);
    fill_rect(framebuffer, x, static_cast<uint16_t>(y + height - 1), width, 1, value);
    fill_rect(framebuffer, x, y, 1, height, value);
    fill_rect(framebuffer, static_cast<uint16_t>(x + width - 1), y, 1, height, value);
}

} // namespace gfx
} // namespace xreader
