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

navigation_result_t navigation_result(navigation_input_t input, uint16_t x, uint16_t display_width,
                                      uint8_t page, uint8_t page_count, uint8_t spine_index,
                                      uint8_t spine_count)
{
    const int8_t delta = page_delta(input, x, display_width, page, page_count);
    if (delta > 0)
        return navigation_page_forward;
    if (delta < 0)
        return navigation_page_backward;
    if (input == navigation_rotary_clockwise ||
        (input == navigation_touch_up && x > display_width / 2))
        return spine_index + 1 < spine_count ? navigation_chapter_forward : navigation_none;
    if (input == navigation_rotary_counterclockwise ||
        (input == navigation_touch_up && x <= display_width / 2))
        return spine_index > 0 ? navigation_chapter_backward : navigation_none;
    return navigation_none;
}

} // namespace ui
} // namespace xreader
