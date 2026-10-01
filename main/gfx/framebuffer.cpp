#include "framebuffer.hpp"

#include <string.h>

#include "esp_heap_caps.h"

namespace xreader
{
namespace gfx
{

namespace
{
// XTeink X4 has no PSRAM. A 4bpp 800x480 buffer (192000 bytes) alone exceeds
// its ~140KB of internal RAM once WiFi and other subsystems have taken their
// share, and there is no room at all for the second, diff-only buffer
// M5Paper (which has PSRAM) uses to compute a tight dirty rect. The panel is
// natively 1-bit anyway (see ssd1677.hpp), so the framebuffer's internal
// storage packs 1 bit/pixel for this board instead of 4 -- a quarter the
// size. This is purely an internal storage detail: every function below still
// takes/returns the same 0-15 grayscale value every caller already uses: it's
// just threshold-packed into a single bit here. Single-buffered mode makes
// present() treat the whole frame as dirty every call instead of diffing,
// which costs nothing extra anyway since this board's display driver always
// does a full-screen refresh.
#if defined(XREADER_BOARD_XTEINK)
constexpr uint32_t pixel_caps = MALLOC_CAP_8BIT;
constexpr bool double_buffered = false;
constexpr bool packed_1bpp = true;
#else
constexpr uint32_t pixel_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
constexpr bool double_buffered = true;
constexpr bool packed_1bpp = false;
#endif

bool value_is_white(uint8_t value)
{
    return (value & 0x0fU) >= 8U;
}
} // namespace

size_t size(uint16_t width, uint16_t height)
{
    if (packed_1bpp)
        return (static_cast<size_t>(width) * height + 7U) / 8U;
    return (static_cast<size_t>(width) * height + 1) / 2;
}

esp_err_t create(framebuffer_t* framebuffer, uint16_t width, uint16_t height)
{
    if (framebuffer == nullptr || width == 0 || height == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    framebuffer->pixels =
        static_cast<uint8_t*>(heap_caps_malloc(size(width, height), pixel_caps));
    framebuffer->front_pixels =
        double_buffered
            ? static_cast<uint8_t*>(heap_caps_calloc(1, size(width, height), pixel_caps))
            : nullptr;
    if (framebuffer->pixels == nullptr || (double_buffered && framebuffer->front_pixels == nullptr))
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
    if (framebuffer == nullptr || framebuffer->pixels == nullptr)
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
    if (framebuffer->front_pixels == nullptr)
    {
        // Single-buffered (no PSRAM for a diff copy): nothing to compare
        // against, so just report the bounding box set_pixel()/fill_rect()
        // already accumulated for whatever was actually drawn this frame,
        // rather than a real "what changed" diff.
        framebuffer->dirty_left = left;
        framebuffer->dirty_top = top;
        framebuffer->dirty_right = right;
        framebuffer->dirty_bottom = bottom;
        return ESP_OK;
    }
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
    if (packed_1bpp)
    {
        memset(framebuffer->pixels, value_is_white(value) ? 0xFF : 0x00,
              size(framebuffer->width, framebuffer->height));
    }
    else
    {
        const uint8_t packed = static_cast<uint8_t>((value & 0x0f) | ((value & 0x0f) << 4));
        memset(framebuffer->pixels, packed, size(framebuffer->width, framebuffer->height));
    }
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

    if (packed_1bpp)
    {
        const size_t row_bytes = (static_cast<size_t>(framebuffer->width) + 7U) / 8U;
        const size_t offset = static_cast<size_t>(y) * row_bytes + x / 8U;
        const uint8_t mask = static_cast<uint8_t>(0x80U >> (x % 8U));
        if (value_is_white(value))
            framebuffer->pixels[offset] = static_cast<uint8_t>(framebuffer->pixels[offset] | mask);
        else
            framebuffer->pixels[offset] =
                static_cast<uint8_t>(framebuffer->pixels[offset] & ~mask);
    }
    else
    {
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

    if (packed_1bpp)
    {
        // The framebuffer is 1 bit/pixel internally, but every caller of this
        // function (transfer_dirty(), the it8951e-shaped driver API) expects
        // a 4bpp output buffer -- expand each bit to a 0x0/0xF nibble here so
        // that boundary is the only place that needs to know about it.
        const size_t source_row_bytes = (static_cast<size_t>(framebuffer->width) + 7U) / 8U;
        memset(output, 0, gfx::size(width, height));
        for (uint16_t row = 0; row < height; ++row)
        {
            for (uint16_t column = 0; column < width; ++column)
            {
                const uint16_t source_x = static_cast<uint16_t>(x + column);
                const size_t source_offset =
                    static_cast<size_t>(y + row) * source_row_bytes + source_x / 8U;
                const uint8_t mask = static_cast<uint8_t>(0x80U >> (source_x % 8U));
                const uint8_t value =
                    (framebuffer->pixels[source_offset] & mask) != 0 ? 0x0FU : 0x00U;
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

    // transfer_dirty() always aligns x/width to four pixels. In that common case
    // the packed 4-bpp bytes can be copied a row at a time instead of unpacking
    // and repacking every pixel.
    if ((x & 1U) == 0 && (width & 1U) == 0)
    {
        const size_t source_stride = (static_cast<size_t>(framebuffer->width) + 1U) / 2U;
        const size_t row_bytes = static_cast<size_t>(width) / 2U;
        const size_t source_byte_x = static_cast<size_t>(x) / 2U;
        for (uint16_t row = 0; row < height; ++row)
        {
            memcpy(output + static_cast<size_t>(row) * row_bytes,
                   framebuffer->pixels + static_cast<size_t>(y + row) * source_stride +
                       source_byte_x,
                   row_bytes);
        }
        return ESP_OK;
    }

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
    if (framebuffer == nullptr || framebuffer->pixels == nullptr || x >= framebuffer->width ||
        y >= framebuffer->height || width == 0 || height == 0)
        return;

    const uint16_t right =
        width > framebuffer->width - x ? framebuffer->width : static_cast<uint16_t>(x + width);
    const uint16_t bottom =
        height > framebuffer->height - y ? framebuffer->height : static_cast<uint16_t>(y + height);

    if (packed_1bpp)
    {
        // Correctness over speed here: this board trades a byte-aligned
        // memset fast path for the 4x memory saving of 1bpp storage.
        for (uint16_t pixel_y = y; pixel_y < bottom; ++pixel_y)
            for (uint16_t pixel_x = x; pixel_x < right; ++pixel_x)
                set_pixel(framebuffer, pixel_x, pixel_y, value);
        if (x < framebuffer->dirty_left)
            framebuffer->dirty_left = x;
        if (y < framebuffer->dirty_top)
            framebuffer->dirty_top = y;
        if (right > framebuffer->dirty_right)
            framebuffer->dirty_right = right;
        if (bottom > framebuffer->dirty_bottom)
            framebuffer->dirty_bottom = bottom;
        return;
    }

    const uint8_t nibble = static_cast<uint8_t>(value & 0x0fU);
    const uint8_t packed = static_cast<uint8_t>((nibble << 4) | nibble);
    const size_t stride = (static_cast<size_t>(framebuffer->width) + 1U) / 2U;

    for (uint16_t pixel_y = y; pixel_y < bottom; ++pixel_y)
    {
        uint16_t left = x;
        uint16_t row_right = right;
        uint8_t* row = framebuffer->pixels + static_cast<size_t>(pixel_y) * stride;

        if ((left & 1U) != 0)
        {
            const size_t byte = static_cast<size_t>(left) / 2U;
            row[byte] = static_cast<uint8_t>((row[byte] & 0xf0U) | nibble);
            ++left;
        }
        if ((row_right & 1U) != 0 && row_right > left)
        {
            --row_right;
            const size_t byte = static_cast<size_t>(row_right) / 2U;
            row[byte] = static_cast<uint8_t>((row[byte] & 0x0fU) | (nibble << 4));
        }
        if (row_right > left)
            memset(row + static_cast<size_t>(left) / 2U, packed,
                   static_cast<size_t>(row_right - left) / 2U);
    }

    if (x < framebuffer->dirty_left)
        framebuffer->dirty_left = x;
    if (y < framebuffer->dirty_top)
        framebuffer->dirty_top = y;
    if (right > framebuffer->dirty_right)
        framebuffer->dirty_right = right;
    if (bottom > framebuffer->dirty_bottom)
        framebuffer->dirty_bottom = bottom;
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

void blit_4bpp_scaled(framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width,
                      uint16_t height, const uint8_t* source, uint16_t source_width,
                      uint16_t source_height)
{
    if (framebuffer == nullptr || framebuffer->pixels == nullptr || source == nullptr ||
        width == 0U || height == 0U || source_width == 0U || source_height == 0U ||
        x >= framebuffer->width || y >= framebuffer->height)
        return;
    const uint16_t draw_width =
        width > framebuffer->width - x ? static_cast<uint16_t>(framebuffer->width - x) : width;
    const uint16_t draw_height =
        height > framebuffer->height - y ? static_cast<uint16_t>(framebuffer->height - y) : height;
    for (uint16_t dy = 0; dy < draw_height; ++dy)
    {
        const uint16_t sy =
            static_cast<uint16_t>((static_cast<uint32_t>(dy) * source_height) / height);
        for (uint16_t dx = 0; dx < draw_width; ++dx)
        {
            const uint16_t sx =
                static_cast<uint16_t>((static_cast<uint32_t>(dx) * source_width) / width);
            const size_t source_pixel = static_cast<size_t>(sy) * source_width + sx;
            const uint8_t packed = source[source_pixel / 2U];
            const uint8_t value = (source_pixel & 1U) == 0U ? static_cast<uint8_t>(packed >> 4U)
                                                            : static_cast<uint8_t>(packed & 0x0fU);
            set_pixel(framebuffer, static_cast<uint16_t>(x + dx), static_cast<uint16_t>(y + dy),
                      value);
        }
    }
}

} // namespace gfx
} // namespace xreader
