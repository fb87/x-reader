#pragma once

#include "core/capability.hpp"
#include "core/router.hpp"
#include "core/event.hpp"
#include "core/shell.hpp"
#include "core/state.hpp"
#include "reader/session.hpp"

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

struct context {
  capability::registry* capabilities = nullptr;
  state::store* memory = nullptr;
  state::store* persistent = nullptr;
  shell::context shell{};
  reader::context reader{};
  router::context router{};
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

inline void bind_page(context& self, page_binding binding) {
  self.bound_page = binding;
  state::set(*self.memory, "app.menu.selected", std::int64_t{0});
  state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content));
  state::set(*self.memory, "app.dock.selected", std::int64_t{0});
  state::set(*self.memory, "app.dialog.book_info", false);
  ++self.invalidations;
}

inline void apply_page(context& self, page value) {
  clear_page_binding(self);
  state::set(*self.memory, "app.page.current", static_cast<std::int64_t>(value));
  state::set(*self.memory, "app.menu.selected", std::int64_t{0});
  state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content));
  state::set(*self.memory, "app.dock.selected", std::int64_t{0});
  state::set(*self.memory, "app.dialog.book_info", false);
  ++self.invalidations;
}

inline void move_selection(context& self, int delta, int count) {
  if (count <= 0) return;
  auto value = selection(self) + delta;
  if (value < 0) value = 0;
  if (value >= count) value = count - 1;
  state::set(*self.memory, "app.menu.selected", value);
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
  auto value = state::get(*self.memory, "reader.library.selected", std::int64_t{0}) + delta;
  if (value < 0) value = 0;
  if (count > 0 && value >= count) value = count - 1;
  state::set(*self.memory, "reader.library.selected", value);
  ++self.invalidations;
}

}  // namespace app
