#include "focus.hpp"

namespace xreader
{
namespace ui
{
namespace focus
{

uint8_t next(uint8_t current, uint8_t count)
{
    if (count == 0)
        return 0;
    return static_cast<uint8_t>((current + 1U) % count);
}

uint8_t previous(uint8_t current, uint8_t count)
{
    if (count == 0)
        return 0;
    return static_cast<uint8_t>((current + count - 1U) % count);
}

bool hit_rows(layout::rect_t area, uint8_t count, uint16_t row_height, uint16_t gap, uint16_t x,
              uint16_t y, uint8_t* index)
{
    if (index == nullptr)
        return false;
    for (uint8_t candidate = 0; candidate < count; ++candidate)
    {
        const layout::rect_t bounds = layout::row(area, candidate, count, row_height, gap);
        if (layout::contains(bounds, x, y))
        {
            *index = candidate;
            return true;
        }
    }
    return false;
}

} // namespace focus
} // namespace ui
} // namespace xreader
