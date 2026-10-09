#pragma once

#include <algorithm>
#include <cstdio>

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

namespace app::pages::library {

inline bool open_selected_book(context& self) {
  const int index = selected_book_index(self);
  if (index < 0 || index >= static_cast<int>(self.reader.library.count)) return false;
  state::set(*self.memory, "reader.library.selected", static_cast<std::int64_t>(index));
  if (!::reader::open_selected(self.reader, *self.memory)) return false;
  routes::set_page(self, page::reader);
  return true;
}

inline bool show_book_info(context& self) {
  if (selected_book_index(self) < 0) return false;
  state::set(*self.memory, "app.dialog.book_info", true);
  state::set(*self.memory, "app.dialog.selected", std::int64_t{0});
  self.next_refresh = refresh::mode::quality;
  ++self.invalidations;
  return true;
}

inline bool activate_book_info(context& self) {
  const auto selected = state::get(*self.memory, "app.dialog.selected", std::int64_t{0});
  state::set(*self.memory, "app.dialog.book_info", false);
  self.next_refresh = refresh::mode::quality;
  ++self.invalidations;
  return selected == 0 ? open_selected_book(self) : true;
}

inline int list_offset(const context& self, int count) {
  const int visible = list_visible_rows(self);
  if (count <= visible) return 0;
  const int selected =
      static_cast<int>(state::get(*self.memory, "reader.library.selected", std::int64_t{0}));
  return (selected / visible) * visible;
}

inline void draw_cover(display::device& d, geometry::rect rect, const book::item& book, int index) {
  canvas::fill(d, rect, canvas::gray::light);
  canvas::border(d, rect, 2, canvas::gray::black);
  canvas::fill(d, {rect.x + 3, rect.y + 3, rect.w - 6, 11},
               index % 3 == 0 ? canvas::gray::black : canvas::gray::dark);
  char initial[2] = {book.title[0], '\0'};
  text::center(d, {rect.x + 2, rect.y + 16, rect.w - 4, rect.h - 18}, initial, 2,
               canvas::gray::black);
}

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  const bool favorites = current_page(self) == page::favorites;
  draw_status(self, favorites ? "Favorites" : "Library");
  const int count = visible_book_count(self);
  const int offset = list_offset(self, count);
  const int visible_rows = list_visible_rows(self);
  const int visible = std::min(visible_rows, count - offset);
  const bool focused = static_cast<focus_area>(state::get(
                           *self.memory, "app.focus.area",
                           static_cast<std::int64_t>(focus_area::content))) == focus_area::content;
  const int row_width = self.shell.display->width - 24 - (count > 10 ? 10 : 0);
  for (int row = 0; row < visible; ++row) {
    const int visible_index = offset + row;
    const int index = visible_book_index(self, visible_index);
    auto& book = self.reader.library.books[static_cast<std::size_t>(index)];
    const geometry::rect rr{4, 58 + row * 76, row_width, 72};
    const bool selected =
        visible_index ==
        static_cast<int>(state::get(*self.memory, "reader.library.selected", std::int64_t{0}));
    const bool inverted = selected && focused;
    const auto fg = inverted ? canvas::gray::white : canvas::gray::black;
    canvas::fill(d, rr, inverted ? canvas::gray::black : canvas::gray::white);
    if (selected && !focused)
      canvas::fill(d, {rr.x + 31, rr.y + 6, 3, rr.h - 12}, canvas::gray::black);
    if (!inverted) canvas::hline(d, rr.x, rr.y + rr.h - 1, rr.w, canvas::gray::light);
    char number[12]{};
    std::snprintf(number, sizeof(number), "%d", visible_index + 1);
    text::draw_in(d, rr.x + 2, rr.y + 22, 28, number, 2, fg);
    const geometry::rect cover{rr.x + 36, rr.y + 7, 42, 58};
    draw_cover(d, cover, book, index);
    const int text_x = cover.x + cover.w + 10;
    text::draw_in(d, text_x, rr.y + 12, rr.x + rr.w - text_x - 8, book.title.data(), 2, fg);
    char meta[64]{};
    std::snprintf(meta, sizeof(meta), "%.50s  |  %d%%", book.author.data(), book.progress);
    text::draw_in(d, text_x, rr.y + 42, rr.x + rr.w - text_x - 8, meta, 1,
                  inverted ? canvas::gray::white : canvas::gray::dark);
  }
  if (count > visible_rows) {
    const geometry::rect track{self.shell.display->width - 18, 58, 6, visible_rows * 76};
    canvas::fill(d, track, canvas::gray::white);
    canvas::vline(d, track.x + 3, track.y, track.h, canvas::gray::light);
    const int thumb_h = std::max(track.h * visible_rows / count, 16);
    const int thumb_y = track.y + (track.h - thumb_h) * offset / std::max(count - visible_rows, 1);
    canvas::fill(d, {track.x, thumb_y, 6, thumb_h}, canvas::gray::black);
  }
  if (count == 0) text::center(d, {0, 300, d.width, 80}, "No books", 3, canvas::gray::dark);
  const char* actions[] = {favorites ? "Remove" : "Favorite", "Delete", "Back"};
  if (static_cast<focus_area>(state::get(*self.memory, "app.focus.area",
                                         static_cast<std::int64_t>(focus_area::content))) ==
      focus_area::dock)
    draw_dock(self, actions, 3);

