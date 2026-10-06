#pragma once

#include "core/text.hpp"
#include "core/widgets/style.hpp"

namespace widget {

/** @brief Renders the stock modal dialog frame and text. */
inline void dialog(display::device& display, const char* title, const char* body,
                   const style& style = {}) {
  geometry::rect box{40, 260, display.width - 80, 330};
  canvas::stipple(display, {box.x + 8, box.y + 8, box.w, box.h}, style.secondary);
  canvas::fill(display, box, style.background);
  canvas::border(display, box, 3, style.foreground);
  text::draw_in(display, box.x + 20, box.y + 24, box.w - 40, title, 3, style.foreground);
  text::draw_in(display, box.x + 20, box.y + 92, box.w - 40, body, style.text_scale, style.foreground);
}

}  // namespace widget
