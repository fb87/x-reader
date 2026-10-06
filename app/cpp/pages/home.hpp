#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/pages/library.hpp"
#include "app/cpp/routes.hpp"

#include <cstdio>

namespace app::pages::home {

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  draw_status(self, "HOME");
  const auto current_book = state::get(*self.memory, "reader.book.current", std::int64_t{-1});
  const geometry::rect card{18, 62, d.width - 36, 190};
  canvas::border(d, card, 3, canvas::gray::black);
  text::draw_in(d, card.x + 16, card.y + 20, card.w - 32, "CONTINUE READING", 1,
                canvas::gray::dark);
  if (current_book >= 0 && current_book < static_cast<std::int64_t>(self.reader.library.count)) {
    const auto& book = self.reader.library.books[static_cast<std::size_t>(current_book)];
    const geometry::rect cover{card.x + 20, card.y + 52, 82, 112};
    canvas::fill(d, cover, canvas::gray::light);
    canvas::border(d, cover, 2, canvas::gray::black);
    canvas::fill(d, {cover.x + 5, cover.y + 5, cover.w - 10, 16}, canvas::gray::black);
    char initial[2] = {book.title[0], '\0'};
    text::center(d, {cover.x + 4, cover.y + 28, cover.w - 8, cover.h - 32}, initial, 2,
                 canvas::gray::black);

    const int text_x = cover.x + cover.w + 14;
    const int text_w = card.x + card.w - text_x - 18;
    text::draw_in(d, text_x, cover.y + 2, text_w, book.title.data(), 3, canvas::gray::black);
    text::draw_in(d, text_x, cover.y + 48, text_w, book.author.data(), 2, canvas::gray::dark);
    char percent[8]{};
    std::snprintf(percent, sizeof(percent), "%d%%", book.progress);
    const int percent_w = text::width(percent, 2);
    const int progress_w = text_w - percent_w - 10;
    canvas::border(d, {text_x, cover.y + 88, progress_w, 10}, 1, canvas::gray::light);
    canvas::fill(d, {text_x + 1, cover.y + 89, (progress_w - 2) * book.progress / 100, 8},
                 canvas::gray::dark);
    text::draw_in(d, text_x + progress_w + 10, cover.y + 84, percent_w, percent, 2,
                  canvas::gray::black);
  } else {
    text::draw_in(d, card.x + 20, card.y + 78, card.w - 40, "NO BOOK OPEN", 3,
                  canvas::gray::black);
  }
  const int count = routes::menu_count(self, "home");
  for (int i = 0; i < count; ++i) {
    const auto* route = routes::menu_route(self, "home", i);
    if (route != nullptr) draw_row(self, i, 265 + i * 76, routes::menu_label(*route));
  }
  int in_progress = 0;
  for (std::size_t i = 0; i < self.reader.library.count; ++i) {
    if (self.reader.library.books[i].progress > 0 && self.reader.library.books[i].progress < 100)
      ++in_progress;
  }
  char stats[64]{};
  std::snprintf(stats, sizeof(stats), "%zu BOOKS  |  %d IN PROGRESS", self.reader.library.count,
                in_progress);
  text::draw(d, 24, d.height - 50, stats, 2, canvas::gray::dark);
}

inline bool open_current_book(context& self) {
  const auto current_book = state::get(*self.memory, "reader.book.current", std::int64_t{-1});
  if (current_book < 0 || current_book >= static_cast<std::int64_t>(self.reader.library.count))
    return false;
  state::set(*self.memory, "reader.library.selected", current_book);
  if (!::reader::open_selected(self.reader, *self.memory)) return false;
  routes::set_page(self, page::reader);
  return true;
}

inline bool event(context& self, const event::value& value) {
  const int count = routes::menu_count(self, "home");
  if (value.event_type == event::type::tap) {
    if (value.y >= 62 && value.y < 252) return open_current_book(self);
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
