#pragma once

#include <stdint.h>

namespace xreader
{
namespace ui
{

enum navigation_input_t : uint8_t
{
    navigation_touch_up,
    navigation_rotary_clockwise,
    navigation_rotary_counterclockwise,
};

int8_t page_delta(navigation_input_t input, uint16_t x, uint16_t display_width, uint8_t page,
                  uint8_t page_count);

} // namespace ui
} // namespace xreader
