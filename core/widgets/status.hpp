#pragma once

#include "core/platform.hpp"
#include "core/text.hpp"
#include "core/widgets/style.hpp"

#include <cstdio>
#include <algorithm>

namespace widget {

/** @brief Renders the stock status bar. */
inline void status(display::device& display, platform::device& platform, const char* title,
                   const style& style = {}) {
   canvas::fill(display, {0, 0, display.width, 44}, style.background);
   canvas::fill(display, {0, 42, display.width, 2}, style.divider);
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
     const int x = display.width - 16 - 3 - battery_width;
     canvas::fill(display, {x + battery_width + 3, 18, 3, 5}, style.foreground);
     canvas::border(display, {x, 14, battery_width, 13}, 2, style.foreground);
     canvas::fill(display, {x + 3, 17, (battery_width - 6) * battery_value / 100, 7}, style.foreground);
     const int percent_width = text::width(battery, 1);
     text::center(display, {x - percent_width - 6, 0, percent_width, 42}, battery, 1,
                  style.foreground);
   }
   const int clock_x = display.width - 16 - 3 - battery_width -
                       (battery_value >= 0 ? text::width(battery, 1) + 6 : 0) -
                       text::bold_width(clock) - 16;
   text::draw_bold(display, clock_x, 5, clock, style.foreground);
   text::draw_bold_in(display, 16, 5, std::max(clock_x - 24, 1), title, style.foreground);
}

}  // namespace widget
