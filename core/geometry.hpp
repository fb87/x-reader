#pragma once

#include <cstdint>

namespace geometry {

/** @brief Rectangle in logical display coordinates. */
struct rect {
    int x;
    int y;
    int w;
    int h;
};

/** @brief Returns true when two rectangles overlap. */
constexpr bool intersects(rect lhs, rect rhs)
{
    return lhs.x < rhs.x + rhs.w && lhs.x + lhs.w > rhs.x && lhs.y < rhs.y + rhs.h &&
           lhs.y + lhs.h > rhs.y;
}

/** @brief Clamps a rectangle to the supplied bounds. */
constexpr rect clamp(rect area, int width, int height)
{
    if (area.x < 0) {
        area.w += area.x;
        area.x = 0;
    }
    if (area.y < 0) {
        area.h += area.y;
        area.y = 0;
    }
    if (area.x + area.w > width) {
        area.w = width - area.x;
    }
    if (area.y + area.h > height) {
        area.h = height - area.y;
    }
    if (area.w < 0) {
        area.w = 0;
    }
    if (area.h < 0) {
        area.h = 0;
    }
    return area;
}

} // namespace geometry
