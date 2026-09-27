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

enum navigation_result_t : uint8_t
{
    navigation_none,
    navigation_page_forward,
    navigation_page_backward,
    navigation_chapter_forward,
    navigation_chapter_backward,
};

int8_t page_delta(navigation_input_t input, uint16_t x, uint16_t display_width, uint8_t page,
                  uint8_t page_count);
navigation_result_t navigation_result(navigation_input_t input, uint16_t x, uint16_t display_width,
                                      uint8_t page, uint8_t page_count, uint8_t spine_index,
                                      uint8_t spine_count);

} // namespace ui
} // namespace xreader
