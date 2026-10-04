#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

namespace xreader::inflate {

esp_err_t decode(const uint8_t* input, size_t input_size, uint8_t* output,
                 size_t output_capacity, size_t* output_size);

} // namespace xreader::inflate
