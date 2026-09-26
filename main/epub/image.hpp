#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace epub
{
namespace image
{

struct info_t
{
    uint16_t width;
    uint16_t height;
    bool supported;
};

esp_err_t inspect(const uint8_t* data, size_t size, info_t* info);
esp_err_t decode_mono(const uint8_t* data, size_t size, uint8_t* output, size_t output_size);

} // namespace image
} // namespace epub
} // namespace xreader
