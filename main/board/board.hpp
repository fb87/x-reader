#pragma once

#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace board
{

struct capabilities_t
{
    uint16_t display_width;
    uint16_t display_height;
    bool has_touch;
    bool has_rotary;
    bool has_sd;
};

esp_err_t get_capabilities(capabilities_t* capabilities);

} // namespace board
} // namespace xreader
