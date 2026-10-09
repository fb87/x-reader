#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @brief Raw RFC 1951 DEFLATE decoder, ported from `xreader::inflate`
 * Used on-device to decompress EPUB ZIP
 * entries without pulling zlib onto the ESP32 target.
 *
 * UNVERIFIED IN THIS SANDBOX: no ESP-IDF toolchain is available here.
 * Logic is otherwise unchanged from the old implementation.
 *
 * This is a reusable protocol/codec driver (it implements a wire format,
 * not board wiring), so it lives under drivers/ alongside it8951/gt911
 * even though it is software rather than a bus driver -- any future
 * board without zlib would want the same decoder.
 */
namespace drivers::inflate {

esp_err_t decode(const uint8_t* input, size_t input_size, uint8_t* output,
                 size_t output_capacity, size_t* output_size);

}  // namespace drivers::inflate
