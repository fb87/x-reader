#pragma once

#include "core/widgets/style.hpp"

namespace widget {

/** @brief Renders a bounded 0-100 progress indicator. */
inline void progress(display::device& display, geometry::rect rect, int value,
                     const style& style = {}) {
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  canvas::border(display, rect, 1, style.divider);
  canvas::fill(display, {rect.x + 1, rect.y + 1, (rect.w - 2) * value / 100, rect.h - 2},
               style.secondary);
}

}  // namespace widget
