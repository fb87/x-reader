#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

#include <cstdio>
#include <cstring>

namespace app::pages::settings {

inline constexpr int base_count = 6;

inline void base_row(context& self, int index, int visual_index) {
  char font[16]{};
  const char* fonts[] = {"Small", "Medium", "Large"};
  auto font_index = state::get(*self.memory, "reader.settings.font_size", std::int64_t{1});
  if (font_index < 0) font_index = 0;
  if (font_index > 2) font_index = 2;
  std::snprintf(font, sizeof(font), "%s", fonts[font_index]);
  char refresh[24]{};
  const auto full = state::get(*self.memory, "reader.settings.full_refresh_every", std::int64_t{6});
  if (full == 0) std::snprintf(refresh, sizeof(refresh), "Never");
  else std::snprintf(refresh, sizeof(refresh), "Every %lld", static_cast<long long>(full));
  char sleep[24]{};
  std::snprintf(sleep, sizeof(sleep), "%lld MIN",
                static_cast<long long>(state::get(*self.memory, "reader.settings.sleep_timeout_minutes",
                                                  std::int64_t{10})));
  const char* labels[] = {"Bluetooth", "Font size", "Full refresh", "Progress bar",
                          "Sleep timeout", "About"};
  const char* values[] = {
      state::get(*self.memory, "network.bluetooth.enabled", false) ? "On" : "Off",
      font, refresh,
      state::get(*self.memory, "reader.settings.show_progress", true) ? "On" : "Off",
      sleep, "READER 0.1",
  };
  draw_row(self, visual_index, 58 + visual_index * 76, labels[index], values[index]);
}

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  draw_status(self, "Settings");

  const int plugin_count = routes::menu_count(self, "settings");
  for (int i = 0; i < plugin_count; ++i) {
    const auto* route = routes::menu_route(self, "settings", i);
    if (route != nullptr)
      draw_row(self, i, 58 + i * 76, routes::menu_label(*route), routes::menu_value(*route));
  }
  for (int i = 0; i < base_count; ++i) base_row(self, i, plugin_count + i);

  const char* actions[] = {"Back"};
  if (static_cast<focus_area>(state::get(*self.memory, "app.focus.area",
                                         static_cast<std::int64_t>(focus_area::content))) ==
      focus_area::dock)
    draw_dock(self, actions, 1);
  if (state::get(*self.memory, "app.dialog.about", false))
    draw_dialog(self, "About Reader", "Software Reader 0.1");
}

inline bool activate_base(context& self, int selected) {
  switch (selected) {
    case 0:
      state::set(*self.memory, "network.bluetooth.enabled",
                 !state::get(*self.memory, "network.bluetooth.enabled", false));
      break;
    case 1:
      ::reader::adjust_font(*self.memory, 1);
      break;
    case 2: {
      constexpr std::int64_t choices[] = {1, 3, 6, 10, 0};
      const auto current = state::get(*self.memory, "reader.settings.full_refresh_every",
                                      std::int64_t{6});
      std::size_t index = 0;
      for (std::size_t i = 0; i < 5; ++i) if (choices[i] == current) index = i;
      state::set(*self.memory, "reader.settings.full_refresh_every", choices[(index + 1) % 5]);
      break;
    }
    case 3:
      state::set(*self.memory, "reader.settings.show_progress",
                 !state::get(*self.memory, "reader.settings.show_progress", true));
      break;
    case 4: {
      const auto current = state::get(*self.memory, "reader.settings.sleep_timeout_minutes",
                                      std::int64_t{10});
      state::set(*self.memory, "reader.settings.sleep_timeout_minutes",
                 current == 10 ? std::int64_t{30}
                               : current == 30 ? std::int64_t{60} : std::int64_t{10});
      break;
    }
    default:
      state::set(*self.memory, "app.dialog.about", true);
      break;
  }
  ++self.invalidations;
  return true;
}

inline bool event(context& self, const event::value& value) {
  if (state::get(*self.memory, "app.dialog.about", false)) {
    if (value.event_type == event::type::key || value.event_type == event::type::tap) {
      state::set(*self.memory, "app.dialog.about", false);
      ++self.invalidations;
      return true;
    }
  }

  const int plugin_count = routes::menu_count(self, "settings");
  const int count = plugin_count + base_count;
  auto activate = [&](int selected) {
    if (selected < plugin_count) {
      const auto* route = routes::menu_route(self, "settings", selected);
      return route != nullptr ? routes::push(self, route->path) : false;
    }
    return activate_base(self, selected - plugin_count);
  };

  if (value.event_type == event::type::tap) {
    if (value.y < 58 || value.y >= 58 + count * 76) return false;
    const int selected = (value.y - 58) / 76;
    state::set(*self.memory, "app.menu.selected", static_cast<std::int64_t>(selected));
    return activate(selected);
  }
  if (value.event_type != event::type::key) return false;
  if (static_cast<focus_area>(state::get(*self.memory, "app.focus.area",
                                         static_cast<std::int64_t>(focus_area::content))) ==
      focus_area::dock) {
    if (value.key == event::key_code::up) {
      state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content));
      ++self.invalidations;
      return true;
    }
    if (value.key == event::key_code::ok || value.key == event::key_code::back) {
      return routes::push(self, "/");
    }
    if (value.key == event::key_code::down) return true;
  }
  if (value.key == event::key_code::back) return routes::push(self, "/");
  if (value.key == event::key_code::up) { move_selection(self, -1, count); return true; }
  if (value.key == event::key_code::down) {
    const auto before = selection(self);
    move_selection(self, 1, count);
    if (before == selection(self))
      state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::dock));
    return true;
  }
  if (value.key == event::key_code::ok) return activate(static_cast<int>(selection(self)));
  return false;
}

}  // namespace app::pages::settings