  if (!book_info_visible(self)) return;
  const int index = selected_book_index(self);
  if (index < 0) return;
  auto& book = self.reader.library.books[static_cast<std::size_t>(index)];
  draw_dialog(self, "BOOK INFO", book.title.data());
  text::draw_in(d, 60, 378, d.width - 120, book.author.data(), 2, canvas::gray::dark);
  char meta[64]{};
  std::snprintf(meta, sizeof(meta), "EPUB  |  %d%%", book.progress);
  text::draw_in(d, 60, 414, d.width - 120, meta, 1, canvas::gray::dark);
  canvas::border(d, {60, 452, d.width - 120, 14}, 1, canvas::gray::light);
  canvas::fill(d, {61, 453, (d.width - 122) * book.progress / 100, 12}, canvas::gray::dark);
  const int selected =
      static_cast<int>(state::get(*self.memory, "app.dialog.selected", std::int64_t{0}));
  geometry::rect left{70, 500, 180, 54};
  geometry::rect right{290, 500, 180, 54};
  canvas::fill(d, left, selected == 0 ? canvas::gray::black : canvas::gray::white);
  canvas::fill(d, right, selected == 1 ? canvas::gray::black : canvas::gray::white);
  canvas::border(d, left, 2, canvas::gray::black);
  canvas::border(d, right, 2, canvas::gray::black);
  text::center(d, left, book.progress ? "CONTINUE" : "READ", 2,
               selected == 0 ? canvas::gray::white : canvas::gray::black);
  text::center(d, right, "CLOSE", 2, selected == 1 ? canvas::gray::white : canvas::gray::black);
}

inline bool activate_dock(context& self) {
  const int action =
      static_cast<int>(state::get(*self.memory, "app.dock.selected", std::int64_t{0}));
  if (action == 2) {
    if (!routes::back(self)) routes::set_page(self, page::home);
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
  state::set(*self.memory, "reader.library.count",
             static_cast<std::int64_t>(self.reader.library.count));
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
      self.next_refresh = refresh::mode::quality;
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

  if (const int dock = dock_tap(self, value, 3); dock >= 0) {
    state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::dock));
    state::set(*self.memory, "app.dock.selected", static_cast<std::int64_t>(dock));
    return activate_dock(self);
  }

  const int count = visible_book_count(self);
  if (value.event_type == event::type::tap) {
    if (value.y < 58 || value.y >= 58 + count * 76) return false;
    state::set(*self.memory, "reader.library.selected",
               static_cast<std::int64_t>(list_offset(self, count) + (value.y - 58) / 76));
    return show_book_info(self);
  }
  if (value.event_type != event::type::key) return false;
  if (value.key == event::key_code::back) {
    if (!routes::back(self)) routes::set_page(self, page::home);
    return true;
  }

  const auto area = static_cast<focus_area>(
      state::get(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content)));
  if (area == focus_area::dock) {
    auto selected = state::get(*self.memory, "app.dock.selected", std::int64_t{0});
    if (value.key == event::key_code::up) {
      if (selected > 0)
        state::set(*self.memory, "app.dock.selected", selected - 1);
      else
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
    if (value.key == event::key_code::down) {
      if (selected < 2) {
        state::set(*self.memory, "app.dock.selected", selected + 1);
        ++self.invalidations;
      }
      return true;
    }
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
