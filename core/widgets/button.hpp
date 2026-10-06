#pragma once

#include "core/text.hpp"
#include "core/widgets/style.hpp"

namespace widget {

/** @brief Renders the stock button. */
inline void button(display::device& display, geometry::rect rect, const char* label, bool active,
                   const style& style = {}) {
  canvas::fill(display, rect, active ? style.focus_background : style.background);
  if (style.border_width > 0) canvas::border(display, rect, style.border_width, style.foreground);
  text::center(display, rect, label, style.text_scale,
               active ? style.focus_foreground : style.foreground);
}

}  // namespace widget
