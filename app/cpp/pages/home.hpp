#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/pages/library.hpp"
#include "app/cpp/routes.hpp"

#include <cstdio>
#include <cstring>

namespace app::pages::home {

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  draw_status(self, "Home");
  const auto current_book = state::get(*self.memory, "reader.book.current", std::int64_t{-1});
  const geometry::rect card{16, 68, d.width - 32, 190};
  const bool card_focused = state::get(*self.memory, "app.home.card.focused", false);
  canvas::border(d, card, card_focused ? 4 : 2, canvas::gray::black);
  text::draw_in(d, card.x + 20, card.y + 20, card.w - 40, "Continue Reading", 1,
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
    text::draw_in(d, card.x + 20, card.y + 59, card.w - 40, "No book open", 2,
                  canvas::gray::black);
  }
  const int count = routes::menu_count(self, "home");
  int rendered = 0;
  for (int i = 0; i < count; ++i) {
    const auto* route = routes::menu_route(self, "home", i);
    if (route != nullptr) {
      const char* label = routes::menu_label(*route);
      if (std::strcmp(route->title_key, "sleep") == 0) continue;
      const bool selected = !card_focused && selection(self) == i;
      const geometry::rect row{16, 290 + rendered++ * 72, d.width - 32, 63};
      widget::row(d, row, label, nullptr, selected, {}, icon::for_label(label));
      canvas::border(d, row, 2, canvas::gray::black);
    }
  }
  canvas::border(d, {412, d.height - 58, 112, 34}, 2, canvas::gray::black);
  icon::draw(d, {422, d.height - 51, 24, 24}, XR_ICON_CLOSE);
  text::draw(d, 454, d.height - 49, "Sleep", 1, canvas::gray::black);
  int in_progress = 0;
  for (std::size_t i = 0; i < self.reader.library.count; ++i) {
    if (self.reader.library.books[i].progress > 0 && self.reader.library.books[i].progress < 100)
      ++in_progress;
  }
  char stats[64]{};
  std::snprintf(stats, sizeof(stats), "%zu books  |  %d in progress", self.reader.library.count,
                in_progress);
  text::center(d, {0, d.height - 58, d.width, 34}, stats, 1, canvas::gray::dark);
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
    if (value.y >= 68 && value.y < 258) return open_current_book(self);
    if (value.y >= self.shell.display->height - 58 && value.x >= 400)
      return routes::push(self, "/sleep");
    if (value.y < 290 || value.y >= 290 + count * 72) return false;
    const int selected = (value.y - 290) / 72;
    state::set(*self.memory, "app.menu.selected", static_cast<std::int64_t>(selected));
    const auto* route = routes::menu_route(self, "home", selected);
    return route != nullptr && routes::push(self, route->path);
  }
  if (value.event_type != event::type::key) return false;
  if (value.key == event::key_code::down) {
    if (state::get(*self.memory, "app.home.card.focused", false)) {
      state::set(*self.memory, "app.home.card.focused", false);
      ++self.invalidations;
      return true;
    }
    move_selection(self, 1, count);
    return true;
  }
  if (value.key == event::key_code::up) {
    if (selection(self) == 0) {
      state::set(*self.memory, "app.home.card.focused", true);
      ++self.invalidations;
      return true;
    }
    move_selection(self, -1, count);
    return true;
  }
  if (value.key == event::key_code::ok) {
    if (state::get(*self.memory, "app.home.card.focused", false)) return open_current_book(self);
    const auto* route = routes::menu_route(self, "home", static_cast<int>(selection(self)));
    return route != nullptr && routes::push(self, route->path);
  }
  return false;
}

}  // namespace app::pages::home
