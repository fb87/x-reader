#pragma once

#include <cstdint>

/**
 * @brief Screen-space rectangles shared by every drawing/dispatch module.
 *
 * Coordinates are absolute screen pixels, ported from `xr_rect_t`
 * (include/xr/xr_types.h). `int16_t` storage is preserved because every
 * current and planned panel profile (540x960, 480x800) fits comfortably,
 * and it keeps `rect` the same size the dirty-rect scheduler was tuned
 * against.
 */
namespace geometry {

/** @brief An axis-aligned rectangle; width/height <= 0 means empty. */
struct rect {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t w = 0;
  std::int16_t h = 0;
};

/** @brief Constructs a rect from plain ints, narrowing to the storage type. */
constexpr rect make(int x, int y, int w, int h) {
  return rect{static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
              static_cast<std::int16_t>(w), static_cast<std::int16_t>(h)};
}

/** @brief True when the rect has no area (width or height <= 0). */
constexpr bool empty(rect r) { return r.w <= 0 || r.h <= 0; }

/** @brief True when the point (x, y) falls inside the rect. */
constexpr bool contains(rect r, int x, int y) {
  return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

/** @brief The overlapping region of two rects, or an empty rect if none. */
constexpr rect intersect(rect a, rect b) {
  const int x0 = a.x > b.x ? a.x : b.x;
  const int y0 = a.y > b.y ? a.y : b.y;
  const int x1 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
  const int y1 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
  if (x1 <= x0 || y1 <= y0) return rect{};
  return make(x0, y0, x1 - x0, y1 - y0);
}

/** @brief True when the two rects overlap. */
constexpr bool intersects(rect a, rect b) { return !empty(intersect(a, b)); }

/** @brief The smallest rect covering both inputs (named `merge`, not `union`: a C++ keyword). */
constexpr rect merge(rect a, rect b) {
  if (empty(a)) return b;
  if (empty(b)) return a;
  const int x0 = a.x < b.x ? a.x : b.x;
  const int y0 = a.y < b.y ? a.y : b.y;
  const int x1 = (a.x + a.w) > (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
  const int y1 = (a.y + a.h) > (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
  return make(x0, y0, x1 - x0, y1 - y0);
}

/** @brief Shrinks (or grows, for negative `d`) a rect by `d` on every side. */
constexpr rect inset(rect r, int d) { return make(r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d); }

/** @brief Pixel area of the rect, or 0 when empty. */
constexpr std::int32_t area(rect r) {
  return empty(r) ? 0 : static_cast<std::int32_t>(r.w) * r.h;
}

}  // namespace geometry
