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
    framebuffer->front_pixels = static_cast<uint8_t*>(
        heap_caps_calloc(1, size(width, height), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (framebuffer->pixels == nullptr || framebuffer->front_pixels == nullptr)
    {
        heap_caps_free(framebuffer->pixels);
        heap_caps_free(framebuffer->front_pixels);
        framebuffer->pixels = nullptr;
        framebuffer->front_pixels = nullptr;
        return ESP_ERR_NO_MEM;
    }
    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->dirty_left = width;
    framebuffer->dirty_top = height;
    framebuffer->dirty_right = 0;
    framebuffer->dirty_bottom = 0;
    return ESP_OK;
}

void destroy(framebuffer_t* framebuffer)
{
    if (framebuffer == nullptr)
    {
        return;
    }
    heap_caps_free(framebuffer->pixels);
    heap_caps_free(framebuffer->front_pixels);
    framebuffer->pixels = nullptr;
    framebuffer->front_pixels = nullptr;
    framebuffer->width = 0;
    framebuffer->height = 0;
    framebuffer->dirty_left = 0;
    framebuffer->dirty_top = 0;
    framebuffer->dirty_right = 0;
    framebuffer->dirty_bottom = 0;
}

esp_err_t present(framebuffer_t* framebuffer)
{
    if (framebuffer == nullptr || framebuffer->pixels == nullptr ||
        framebuffer->front_pixels == nullptr)
        return ESP_ERR_INVALID_ARG;
    uint16_t left = framebuffer->dirty_left;
    uint16_t top = framebuffer->dirty_top;
    uint16_t right = framebuffer->dirty_right;
    uint16_t bottom = framebuffer->dirty_bottom;
    framebuffer->dirty_left = framebuffer->width;
    framebuffer->dirty_top = framebuffer->height;
    framebuffer->dirty_right = 0;
    framebuffer->dirty_bottom = 0;
    if (left >= right || top >= bottom)
        return ESP_OK;
    const uint16_t first_byte = static_cast<uint16_t>(left / 2);
    const uint16_t last_byte = static_cast<uint16_t>((right + 1) / 2);
    const uint16_t row_bytes = static_cast<uint16_t>((framebuffer->width + 1) / 2);
    for (uint16_t y = top; y < bottom; ++y)
    {
        for (uint16_t byte = first_byte; byte < last_byte; ++byte)
        {
            const size_t offset = static_cast<size_t>(y) * row_bytes + byte;
            if (framebuffer->pixels[offset] == framebuffer->front_pixels[offset])
                continue;
            framebuffer->front_pixels[offset] = framebuffer->pixels[offset];
            const uint16_t x = static_cast<uint16_t>(byte * 2);
            if (x < framebuffer->dirty_left)
                framebuffer->dirty_left = x;
            if (y < framebuffer->dirty_top)
                framebuffer->dirty_top = y;
            if (x + 2 > framebuffer->dirty_right)
                framebuffer->dirty_right = static_cast<uint16_t>(x + 2);
            if (y + 1 > framebuffer->dirty_bottom)
                framebuffer->dirty_bottom = static_cast<uint16_t>(y + 1);
        }
    }
    if (framebuffer->dirty_right > framebuffer->width)
        framebuffer->dirty_right = framebuffer->width;
    return ESP_OK;
}

void clear(framebuffer_t* framebuffer, uint8_t value)
{
    if (framebuffer == nullptr || framebuffer->pixels == nullptr)
    {
        return;
    }
    const uint8_t packed = static_cast<uint8_t>((value & 0x0f) | ((value & 0x0f) << 4));
    memset(framebuffer->pixels, packed, size(framebuffer->width, framebuffer->height));
    framebuffer->dirty_left = 0;
    framebuffer->dirty_top = 0;
    framebuffer->dirty_right = framebuffer->width;
    framebuffer->dirty_bottom = framebuffer->height;
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
    if (x < framebuffer->dirty_left)
        framebuffer->dirty_left = x;
    if (y < framebuffer->dirty_top)
        framebuffer->dirty_top = y;
    if (x + 1 > framebuffer->dirty_right)
        framebuffer->dirty_right = static_cast<uint16_t>(x + 1);
    if (y + 1 > framebuffer->dirty_bottom)
        framebuffer->dirty_bottom = static_cast<uint16_t>(y + 1);
}

bool take_dirty(framebuffer_t* framebuffer, uint16_t* x, uint16_t* y, uint16_t* width,
                uint16_t* height)
{
    if (framebuffer == nullptr || x == nullptr || y == nullptr || width == nullptr ||
        height == nullptr || framebuffer->dirty_left >= framebuffer->dirty_right ||
        framebuffer->dirty_top >= framebuffer->dirty_bottom)
        return false;
    *x = framebuffer->dirty_left;
    *y = framebuffer->dirty_top;
    *width = static_cast<uint16_t>(framebuffer->dirty_right - framebuffer->dirty_left);
    *height = static_cast<uint16_t>(framebuffer->dirty_bottom - framebuffer->dirty_top);
    framebuffer->dirty_left = framebuffer->width;
    framebuffer->dirty_top = framebuffer->height;
    framebuffer->dirty_right = 0;
    framebuffer->dirty_bottom = 0;
    return true;
}

esp_err_t copy_region_4bpp(const framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width,
                           uint16_t height, uint8_t* output, size_t output_size)
{
    if (framebuffer == nullptr || framebuffer->pixels == nullptr || output == nullptr ||
        width == 0 || height == 0 || (width & 3U) != 0 || x >= framebuffer->width ||
        y >= framebuffer->height || width > framebuffer->width - x ||
        height > framebuffer->height - y || output_size < size(width, height))
        return ESP_ERR_INVALID_ARG;
    memset(output, 0, size(width, height));
    for (uint16_t row = 0; row < height; ++row)
    {
        for (uint16_t column = 0; column < width; ++column)
        {
            const uint16_t source_x = static_cast<uint16_t>(x + column);
            const size_t source_offset =
                (static_cast<size_t>(y + row) * framebuffer->width + source_x) / 2;
            const uint8_t value = (source_x & 1U) == 0 ? framebuffer->pixels[source_offset] >> 4
                                                       : framebuffer->pixels[source_offset] & 0x0f;
            const size_t destination_offset = (static_cast<size_t>(row) * width + column) / 2;
            if ((column & 1U) == 0)
                output[destination_offset] = static_cast<uint8_t>(value << 4);
            else
                output[destination_offset] =
                    static_cast<uint8_t>(output[destination_offset] | value);
        }
    }
    return ESP_OK;
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
