#pragma once

#include <stdint.h>

#include "input/action.hpp"

namespace xreader
{
namespace ui
{

enum navigation_result_t : uint8_t
{
    navigation_none,
    navigation_page_forward,
    navigation_page_backward,
    navigation_chapter_forward,
    navigation_chapter_backward,
};

int8_t page_delta(const input::action_event_t* event, uint16_t display_width, uint8_t page,
                  uint8_t page_count);
navigation_result_t navigation_result(const input::action_event_t* event, uint16_t display_width,
                                      uint8_t page, uint8_t page_count, uint8_t spine_index,
                                      uint8_t spine_count);

} // namespace ui
} // namespace xreader
