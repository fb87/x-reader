#pragma once

#include "core/storage.hpp"
#include "reader/library.hpp"
#include "app/cpp/model.hpp"

namespace app {

inline void restore_persistent(context& self) {
  const char* integer_keys[] = {
      "reader.book.current",
      "reader.book.chapter",
      "reader.book.page",
      "reader.book.progress",
      "reader.library.selected",
      "reader.settings.font_size",
      "reader.settings.full_refresh_every",
      "reader.settings.sleep_timeout_minutes",
      "app.menu.selected",
      "app.focus.area",
      "app.dock.selected",
  };
  for (const char* key : integer_keys) {
    if (state::find(*self.persistent, key) != nullptr)
      state::set(*self.memory, key, state::get(*self.persistent, key, std::int64_t{0}));
  }
  const char* bool_keys[] = {"reader.settings.show_progress", "network.wifi.auto_connect"};
  for (const char* key : bool_keys) {
    if (state::find(*self.persistent, key) != nullptr)
      state::set(*self.memory, key, state::get(*self.persistent, key, false));
  }
  const char* string_keys[] = {"network.wifi.ssid", "system.language"};
  for (const char* key : string_keys) {
    if (state::find(*self.persistent, key) != nullptr)
      state::set(*self.memory, key, state::get(*self.persistent, key, ""));
  }
}

inline void checkpoint(context& self) {
  const char* integer_keys[] = {
      "reader.book.current",
      "reader.book.chapter",
      "reader.book.page",
      "reader.book.progress",
      "reader.library.selected",
      "reader.settings.font_size",
      "reader.settings.full_refresh_every",
      "reader.settings.sleep_timeout_minutes",
      "app.menu.selected",
      "app.focus.area",
      "app.dock.selected",
  };
  for (const char* key : integer_keys)
    state::set(*self.persistent, key, state::get(*self.memory, key, std::int64_t{0}));
  const char* bool_keys[] = {"reader.settings.show_progress", "network.wifi.auto_connect"};
  for (const char* key : bool_keys)
    state::set(*self.persistent, key, state::get(*self.memory, key, false));
  state::set(*self.persistent, "app.route.current",
             state::get(*self.memory, "app.route.current", "/splash"));
  state::set(*self.persistent, "network.wifi.ssid",
             state::get(*self.memory, "network.wifi.ssid", ""));
  state::set(*self.persistent, "system.language",
             state::get(*self.memory, "system.language", "en"));
}

inline bool scan_library(context& self) {
  auto* storage = capability::get<storage::device>(*self.capabilities);
  if (storage == nullptr) {
    state::set(*self.memory, "reader.library.count", std::int64_t{0});
    return false;
  }
  return library::scan(self.reader.library, *storage, *self.memory);
}


}  // namespace app
