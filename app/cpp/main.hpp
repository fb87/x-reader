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
  self.last_input_ms = platform::now_ms(*self.shell.platform);
  if (value.key == event::key_code::ok) {
    routes::set_page(self, page::sleep);
    return true;
  }
  if (current_page(self) == page::home)
    state::set(*self.memory, "app.home.card.focused", false);
  state::set(*self.memory, "app.input.long_press", static_cast<std::int64_t>(value.key));
  state::set(*self.memory, "app.input.long_press_duration_ms",
             static_cast<std::int64_t>(value.duration_ms));
  const auto page = current_page(self);
  const bool has_dock = page == page::library || page == page::favorites || page == page::files ||
                        page == page::settings ||
                        (page == page::reader && state::get(*self.memory, "app.reader.chrome", false));
  if (value.key == event::key_code::down && has_dock) {
    state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::dock));
    self.next_refresh = refresh::mode::quality;
    ++self.invalidations;
  } else if (value.key == event::key_code::up &&
             static_cast<focus_area>(state::get(*self.memory, "app.focus.area",
                                                static_cast<std::int64_t>(focus_area::content))) ==
                 focus_area::dock) {
    state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content));
    self.next_refresh = refresh::mode::quality;
    ++self.invalidations;
  }
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
  const auto mode = self.next_refresh;
  const auto rect = self.has_next_rect ? self.next_rect : geometry::rect{0, 0, d.width, d.height};
  self.next_refresh = refresh::mode::fast;
  self.has_next_rect = false;
  display::update(d, rect, mode);
}

inline bool on_event(const event::value& value, void* user) {
  auto& self = *static_cast<context*>(user);
  self.last_input_ms = platform::now_ms(*self.shell.platform);
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
  if (current_page(self) == page::sleep) {
    if (self.sleep_armed && now - self.sleep_armed_ms >= 1500U) {
      self.sleep_armed = false;
      platform::enter_deep_sleep(*self.shell.platform);
    }
    return;
  }
  const auto timeout = state::get(*self.memory, "reader.settings.sleep_timeout_minutes", std::int64_t{10});
  if (timeout > 0 && now - self.last_input_ms >= static_cast<std::uint32_t>(timeout) * 60000U)
    routes::set_page(self, page::sleep);
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
