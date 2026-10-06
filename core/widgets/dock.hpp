#pragma once

#include "core/text.hpp"
#include "core/widgets/style.hpp"

namespace widget {

/** @brief Renders the stock bottom dock. */
inline void dock(display::device& display, const char* const* labels, int count, int selected,
                 bool focused, const style& style = {}) {
  if (count <= 0) return;
  const int y = display.height - 64;
  const int width = display.width / count;
  canvas::hline(display, 0, y, display.width, style.foreground);
  for (int i = 0; i < count; ++i) {
    const bool active = focused && selected == i;
    canvas::fill(display, {i * width, y + 1, width, 63},
                 active ? style.focus_background : style.background);
    text::center(display, {i * width, y + 1, width, 63}, labels[i], style.text_scale,
                 active ? style.focus_foreground : style.foreground);
  }
}

}  // namespace widget
