#pragma once

#include "app/cpp/main.hpp"
#include "app/cpp/routes.hpp"
#include "app/cpp/services.hpp"
#include "core/capability.hpp"
#include "core/display.hpp"
#include "core/input.hpp"
#include "core/platform.hpp"
#include "core/shell.hpp"
#include "enabled_plugins.hpp"

namespace app {

inline bool init(context& self, capability::registry& capabilities, state::store& memory,
                 state::store& persistent, bool native_ui = true) {
  self.capabilities = &capabilities;
  self.memory = &memory;
  self.persistent = &persistent;
  auto* display = capability::get<display::device>(capabilities);
  auto* input = capability::get<input::device>(capabilities);
  auto* platform_device = capability::get<platform::device>(capabilities);
  auto* storage_device = capability::get<storage::device>(capabilities);
  if (display == nullptr || input == nullptr || platform_device == nullptr) return false;
  const bool woke_from_sleep = platform::woke_from_deep_sleep(*platform_device);
  self.reader.storage = storage_device;

  restore_persistent(self);
  reader::init(self.reader, memory);
  if (!routes::register_all(self)) return false;
  if (!generated::plugins::init(self)) return false;
  if (state::find(memory, "reader.settings.sleep_timeout_minutes") == nullptr)
    state::set(memory, "reader.settings.sleep_timeout_minutes", std::int64_t{10});

  apply_page(self, page::splash);
  (void)routes::replace(self, "/splash");
  state::set(memory, "app.menu.selected", std::int64_t{0});
  state::set(memory, "app.reader.chrome", false);
  state::set(memory, "app.dialog.book_info", false);
  state::set(memory, "app.dialog.about", false);
  state::set(memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content));
  shell::init(self.shell, *display, *input, *platform_device, native_ui ? on_event : nullptr,
              native_ui ? static_cast<void*>(&self) : nullptr, on_shell_event,
              static_cast<void*>(&self));
  self.last_input_ms = platform::now_ms(*platform_device);
  self.splash_entered_ms = platform::now_ms(*platform_device);
  (void)scan_library(self);
  if (woke_from_sleep) {
    const char* return_route = state::get(memory, "app.sleep.return_route", "");
    const char* route = return_route;
    if (route != nullptr && std::strncmp(route, "/book/", 6) == 0) {
      const auto current = state::get(memory, "reader.book.current", std::int64_t{-1});
      state::set(memory, "reader.library.selected", current);
      if (!reader::open_selected(self.reader, memory)) route = "/";
    }
    if (route != nullptr && route[0] != '\0' && std::strcmp(route, "/sleep") != 0)
      routes::replace(self, route);
    state::set(memory, "app.sleep.return_route", "");
  }
  if (native_ui) render(self);
  return true;
}

}  // namespace app
