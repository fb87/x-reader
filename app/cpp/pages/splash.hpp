#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

namespace app::pages::splash {

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  text::center(d, {0, 320, d.width, 80}, "Reader", 5);
  text::center(d, {0, 410, d.width, 40}, "The ebook reader for e-ink", 2, canvas::gray::dark);
  text::center(d, {0, d.height - 90, d.width, 30}, "Loading library...", 2, canvas::gray::dark);
}

inline bool event(context& self, const event::value&) {
  routes::set_page(self, page::home);
  return true;
}

inline void tick(context& self, std::uint32_t now_ms) {
  if (now_ms - self.splash_entered_ms >= 1500U) routes::set_page(self, page::home);
}

}  // namespace app::pages::splash
