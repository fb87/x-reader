#pragma once

#include "core/canvas.hpp"
#include "core/display.hpp"
#include "core/event.hpp"
#include "core/platform.hpp"
#include "core/refresh.hpp"
#include "core/shell.hpp"
#include "app/cpp/model.hpp"
#include "app/cpp/routes.hpp"
#include "app/cpp/pages/all.hpp"
#include "enabled_plugins.hpp"

namespace app {


inline bool on_shell_event(const event::value& value, void* user) {
  if (value.event_type != event::type::key || !value.long_press) return false;
  auto& self = *static_cast<context*>(user);
  state::set(*self.memory, "app.input.long_press", static_cast<std::int64_t>(value.key));
  state::set(*self.memory, "app.input.long_press_duration_ms",
             static_cast<std::int64_t>(value.duration_ms));
  if (value.key == event::key_code::back) {
    (void)routes::replace(self, "/");
  }
  // Long presses are shell-reserved. Once classified here they never leak to
  // dialog, dock, plugin, or active-page handlers.
  return true;
}

inline void render(context& self) {
  if (self.bound_page.render != nullptr) {
    self.bound_page.render(self);
  } else switch (current_page(self)) {
    case page::splash: pages::splash::render(self); break;
    case page::home: pages::home::render(self); break;
    case page::library: pages::library::render(self); break;
    case page::favorites: pages::favorites::render(self); break;
    case page::files: pages::files::render(self); break;
    case page::reader: pages::reader::render(self); break;
    case page::settings: pages::settings::render(self); break;
    case page::connectivity: break;
    case page::sleep: pages::sleep::render(self); break;
  }
  auto& d = *self.shell.display;
  display::update(d, {0, 0, d.width, d.height}, refresh::mode::quality);
}

inline bool on_event(const event::value& value, void* user) {
  auto& self = *static_cast<context*>(user);
  if (self.bound_page.event != nullptr) return self.bound_page.event(self, value);
  switch (current_page(self)) {
    case page::splash: return pages::splash::event(self, value);
    case page::home: return pages::home::event(self, value);
    case page::library: return pages::library::event(self, value);
    case page::favorites: return pages::favorites::event(self, value);
    case page::files: return pages::files::event(self, value);
    case page::reader: return pages::reader::event(self, value);
    case page::settings: return pages::settings::event(self, value);
    case page::connectivity: return false;
    case page::sleep: return pages::sleep::event(self, value);
  }
  return false;
}

inline void tick(context& self) {
  const auto now = self.shell.platform != nullptr ? platform::now_ms(*self.shell.platform) : 0;
  if (self.bound_page.tick != nullptr) self.bound_page.tick(self, now);
  if (self.bound_page.render == nullptr && current_page(self) == page::splash && self.shell.platform != nullptr)
    pages::splash::tick(self, platform::now_ms(*self.shell.platform));
}

inline void pump(context& self) {
  const int before = self.invalidations;
  tick(self);
  shell::pump(self.shell);
  if (self.invalidations != before) render(self);
}

}  // namespace app
