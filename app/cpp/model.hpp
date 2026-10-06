#pragma once

#include "core/capability.hpp"
#include "core/router.hpp"
#include "core/event.hpp"
#include "core/shell.hpp"
#include "core/state.hpp"
#include "core/storage.hpp"
#include "core/refresh.hpp"
#include "reader/session.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace app {

/** @brief Compatibility page IDs retained for durable state and cross-language tests. */
enum class page : std::int64_t {
  splash = 0,
  home,
  library,
  favorites,
  files,
  reader,
  settings,
  connectivity,
  sleep,
};

/** @brief Bottom action-bar focus state. */
enum class focus_area : std::int64_t { content = 0, dock = 1 };

/** @brief Board-independent application context shared by all native pages. */
struct context;

struct page_binding {
  void (*render)(context&) = nullptr;
  bool (*event)(context&, const ::event::value&) = nullptr;
  void (*tick)(context&, std::uint32_t) = nullptr;
};

inline constexpr std::size_t file_entry_capacity = 64;

struct file_entry {
  char name[128]{};
  char display[128]{};
  bool directory = false;
};

struct context {
  capability::registry* capabilities = nullptr;
  state::store* memory = nullptr;
  state::store* persistent = nullptr;
  shell::context shell{};
  reader::context reader{};
  router::context router{};
  std::array<file_entry, file_entry_capacity> files{};
  char file_path[storage::path_max]{};
  int file_count = 0;
  refresh::mode next_refresh = refresh::mode::full;
  geometry::rect next_rect{};
  bool has_next_rect = false;
  page_binding bound_page{};
  std::uint32_t splash_entered_ms = 0;
  int invalidations = 0;
};

inline page current_page(const context& self) {
  return static_cast<page>(
      state::get(*self.memory, "app.page.current", static_cast<std::int64_t>(page::splash)));
}

inline std::int64_t selection(const context& self) {
  return state::get(*self.memory, "app.menu.selected", std::int64_t{0});
}

inline bool book_info_visible(const context& self) {
  return state::get(*self.memory, "app.dialog.book_info", false);
}

inline void clear_page_binding(context& self) { self.bound_page = {}; }

inline void schedule_rect(context& self, geometry::rect rect, refresh::mode mode) {
  if (!self.has_next_rect) {
    self.next_rect = rect;
    self.has_next_rect = true;
  } else {
    const int right = std::max(self.next_rect.x + self.next_rect.w, rect.x + rect.w);
    const int bottom = std::max(self.next_rect.y + self.next_rect.h, rect.y + rect.h);
    self.next_rect.x = std::min(self.next_rect.x, rect.x);
    self.next_rect.y = std::min(self.next_rect.y, rect.y);
    self.next_rect.w = right - self.next_rect.x;
    self.next_rect.h = bottom - self.next_rect.y;
  }
  self.next_refresh = mode;
}

inline void bind_page(context& self, page_binding binding) {
  self.bound_page = binding;
  state::set(*self.memory, "app.menu.selected", std::int64_t{0});
  state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content));
  state::set(*self.memory, "app.dock.selected", std::int64_t{0});
  state::set(*self.memory, "app.dialog.book_info", false);
  self.next_refresh = refresh::mode::full;
  self.has_next_rect = false;
  ++self.invalidations;
}

inline void apply_page(context& self, page value) {
  clear_page_binding(self);
  state::set(*self.memory, "app.page.current", static_cast<std::int64_t>(value));
  state::set(*self.memory, "app.menu.selected", std::int64_t{0});
  state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content));
  state::set(*self.memory, "app.dock.selected", std::int64_t{0});
  state::set(*self.memory, "app.dialog.book_info", false);
  self.next_refresh = refresh::mode::full;
  self.has_next_rect = false;
  ++self.invalidations;
}

inline void move_selection(context& self, int delta, int count) {
  if (count <= 0) return;
  const auto old = selection(self);
  auto value = selection(self) + delta;
  if (value < 0) value = 0;
  if (value >= count) value = count - 1;
  state::set(*self.memory, "app.menu.selected", value);
  if (current_page(self) == page::home) {
    const int old_y = 265 + static_cast<int>(old) * 76;
    const int new_y = 265 + static_cast<int>(value) * 76;
    schedule_rect(self, {0, std::min(old_y, new_y), 540, 152}, refresh::mode::fast);
  } else {
    self.next_refresh = refresh::mode::fast;
    self.has_next_rect = false;
  }
  ++self.invalidations;
}

inline int visible_book_count(const context& self) {
  if (current_page(self) != page::favorites) return static_cast<int>(self.reader.library.count);
  int count = 0;
  for (std::size_t i = 0; i < self.reader.library.count; ++i)
    if (self.reader.library.books[i].favorite) ++count;
  return count;
}

inline int visible_book_index(const context& self, int visible_index) {
  if (current_page(self) != page::favorites) return visible_index;
  int seen = 0;
  for (std::size_t i = 0; i < self.reader.library.count; ++i) {
    if (!self.reader.library.books[i].favorite) continue;
    if (seen++ == visible_index) return static_cast<int>(i);
  }
  return -1;
}

inline int selected_book_index(const context& self) {
  return visible_book_index(
      self, static_cast<int>(state::get(*self.memory, "reader.library.selected", std::int64_t{0})));
}

inline void move_library(context& self, int delta) {
  const int count = visible_book_count(self);
  const auto old = state::get(*self.memory, "reader.library.selected", std::int64_t{0});
  auto value = old + delta;
  if (value < 0) value = 0;
  if (count > 0 && value >= count) value = count - 1;
  state::set(*self.memory, "reader.library.selected", value);
  schedule_rect(self, {0, 58 + static_cast<int>(std::min(old, value)) * 76,
                       540, 152}, refresh::mode::fast);
  ++self.invalidations;
}

}  // namespace app
