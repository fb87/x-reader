#pragma once

#include "core/text.hpp"
#include "core/icons.hpp"
#include "core/widgets/style.hpp"

namespace widget {

/** @brief Renders the stock bottom dock. */
inline void dock(display::device& display, const char* const* labels, int count, int selected,
                 bool focused, const style& style = {}, const int* icons = nullptr) {
  if (count <= 0) return;
  const int y = display.height - 64;
  const int width = display.width / count;
  canvas::hline(display, 0, y, display.width, style.foreground);
  for (int i = 0; i < count; ++i) {
    const bool active = focused && selected == i;
    canvas::fill(display, {i * width, y + 1, width, 63},
                 active ? style.focus_background : style.background);
    if (icons != nullptr && icons[i] != 0)
      icon::draw(display, {i * width + width / 2 - 12, y + 8, 24, 24}, icons[i],
                 active ? style.focus_foreground : style.foreground);
    text::center(display, {i * width, y + (icons != nullptr && icons[i] != 0 ? 30 : 1), width,
                           icons != nullptr && icons[i] != 0 ? 33 : 63},
                 labels[i], style.text_scale, active ? style.focus_foreground : style.foreground);
  }
}

}  // namespace widget
