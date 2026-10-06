#pragma once

#include "pages_fwd.hpp"

/** @brief The splash page shown while the library scan runs, ported from app/page_splash.c. */
namespace app {

struct splash_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
  std::uint32_t entered_ms = 0;
  widget::label name{}, tagline{}, status{};
};

inline splash_page& splash_of(page::context& p) {
  return *reinterpret_cast<splash_page*>(reinterpret_cast<char*>(&p) - offsetof(splash_page, base));
}

inline void splash_create(page::context& p) {
  splash_page& sp = splash_of(p);
  const shell::theme& t = theme_of(p);
  const geometry::rect a = p.area;
  const int cy = a.y + a.h / 2;
  widget::init(sp.name, geometry::make(a.x, cy + 20, a.w, 40), "X-Reader", *t.font_title,
               text::align::center);
  widget::init(sp.tagline, geometry::make(a.x, cy + 62, a.w, 28), "the ebook reader for e-ink",
               *t.font_normal, text::align::center);
  sp.tagline.gray = canvas::gray::dark;
  widget::init(sp.status, geometry::make(a.x, a.y + a.h - 70, a.w, 24), "Loading library...",
               *t.font_small, text::align::center);
  sp.status.gray = canvas::gray::dark;
  page::add(p, sp.name.base_widget);
  page::add(p, sp.tagline.base_widget);
  page::add(p, sp.status.base_widget);
}

inline void splash_render(page::context& p, canvas::surface& c) {
  const geometry::rect a = p.area;
  const int cx = a.x + a.w / 2;
  const int top = a.y + a.h / 2 - 110;
  const int pw = 64, ph = 96;
  canvas::fill(c, geometry::make(cx - pw - 4, top, pw, ph), canvas::gray::black);
  canvas::fill(c, geometry::make(cx + 4, top, pw, ph), canvas::gray::black);
  for (int i = 0; i < 5; ++i) {
    const int y = top + 18 + i * 14;
    canvas::fill(c, geometry::make(cx - pw + 8, y, pw - 24, 4), canvas::gray::white);
    canvas::fill(c, geometry::make(cx + 16, y, pw - 24, 4), canvas::gray::white);
  }
  widget::render_tree(p.scope.root, c);
}

inline void splash_enter(page::context& p) { splash_of(p).entered_ms = shell::now(*p.shell); }

inline void splash_tick(page::context& p, std::uint32_t now) {
  splash_page& sp = splash_of(p);
  if (now - sp.entered_ms >= 1500) shell::replace(*p.shell, *page_home(*sp.app, *sp.nav));
}

inline constexpr page::vtbl splash_vtbl{
    splash_create, nullptr, splash_enter, nullptr,     nullptr,
    splash_render, nullptr, nullptr,      splash_tick,
};

}  // namespace app
