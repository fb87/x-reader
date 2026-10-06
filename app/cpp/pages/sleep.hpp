#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

namespace app::pages::sleep {

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::black);
  text::center(d, {0, 400, d.width, 100}, "SLEEPING", 4, canvas::gray::white);
}

inline bool event(context& self, const event::value& value) {
  if (value.event_type != event::type::key && value.event_type != event::type::tap) return false;
  routes::set_page(self, page::home);
  return true;
}

}  // namespace app::pages::sleep
