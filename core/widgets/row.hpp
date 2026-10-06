#pragma once

#include "core/text.hpp"
#include "core/widgets/style.hpp"

namespace widget {

/** @brief Renders one two-column list row. */
inline void row(display::device& display, geometry::rect rect, const char* primary,
                const char* secondary, bool selected, const style& style = {}) {
  canvas::fill(display, rect, selected ? style.focus_background : style.background);
  text::draw(display, rect.x + 14, rect.y + 16, primary, style.text_scale,
             selected ? style.focus_foreground : style.foreground);
  if (secondary != nullptr && secondary[0] != '\0') {
    text::draw(display, rect.x + rect.w - text::width(secondary, style.text_scale) - 14,
               rect.y + 16, secondary, style.text_scale,
               selected ? style.focus_foreground : style.secondary);
  }
  if (!selected) canvas::hline(display, rect.x, rect.y + rect.h - 1, rect.w, style.divider);
}

}  // namespace widget
