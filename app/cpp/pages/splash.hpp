#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

namespace app::pages::splash {

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  canvas::fill(d, {202, 370, 64, 96}, canvas::gray::black);
  canvas::fill(d, {273, 370, 64, 96}, canvas::gray::black);
  for (int y : {387, 407, 429, 449}) {
    canvas::hline(d, 214, y, 40, canvas::gray::white);
    canvas::hline(d, 285, y, 40, canvas::gray::white);
  }
  text::draw_title(d, 212, 500, "X-Reader");
  text::center(d, {0, 544, d.width, 30}, "the ebook reader for e-ink", 2, canvas::gray::dark);
  text::center(d, {0, d.height - 70, d.width, 30}, "Loading library...", 2, canvas::gray::dark);
}

inline bool event(context&, const event::value&) {
  // Ignore input while the board settles; the timer performs the Home transition.
  return true;
}

inline void tick(context& self, std::uint32_t now_ms) {
  if (now_ms - self.splash_entered_ms >= 1500U) routes::set_page(self, page::home);
}

}  // namespace app::pages::splash
