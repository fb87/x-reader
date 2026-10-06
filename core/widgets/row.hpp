#pragma once

#include "core/text.hpp"
#include "core/widgets/style.hpp"

namespace widget {

/** @brief Renders one two-column list row. */
inline void row(display::device& display, geometry::rect rect, const char* primary,
                const char* secondary, bool selected, const style& style = {}) {
  canvas::fill(display, rect, selected ? style.focus_background : style.background);
  text::draw_in(display, rect.x + 14, rect.y + 16, rect.w - 28, primary, style.text_scale,
                selected ? style.focus_foreground : style.foreground);
  if (secondary != nullptr && secondary[0] != '\0') {
    const int secondary_width = text::width(secondary, style.text_scale);
    text::draw_in(display, rect.x + rect.w - secondary_width - 14, rect.y + 16,
               secondary_width, secondary, style.text_scale,
               selected ? style.focus_foreground : style.secondary);
  }
  if (!selected) canvas::hline(display, rect.x, rect.y + rect.h - 1, rect.w, style.divider);
}

}  // namespace widget
