#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

namespace app::pages::sleep {

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::black);
  const int cx = d.width / 2;
  const int cy = d.height / 2;
  canvas::border(d, {cx - 38, cy - 52, 76, 104}, 3, canvas::gray::white);
  canvas::hline(d, cx - 20, cy - 20, 40, canvas::gray::white);
  canvas::hline(d, cx - 20, cy - 4, 40, canvas::gray::white);
  canvas::hline(d, cx - 20, cy + 12, 28, canvas::gray::white);
  text::center(d, {0, cy + 130, d.width, 40}, "Sleeping", 2, canvas::gray::white);
}

inline bool event(context& self, const event::value& value) {
  if (value.event_type != event::type::key && value.event_type != event::type::tap) return false;
  routes::set_page(self, page::home);
  return true;
}

}  // namespace app::pages::sleep
