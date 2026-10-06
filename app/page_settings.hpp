#pragma once

#include "assets.hpp"
#include "pages_fwd.hpp"

/** @brief The settings page, ported from app/page_settings.c. */
namespace app {

enum settings_row {
  row_wifi = 0,
  row_bluetooth,
  row_font,
  row_refresh,
  row_progress,
  row_sleep,
  row_about,
  row_count,
};

inline constexpr int refresh_choices[] = {1, 3, 6, 10, 0};
inline constexpr page::action settings_actions[] = {
    {1, "Back", event::key_code::none, back_icon},
};

struct settings_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
  widget::list list{};
  char value[32] = {0};
};

inline settings_page& settings_of(page::context& p) {
  return *reinterpret_cast<settings_page*>(reinterpret_cast<char*>(&p) -
                                           offsetof(settings_page, base));
}

inline void settings_row_fn(widget::list& l, int i, const char** primary, const char** secondary) {
  settings_page& sp = *static_cast<settings_page*>(l.user);
  state::store& memory = *sp.app->memory;
  switch (i) {
    case row_wifi:
      *primary = "Wi-Fi";
      *secondary = state::get(memory, key::wifi_connected, false) ? "Connected" : "Off";
      break;
    case row_bluetooth:
      *primary = "Bluetooth";
      *secondary = state::get(memory, key::bluetooth_enabled, false) ? "Connected" : "Off";
      break;
    case row_font:
      *primary = "Font size";
      *secondary = font_names[state::get(memory, key::font_size, std::int64_t{1})];
      break;
    case row_refresh: {
      *primary = "Full refresh";
      const std::int64_t every = state::get(memory, key::full_refresh_every, std::int64_t{6});
      if (every == 0)
        *secondary = "Never";
      else if (every == 1)
        *secondary = "Every page";
      else {
        std::snprintf(sp.value, sizeof(sp.value), "Every %lld pages",
                      static_cast<long long>(every));
        *secondary = sp.value;
      }
      break;
    }
    case row_progress:
      *primary = "Progress bar";
      *secondary = state::get(memory, key::show_progress, true) ? "On" : "Off";
      break;
    case row_sleep:
      *primary = "Sleep timeout";
      std::snprintf(
          sp.value, sizeof(sp.value), "%lld minutes",
          static_cast<long long>(state::get(memory, key::sleep_timeout_minutes, std::int64_t{10})));
      *secondary = sp.value;
      break;
    default:
      *primary = "About";
      *secondary = "X-Reader 0.1";
      break;
  }
}

inline void settings_selected(widget::list& l, int i) {
  settings_page& sp = *static_cast<settings_page*>(l.user);
  state::store& memory = *sp.app->memory;
  switch (i) {
    case row_wifi:
      state::set(memory, key::wifi_connected, !state::get(memory, key::wifi_connected, false));
      break;
    case row_bluetooth:
      state::set(memory, key::bluetooth_enabled,
                 !state::get(memory, key::bluetooth_enabled, false));
      break;
    case row_font:
      state::set(memory, key::font_size,
                 (state::get(memory, key::font_size, std::int64_t{1}) + 1) % 3);
      break;
    case row_refresh: {
      constexpr int n = sizeof(refresh_choices) / sizeof(refresh_choices[0]);
      const std::int64_t current = state::get(memory, key::full_refresh_every, std::int64_t{6});
      int k = 0;
      for (int j = 0; j < n; ++j) {
        if (refresh_choices[j] == current) k = j;
      }
      const std::int64_t next = refresh_choices[(k + 1) % n];
      state::set(memory, key::full_refresh_every, next);
      shell::set_full_refresh_every(sp.app->shell, static_cast<std::uint16_t>(next));
      break;
    }
    case row_progress:
      state::set(memory, key::show_progress, !state::get(memory, key::show_progress, true));
      break;
    case row_sleep: {
      const std::int64_t current = state::get(memory, key::sleep_timeout_minutes, std::int64_t{10});
      state::set(memory, key::sleep_timeout_minutes,
                 static_cast<std::int64_t>(current == 10   ? 30
                                           : current == 30 ? 60
                                                           : 10));
      break;
    }
    case row_about:
      show_about_confirm(*sp.app, *sp.nav);
      return;
    default:
      return;
  }
  // Only the value text changed: refresh just that row.
  widget::invalidate_row(l, i, refresh::mode::quality);
}

inline void settings_create(page::context& p) {
  settings_page& sp = settings_of(p);
  const shell::theme& t = theme_of(p);
  const geometry::rect a = p.area;
  widget::init(sp.list, geometry::make(a.x + 4, a.y + 4, a.w - 8, a.h - 8),
               widget::list_style::value, t.row_h - 8, *t.font_normal, *t.font_normal,
               settings_row_fn, &sp);
  sp.list.on_select = settings_selected;
  widget::set_count(sp.list, row_count);
  page::add(p, sp.list.base_widget);
}

inline void settings_action(page::context& p, std::uint16_t) { shell::pop(*p.shell); }

inline constexpr page::vtbl settings_vtbl{
    settings_create, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, settings_action, nullptr,
};

}  // namespace app
