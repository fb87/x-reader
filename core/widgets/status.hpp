#pragma once

#include "core/platform.hpp"
#include "core/text.hpp"
#include "core/widgets/style.hpp"

#include <cstdio>

namespace widget {

/** @brief Renders the stock status bar. */
inline void status(display::device& display, platform::device& platform, const char* title,
                   const style& style = {}) {
  canvas::fill(display, {0, 0, display.width, 44}, style.background);
  canvas::hline(display, 0, 43, display.width, style.divider);
  text::draw(display, 14, 13, title, style.text_scale, style.foreground);
  char battery[16]{};
  std::snprintf(battery, sizeof(battery), "%d%%", platform::battery_percent(platform));
  text::draw(display, display.width - text::width(battery, style.text_scale) - 14, 13, battery,
             style.text_scale, style.foreground);
}

}  // namespace widget
