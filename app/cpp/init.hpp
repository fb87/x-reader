#pragma once

#include "core/capability.hpp"
#include "core/display.hpp"
#include "core/input.hpp"
#include "core/platform.hpp"
#include "core/shell.hpp"
#include "app/cpp/main.hpp"
#include "app/cpp/routes.hpp"
#include "app/cpp/services.hpp"
#include "enabled_plugins.hpp"

namespace app {

inline bool init(context& self, capability::registry& capabilities, state::store& memory,
                 state::store& persistent, bool native_ui = true) {
  const char* restored_route = state::get(persistent, "app.route.current", "");
  const auto restored_menu = state::get(persistent, "app.menu.selected", std::int64_t{0});
  const auto restored_focus = state::get(persistent, "app.focus.area", std::int64_t{0});
  const auto restored_dock = state::get(persistent, "app.dock.selected", std::int64_t{0});
  self.capabilities = &capabilities;
  self.memory = &memory;
  self.persistent = &persistent;
  auto* display = capability::get<display::device>(capabilities);
  auto* input = capability::get<input::device>(capabilities);
  auto* platform_device = capability::get<platform::device>(capabilities);
  auto* storage_device = capability::get<storage::device>(capabilities);
  if (display == nullptr || input == nullptr || platform_device == nullptr) return false;
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
  self.splash_entered_ms = platform::now_ms(*platform_device);
  (void)scan_library(self);
  if (restored_route[0] != '\0' && std::strcmp(restored_route, "/splash") != 0) {
    const char* route = std::strcmp(restored_route, "/book/0/reader") == 0 ? "/library" : restored_route;
    routes::replace(self, route);
    state::set(*self.memory, "app.menu.selected", restored_menu);
    state::set(*self.memory, "app.focus.area", restored_focus);
    state::set(*self.memory, "app.dock.selected", restored_dock);
  }
  if (native_ui) render(self);
  return true;
}

}  // namespace app
