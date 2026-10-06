#pragma once

#include "core/text.hpp"
#include "core/icons.hpp"
#include "core/widgets/style.hpp"

namespace widget {

/** @brief Renders one two-column list row. */
inline void row(display::device& display, geometry::rect rect, const char* primary,
                const char* secondary, bool selected, const style& style = {}, int icon_value = 0) {
  canvas::fill(display, rect, selected ? style.focus_background : style.background);
  const int text_x = icon_value > 0 ? rect.x + 48 : rect.x + 14;
  if (icon_value > 0)
    icon::draw(display, {rect.x + 14, rect.y + 22, 24, 24}, icon_value,
               selected ? style.focus_foreground : style.foreground);
  text::draw_in(display, text_x, rect.y + 16, rect.x + rect.w - text_x - 14, primary, style.text_scale,
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
