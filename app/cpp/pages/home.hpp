#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

#include <cstdio>

namespace app::pages::home {

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  draw_status(self, "HOME");
  const auto current_book = state::get(*self.memory, "reader.book.current", std::int64_t{-1});
  widget::book_card(d, {18, 62, d.width - 36, 180}, "CONTINUE READING",
                    current_book >= 0 ? state::get(*self.memory, "reader.book.title", "BOOK")
                                      : "NO BOOK OPEN",
                    false);
  const int count = routes::menu_count(self, "home");
  for (int i = 0; i < count; ++i) {
    const auto* route = routes::menu_route(self, "home", i);
    if (route != nullptr) draw_row(self, i, 265 + i * 76, routes::menu_label(*route));
  }
  char stats[64]{};
  std::snprintf(stats, sizeof(stats), "%zu BOOKS", self.reader.library.count);
  text::draw(d, 24, d.height - 50, stats, 2, canvas::gray::dark);
}

inline bool event(context& self, const event::value& value) {
  const int count = routes::menu_count(self, "home");
  if (value.event_type == event::type::tap) {
    if (value.y < 265 || value.y >= 265 + count * 76) return false;
    const int selected = (value.y - 265) / 76;
    state::set(*self.memory, "app.menu.selected", static_cast<std::int64_t>(selected));
    const auto* route = routes::menu_route(self, "home", selected);
    return route != nullptr && routes::push(self, route->path);
  }
  if (value.event_type != event::type::key) return false;
  if (value.key == event::key_code::down) {
    move_selection(self, 1, count);
    return true;
  }
  if (value.key == event::key_code::up) {
    move_selection(self, -1, count);
    return true;
  }
  if (value.key == event::key_code::ok) {
    const auto* route = routes::menu_route(self, "home", static_cast<int>(selection(self)));
    return route != nullptr && routes::push(self, route->path);
  }
  return false;
}

}  // namespace app::pages::home
