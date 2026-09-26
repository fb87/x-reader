#include "image.hpp"

#include <string.h>

namespace xreader
{
namespace epub
{
namespace image
{

esp_err_t inspect(const uint8_t* data, size_t size, info_t* info)
{
    if (data == nullptr || info == nullptr || size < 8)
        return ESP_ERR_INVALID_ARG;
    *info = {};
    if (size >= 24 && memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0)
    {
        info->width = static_cast<uint16_t>((data[16] << 8) | data[17]);
        info->height = static_cast<uint16_t>((data[20] << 8) | data[21]);
        info->supported = false;
        return ESP_OK;
    }
    if (size >= 4 && data[0] == 0xff && data[1] == 0xd8)
        return ESP_ERR_NOT_SUPPORTED;
    return ESP_ERR_INVALID_RESPONSE;
}

esp_err_t decode_mono(const uint8_t* data, size_t size, uint8_t* output, size_t output_size)
{
    if (data == nullptr || output == nullptr || size == 0 || output_size == 0)
        return ESP_ERR_INVALID_ARG;
    return ESP_ERR_NOT_SUPPORTED;
}

} // namespace image
} // namespace epub
} // namespace xreader
