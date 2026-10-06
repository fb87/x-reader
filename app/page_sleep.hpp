#pragma once

#include "pages_fwd.hpp"

/** @brief The deep-sleep placeholder page, ported from app/page_sleep.c. */
namespace app {

struct sleep_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
};

inline sleep_page& sleep_of(page::context& p) {
  return *reinterpret_cast<sleep_page*>(reinterpret_cast<char*>(&p) - offsetof(sleep_page, base));
}

inline void sleep_render(page::context& p, canvas::surface& c) {
  const shell::theme& t = theme_of(p);
  const geometry::rect a = p.area;
  const int cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  canvas::fill(c, a, canvas::gray::black);
  canvas::border(c, geometry::make(cx - 38, cy - 52, 76, 104), 3, canvas::gray::white);
  canvas::hline(c, cx - 20, cy - 20, 40, canvas::gray::white);
  canvas::hline(c, cx - 20, cy - 4, 40, canvas::gray::white);
  canvas::hline(c, cx - 20, cy + 12, 28, canvas::gray::white);
  canvas::draw_text_in(c, *t.font_small, geometry::make(a.x, cy + 130, a.w, 30), "Sleeping",
                       text::align::center, canvas::gray::white);
}

inline bool sleep_event(page::context& p, const event::value& ev) {
  if (ev.kind == event::type::key || ev.kind == event::type::tap) {
    sleep_page& sp = sleep_of(p);
    shell::replace(*p.shell, *page_home(*sp.app, *sp.nav));
    return true;
  }
  return false;
}

inline constexpr page::vtbl sleep_vtbl{
    nullptr, nullptr, nullptr, nullptr, nullptr, sleep_render, sleep_event, nullptr, nullptr,
};

}  // namespace app
