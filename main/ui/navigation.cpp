#include "navigation.hpp"

namespace xreader
{
namespace ui
{

int8_t page_delta(navigation_input_t input, uint16_t x, uint16_t display_width, uint8_t page,
                  uint8_t page_count)
{
    const bool forward = input == navigation_rotary_clockwise ||
                         (input == navigation_touch_up && x > display_width / 2);
    const bool backward = input == navigation_rotary_counterclockwise ||
                          (input == navigation_touch_up && x <= display_width / 2);
    if (forward && page + 1 < page_count)
        return 1;
    if (backward && page > 0)
        return -1;
    return 0;
}

} // namespace ui
} // namespace xreader
