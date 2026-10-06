#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/pages/library.hpp"
#include "app/cpp/routes.hpp"

#include <cstdio>

namespace app::pages::files {

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  draw_status(self, "FILE MANAGER");
  const int count = static_cast<int>(self.reader.library.count);
  for (int row = 0; row < count && row < 10; ++row) {
    auto& book = self.reader.library.books[static_cast<std::size_t>(row)];
    char meta[64]{};
    std::snprintf(meta, sizeof(meta), "EPUB  %d%%", book.progress);
    draw_row(self, row, 58 + row * 76, book.title.data(), meta);
  }
  if (count == 0) text::center(d, {0, 300, d.width, 80}, "NO BOOKS", 3, canvas::gray::dark);
  const char* actions[] = {"UP / BACK"};
  draw_dock(self, actions, 1);
}

inline bool event(context& self, const event::value& value) {
  const int count = static_cast<int>(self.reader.library.count);
  if (value.event_type == event::type::tap) {
    if (value.y < 58 || value.y >= 58 + count * 76) return false;
    state::set(*self.memory, "reader.library.selected",
               static_cast<std::int64_t>((value.y - 58) / 76));
    return library::open_selected_book(self);
  }
  if (value.event_type != event::type::key) return false;
  if (value.key == event::key_code::back) {
    routes::set_page(self, page::home);
    return true;
  }
  if (value.key == event::key_code::down) {
    move_library(self, 1);
    return true;
  }
  if (value.key == event::key_code::up) {
    move_library(self, -1);
    return true;
  }
  if (value.key == event::key_code::ok) return library::open_selected_book(self);
  return false;
}

}  // namespace app::pages::files
