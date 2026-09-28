#include "navigation.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static bool forward(const input::action_event_t* event, uint16_t display_width)
{
    if (event == nullptr)
        return false;
    return event->action == input::action_right || event->action == input::action_down ||
           event->action == input::action_page_next ||
           (event->action == input::action_pointer && event->x > display_width / 2U);
}

static bool backward(const input::action_event_t* event, uint16_t display_width)
{
    if (event == nullptr)
        return false;
    return event->action == input::action_left || event->action == input::action_up ||
           event->action == input::action_page_prev ||
           (event->action == input::action_pointer && event->x <= display_width / 2U);
}
} // namespace

int8_t page_delta(const input::action_event_t* event, uint16_t display_width, uint8_t page,
                  uint8_t page_count)
{
    if (forward(event, display_width) && page + 1U < page_count)
        return 1;
    if (backward(event, display_width) && page > 0)
        return -1;
    return 0;
}

navigation_result_t navigation_result(const input::action_event_t* event, uint16_t display_width,
                                      uint8_t page, uint8_t page_count, uint8_t spine_index,
                                      uint8_t spine_count)
{
    const int8_t delta = page_delta(event, display_width, page, page_count);
    if (delta > 0)
        return navigation_page_forward;
    if (delta < 0)
        return navigation_page_backward;
    if (forward(event, display_width))
        return spine_index + 1U < spine_count ? navigation_chapter_forward : navigation_none;
    if (backward(event, display_width))
        return spine_index > 0 ? navigation_chapter_backward : navigation_none;
    return navigation_none;
}

} // namespace ui
} // namespace xreader
