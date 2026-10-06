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
  int hour = 0;
  int minute = 0;
  const bool have_time = platform::wall_time(platform, hour, minute);
  char clock[8]{};
  std::snprintf(clock, sizeof(clock), have_time ? "%02d:%02d" : "--:--", hour % 24,
                minute % 60);
  char battery[16]{};
  const int battery_value = platform::battery_percent(platform);
  const int battery_width = battery_value >= 0 ? 26 : 0;
  if (battery_value >= 0) {
    std::snprintf(battery, sizeof(battery), "%d%%", battery_value);
    const int x = display.width - 14 - 3 - battery_width;
    canvas::fill(display, {x + battery_width + 3, 17, 3, 5}, style.foreground);
    canvas::border(display, {x, 15, battery_width, 13}, 2, style.foreground);
    canvas::fill(display, {x + 3, 18, (battery_width - 6) * battery_value / 100, 7}, style.foreground);
    text::draw(display, x - text::width(battery, style.text_scale) - 6, 13, battery,
               style.text_scale, style.foreground);
  }
  const int clock_x = display.width - 14 - 3 - battery_width -
                      (battery_value >= 0 ? text::width(battery, style.text_scale) + 6 : 0) -
                      text::width(clock, style.text_scale);
  text::draw_in(display, clock_x, 13, text::width(clock, style.text_scale), clock,
                style.text_scale, style.foreground);
  text::draw_in(display, 14, 13, clock_x - 28, title, style.text_scale, style.foreground);
}

}  // namespace widget
