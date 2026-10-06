#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

#include <cstdio>

namespace app::pages::library {

inline bool open_selected_book(context& self) {
  const int index = selected_book_index(self);
  if (index < 0 || index >= static_cast<int>(self.reader.library.count)) return false;
  state::set(*self.memory, "reader.library.selected", static_cast<std::int64_t>(index));
  if (!reader::open_selected(self.reader, *self.memory)) return false;
  routes::set_page(self, page::reader);
  return true;
}

inline bool show_book_info(context& self) {
  if (selected_book_index(self) < 0) return false;
  state::set(*self.memory, "app.dialog.book_info", true);
  state::set(*self.memory, "app.dialog.selected", std::int64_t{0});
  ++self.invalidations;
  return true;
}

inline bool activate_book_info(context& self) {
  const auto selected = state::get(*self.memory, "app.dialog.selected", std::int64_t{0});
  state::set(*self.memory, "app.dialog.book_info", false);
  ++self.invalidations;
  return selected == 0 ? open_selected_book(self) : true;
}

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  const bool favorites = current_page(self) == page::favorites;
  draw_status(self, favorites ? "FAVORITES" : "LIBRARY");
  const int count = visible_book_count(self);
  for (int row = 0; row < count && row < 10; ++row) {
    const int index = visible_book_index(self, row);
    auto& book = self.reader.library.books[static_cast<std::size_t>(index)];
    char meta[64]{};
    std::snprintf(meta, sizeof(meta), "%s  %d%%", book.favorite ? "FAV" : "EPUB", book.progress);
    draw_row(self, row, 58 + row * 76, book.title.data(), meta);
  }
  if (count == 0) text::center(d, {0, 300, d.width, 80}, "NO BOOKS", 3, canvas::gray::dark);
  const char* actions[] = {favorites ? "REMOVE" : "FAVORITE", "DELETE", "BACK"};
  draw_dock(self, actions, 3);

  if (!book_info_visible(self)) return;
  const int index = selected_book_index(self);
  if (index < 0) return;
  auto& book = self.reader.library.books[static_cast<std::size_t>(index)];
  draw_dialog(self, "BOOK INFO", book.title.data());
  const int selected = static_cast<int>(
      state::get(*self.memory, "app.dialog.selected", std::int64_t{0}));
  geometry::rect left{70, 500, 180, 54};
  geometry::rect right{290, 500, 180, 54};
  canvas::fill(d, left, selected == 0 ? canvas::gray::black : canvas::gray::white);
  canvas::fill(d, right, selected == 1 ? canvas::gray::black : canvas::gray::white);
  canvas::border(d, left, 2, canvas::gray::black);
  canvas::border(d, right, 2, canvas::gray::black);
  text::center(d, left, book.progress ? "CONTINUE" : "READ", 2,
               selected == 0 ? canvas::gray::white : canvas::gray::black);
  text::center(d, right, "CLOSE", 2,
               selected == 1 ? canvas::gray::white : canvas::gray::black);
}

inline bool activate_dock(context& self) {
  const int action = static_cast<int>(
      state::get(*self.memory, "app.dock.selected", std::int64_t{0}));
  if (action == 2) {
    routes::set_page(self, page::home);
    return true;
  }
  const int index = selected_book_index(self);
  if (index < 0) return false;
  auto& item = self.reader.library.books[static_cast<std::size_t>(index)];
  if (action == 0) {
    item.favorite = current_page(self) == page::favorites ? false : !item.favorite;
    ++self.invalidations;
    return true;
  }
  ::library::remove(self.reader.library, static_cast<std::size_t>(index));
  state::set(*self.memory, "reader.library.count", static_cast<std::int64_t>(self.reader.library.count));
  state::set(*self.memory, "reader.library.selected", std::int64_t{0});
  ++self.invalidations;
  return true;
}

inline bool event(context& self, const event::value& value) {
  if (book_info_visible(self)) {
    if (value.event_type == event::type::key) {
      if (value.key == event::key_code::left || value.key == event::key_code::up)
        state::set(*self.memory, "app.dialog.selected", std::int64_t{0});
      else if (value.key == event::key_code::right || value.key == event::key_code::down)
        state::set(*self.memory, "app.dialog.selected", std::int64_t{1});
      else if (value.key == event::key_code::ok)
        return activate_book_info(self);
      else if (value.key == event::key_code::back)
        state::set(*self.memory, "app.dialog.book_info", false);
      ++self.invalidations;
      return true;
    }
    if (value.event_type == event::type::tap) {
      state::set(*self.memory, "app.dialog.selected",
                 value.x < self.shell.display->width / 2 ? std::int64_t{0} : std::int64_t{1});
      return activate_book_info(self);
    }
    return true;
  }

  const int count = visible_book_count(self);
  if (value.event_type == event::type::tap) {
    if (value.y < 58 || value.y >= 58 + count * 76) return false;
    state::set(*self.memory, "reader.library.selected",
               static_cast<std::int64_t>((value.y - 58) / 76));
    return show_book_info(self);
  }
  if (value.event_type != event::type::key) return false;
  if (value.key == event::key_code::back) {
    routes::set_page(self, page::home);
    return true;
  }

  const auto area = static_cast<focus_area>(state::get(
      *self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content)));
  if (area == focus_area::dock) {
    auto selected = state::get(*self.memory, "app.dock.selected", std::int64_t{0});
    if (value.key == event::key_code::up) {
      state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content));
      ++self.invalidations;
      return true;
    }
    if (value.key == event::key_code::left && selected > 0) {
      state::set(*self.memory, "app.dock.selected", selected - 1);
      ++self.invalidations;
      return true;
    }
    if (value.key == event::key_code::right && selected < 2) {
      state::set(*self.memory, "app.dock.selected", selected + 1);
      ++self.invalidations;
      return true;
    }
    if (value.key == event::key_code::down) return true;
    if (value.key == event::key_code::ok) return activate_dock(self);
  }

  if (value.key == event::key_code::down) {
    const auto before = state::get(*self.memory, "reader.library.selected", std::int64_t{0});
    move_library(self, 1);
    if (before == state::get(*self.memory, "reader.library.selected", std::int64_t{0})) {
      state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::dock));
      ++self.invalidations;
    }
    return true;
  }
  if (value.key == event::key_code::up) {
    move_library(self, -1);
    return true;
  }
  if (value.key == event::key_code::ok) return show_book_info(self);
  return false;
}

}  // namespace app::pages::library
