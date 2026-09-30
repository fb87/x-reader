#include "image.hpp"

#include <stdlib.h>
#include <string.h>

#include "inflate.hpp"

namespace xreader
{
namespace epub
{
namespace image
{

namespace
{

struct png_t
{
    uint16_t width;
    uint16_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t* compressed;
    size_t compressed_size;
};

static uint32_t read_u32(const uint8_t* data)
{
    return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) | data[3];
}

static esp_err_t parse_png(const uint8_t* data, size_t size, png_t* png)
{
    if (data == nullptr || png == nullptr || size < 8 || memcmp(data, "\x89PNG\r\n\x1a\n", 8) != 0)
        return ESP_ERR_INVALID_RESPONSE;
    *png = {};
    size_t offset = 8;
    while (offset <= size - 12)
    {
        const uint32_t length = read_u32(data + offset);
        offset += 4;
        const uint8_t* type = data + offset;
        offset += 4;
        if (length > size - offset - 4)
            return ESP_ERR_INVALID_SIZE;
        const uint8_t* chunk = data + offset;
        if (memcmp(type, "IHDR", 4) == 0)
        {
            if (length != 13 || png->width != 0)
                return ESP_ERR_INVALID_RESPONSE;
            const uint32_t width = read_u32(chunk);
            const uint32_t height = read_u32(chunk + 4);
            if (width == 0 || height == 0 || width > UINT16_MAX || height > UINT16_MAX)
                return ESP_ERR_INVALID_SIZE;
            png->width = static_cast<uint16_t>(width);
            png->height = static_cast<uint16_t>(height);
            png->bit_depth = chunk[8];
            png->color_type = chunk[9];
            if (png->bit_depth != 8 || (png->color_type != 0 && png->color_type != 2 &&
                                        png->color_type != 4 && png->color_type != 6))
                return ESP_ERR_NOT_SUPPORTED;
        }
        else if (memcmp(type, "IDAT", 4) == 0)
        {
            if (length > SIZE_MAX - png->compressed_size)
                return ESP_ERR_INVALID_SIZE;
            uint8_t* expanded =
                static_cast<uint8_t*>(realloc(png->compressed, png->compressed_size + length));
            if (expanded == nullptr)
                return ESP_ERR_NO_MEM;
            png->compressed = expanded;
            memcpy(png->compressed + png->compressed_size, chunk, length);
            png->compressed_size += length;
        }
        offset += length + 4;
        if (memcmp(type, "IEND", 4) == 0)
            break;
    }
    if (png->width == 0 || png->height == 0 || png->compressed_size < 6)
        return ESP_ERR_INVALID_RESPONSE;
    return ESP_OK;
}

static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c)
{
    const int p = a + b - c;
    const int pa = abs(p - a);
    const int pb = abs(p - b);
    const int pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

static esp_err_t inspect_jpeg(const uint8_t* data, size_t size, info_t* info)
{
    if (data == nullptr || info == nullptr || size < 4U || data[0] != 0xffU || data[1] != 0xd8U)
        return ESP_ERR_INVALID_RESPONSE;
    size_t offset = 2U;
    while (offset + 4U <= size)
    {
        while (offset < size && data[offset] == 0xffU)
            ++offset;
        if (offset >= size)
            break;
        const uint8_t marker = data[offset++];
        if (marker == 0xd9U || marker == 0xdaU)
            break;
        if (marker == 0x01U || (marker >= 0xd0U && marker <= 0xd7U))
            continue;
        if (offset + 2U > size)
            return ESP_ERR_INVALID_SIZE;
        const uint16_t length =
            static_cast<uint16_t>((static_cast<uint16_t>(data[offset]) << 8U) | data[offset + 1U]);
        if (length < 2U || offset + length > size)
            return ESP_ERR_INVALID_SIZE;
        const bool sof =
            (marker >= 0xc0U && marker <= 0xc3U) || (marker >= 0xc5U && marker <= 0xc7U) ||
            (marker >= 0xc9U && marker <= 0xcbU) || (marker >= 0xcdU && marker <= 0xcfU);
        if (sof)
        {
            if (length < 7U)
                return ESP_ERR_INVALID_RESPONSE;
            const uint16_t height = static_cast<uint16_t>(
                (static_cast<uint16_t>(data[offset + 3U]) << 8U) | data[offset + 4U]);
            const uint16_t width = static_cast<uint16_t>(
                (static_cast<uint16_t>(data[offset + 5U]) << 8U) | data[offset + 6U]);
            if (width == 0U || height == 0U)
                return ESP_ERR_INVALID_SIZE;
            info->width = width;
            info->height = height;
            info->supported = false; // dimensions known; decoder is intentionally still PNG-only.
            return ESP_OK;
        }
        offset += length;
    }
    return ESP_ERR_INVALID_RESPONSE;
}

static uint8_t sample(const uint8_t* row, uint8_t color_type, uint16_t x)
{
    const uint8_t* pixel = row + static_cast<size_t>(x) * (color_type == 0   ? 1
                                                           : color_type == 2 ? 3
                                                           : color_type == 4 ? 2
                                                                             : 4);
    const uint16_t luminance =
        color_type == 0 || color_type == 4
            ? static_cast<uint16_t>(pixel[0])
            : static_cast<uint16_t>((static_cast<uint32_t>(pixel[0]) * 77U +
                                     static_cast<uint32_t>(pixel[1]) * 150U +
                                     static_cast<uint32_t>(pixel[2]) * 29U) >>
                                    8);
    return static_cast<uint8_t>(15U - (luminance * 15U / 255U));
}

} // namespace

esp_err_t inspect(const uint8_t* data, size_t size, info_t* info)
{
    if (data == nullptr || info == nullptr || size < 8)
        return ESP_ERR_INVALID_ARG;
    *info = {};
    png_t png = {};
    esp_err_t error = parse_png(data, size, &png);
    if (error == ESP_OK)
    {
        info->width = png.width;
        info->height = png.height;
        info->supported = true;
        free(png.compressed);
        return ESP_OK;
    }
    free(png.compressed);
    if (size >= 2U && data[0] == 0xffU && data[1] == 0xd8U)
        return inspect_jpeg(data, size, info);
    return error;
}

esp_err_t decode_mono(const uint8_t* data, size_t size, uint8_t* output, size_t output_size)
{
    if (data == nullptr || output == nullptr || size == 0 || output_size == 0)
        return ESP_ERR_INVALID_ARG;
    png_t png = {};
    esp_err_t error = parse_png(data, size, &png);
    if (error != ESP_OK)
    {
        free(png.compressed);
        return error;
    }
    const size_t channels = png.color_type == 0   ? 1
                            : png.color_type == 2 ? 3
                            : png.color_type == 4 ? 2
                                                  : 4;
    const size_t row_size = static_cast<size_t>(png.width) * channels;
    if (row_size > SIZE_MAX - 1 || static_cast<size_t>(png.height) > SIZE_MAX / (row_size + 1) ||
        output_size < (static_cast<size_t>(png.width) * png.height + 1) / 2)
    {
        free(png.compressed);
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t* filtered =
        static_cast<uint8_t*>(malloc(static_cast<size_t>(png.height) * (row_size + 1)));
    uint8_t* previous = static_cast<uint8_t*>(calloc(row_size, 1));
    if (filtered == nullptr || previous == nullptr)
    {
        free(filtered);
        free(previous);
        free(png.compressed);
        return ESP_ERR_NO_MEM;
    }
    size_t decoded_size = 0;
    if (png.compressed_size < 6 ||
        inflate::decode(png.compressed + 2, png.compressed_size - 6, filtered,
                        static_cast<size_t>(png.height) * (row_size + 1),
                        &decoded_size) != ESP_OK ||
        decoded_size != static_cast<size_t>(png.height) * (row_size + 1))
        error = ESP_ERR_INVALID_RESPONSE;
    memset(output, 0, (static_cast<size_t>(png.width) * png.height + 1) / 2);
    for (uint16_t y = 0; error == ESP_OK && y < png.height; ++y)
    {
        uint8_t* row = filtered + static_cast<size_t>(y) * (row_size + 1);
        const uint8_t filter = row[0];
        for (size_t index = 0; index < row_size; ++index)
        {
            const uint8_t left = index >= channels ? row[index - channels + 1] : 0;
            const uint8_t up = previous[index];
            const uint8_t upper_left = index >= channels ? previous[index - channels] : 0;
            if (filter == 1)
                row[index + 1] = static_cast<uint8_t>(row[index + 1] + left);
            else if (filter == 2)
                row[index + 1] = static_cast<uint8_t>(row[index + 1] + up);
            else if (filter == 3)
                row[index + 1] = static_cast<uint8_t>(row[index + 1] + ((left + up) / 2));
            else if (filter == 4)
                row[index + 1] = static_cast<uint8_t>(row[index + 1] + paeth(left, up, upper_left));
            else if (filter != 0)
                error = ESP_ERR_NOT_SUPPORTED;
        }
        if (error != ESP_OK)
            break;
        for (uint16_t x = 0; x < png.width; ++x)
        {
            const size_t pixel = static_cast<size_t>(y) * png.width + x;
            const uint8_t value = sample(row + 1, png.color_type, x);
            output[pixel / 2] =
                static_cast<uint8_t>((x & 1U) == 0 ? (value << 4) | (output[pixel / 2] & 0x0f)
                                                   : (output[pixel / 2] & 0xf0) | value);
        }
        memcpy(previous, row + 1, row_size);
    }
    free(filtered);
    free(previous);
    free(png.compressed);
    return error;
}

} // namespace image
} // namespace epub
} // namespace xreader
