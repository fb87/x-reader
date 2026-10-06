#pragma once

#include "core/text.hpp"
#include "core/widgets/style.hpp"

namespace widget {

/** @brief Renders the stock continue-reading book card. All visible text is caller-owned. */
inline void book_card(display::device& display, geometry::rect rect, const char* eyebrow,
                      const char* title, bool active, const style& style = {}) {
  canvas::border(display, rect, active ? 4 : 3, style.foreground);
  text::draw(display, rect.x + 16, rect.y + 20, eyebrow, style.text_scale, style.secondary);
  text::draw(display, rect.x + 16, rect.y + 64, title, 3, style.foreground);
}

}  // namespace widget
