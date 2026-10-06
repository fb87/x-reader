#pragma once

#include "core/text.hpp"
#include "core/widgets/style.hpp"

namespace widget {

/** @brief Renders the stock continue-reading book card. All visible text is caller-owned. */
inline void book_card(display::device& display, geometry::rect rect, const char* eyebrow,
                      const char* title, bool active, const style& style = {}) {
  canvas::border(display, rect, active ? 4 : 3, style.foreground);
  text::draw_in(display, rect.x + 16, rect.y + 20, rect.w - 32, eyebrow, style.text_scale,
                style.secondary);
  text::draw_in(display, rect.x + 16, rect.y + 64, rect.w - 32, title, 3, style.foreground);
}

}  // namespace widget
