#pragma once

#include "app/cpp/init.hpp"
#include "core/widgets/all.hpp"
#include "core/connectivity.hpp"
#include "runtime/lua/api.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

/** @brief Product-neutral Lua binding runtime. */
namespace lua {

struct host {
  api::library* lib = nullptr;
  app::context* app = nullptr;
};

struct runtime {
  api::library api{};
  api::state* vm = nullptr;
  host binding{};
  char script[storage::path_max] = "app/lua/init.lua";
  std::uint32_t generation = 0;
  bool reload_requested = false;
};

inline runtime*& active_runtime() {
  static runtime* current = nullptr;
  return current;
}

inline host& bound(api::state* vm) {
  runtime* rt = active_runtime();
  return *static_cast<host*>(rt->api.to_userdata(vm, api::upvalue_index(1)));
}

inline int integer_arg(api::state* vm, api::library& lib, int index, int fallback = 0) {
  int valid = 0;
  const auto value = lib.to_integer(vm, index, &valid);
  return valid != 0 ? static_cast<int>(value) : fallback;
}

inline canvas::gray gray_arg(api::state* vm, api::library& lib, int index,
                             canvas::gray fallback = canvas::gray::black) {
  const int value = integer_arg(vm, lib, index, static_cast<int>(fallback));
  if (value <= 0) return canvas::gray::black;
  if (value <= 5) return canvas::gray::dark;
  if (value <= 10) return canvas::gray::light;
  return canvas::gray::white;
}

inline display::device* display_of(host& h) { return h.app->shell.display; }

inline int l_state_get(api::state* vm) {
  auto& h = bound(vm);
  auto& lib = *h.lib;
  const char* key = lib.to_string(vm, 1, nullptr);
  if (key == nullptr) return 0;
  const auto* entry = state::find(*h.app->memory, key);
  if (entry == nullptr) return 0;
  switch (entry->data.type) {
    case state::value_type::boolean:
      lib.push_boolean(vm, entry->data.boolean ? 1 : 0);
      break;
    case state::value_type::integer:
      lib.push_integer(vm, entry->data.integer);
      break;
    case state::value_type::string:
      lib.push_string(vm, entry->data.string.data());
      break;
    case state::value_type::empty:
      return 0;
  }
  return 1;
}

inline int l_state_set(api::state* vm) {
  auto& h = bound(vm);
  auto& lib = *h.lib;
  const char* key = lib.to_string(vm, 1, nullptr);
  if (key == nullptr) return 0;
  bool ok = false;
  switch (lib.type(vm, 2)) {
    case api::type_boolean:
      ok = state::set(*h.app->memory, key, lib.to_boolean(vm, 2) != 0);
      break;
    case api::type_number: {
      int valid = 0;
      const auto value = lib.to_integer(vm, 2, &valid);
      ok = valid != 0 && state::set(*h.app->memory, key, static_cast<std::int64_t>(value));
      break;
    }
    case api::type_string: {
      const char* value = lib.to_string(vm, 2, nullptr);
      ok = value != nullptr && state::set(*h.app->memory, key, value);
      break;
    }
    default:
      break;
  }
  if (ok) ++h.app->invalidations;
  lib.push_boolean(vm, ok ? 1 : 0);
  return 1;
}

inline int l_api_width(api::state* vm) {
  auto& h = bound(vm);
  h.lib->push_integer(vm, display_of(h) != nullptr ? display_of(h)->width : 0);
  return 1;
}

inline int l_api_height(api::state* vm) {
  auto& h = bound(vm);
  h.lib->push_integer(vm, display_of(h) != nullptr ? display_of(h)->height : 0);
  return 1;
}

inline int l_api_clear(api::state* vm) {
  auto& h = bound(vm);
  auto* display = display_of(h);
  if (display != nullptr) {
    canvas::fill(*display, {0, 0, display->width, display->height},
                 gray_arg(vm, *h.lib, 1, canvas::gray::white));
  }
  return 0;
}

inline int l_api_fill(api::state* vm) {
  auto& h = bound(vm);
  auto* display = display_of(h);
  if (display != nullptr) {
    canvas::fill(*display,
                 {integer_arg(vm, *h.lib, 1), integer_arg(vm, *h.lib, 2),
                  integer_arg(vm, *h.lib, 3), integer_arg(vm, *h.lib, 4)},
                 gray_arg(vm, *h.lib, 5));
  }
  return 0;
}

inline int l_api_border(api::state* vm) {
  auto& h = bound(vm);
  auto* display = display_of(h);
  if (display != nullptr) {
    canvas::border(*display,
                   {integer_arg(vm, *h.lib, 1), integer_arg(vm, *h.lib, 2),
                    integer_arg(vm, *h.lib, 3), integer_arg(vm, *h.lib, 4)},
                   integer_arg(vm, *h.lib, 5, 1), gray_arg(vm, *h.lib, 6));
  }
  return 0;
}

inline int l_api_hline(api::state* vm) {
  auto& h = bound(vm);
  auto* display = display_of(h);
  if (display != nullptr) {
    canvas::hline(*display, integer_arg(vm, *h.lib, 1), integer_arg(vm, *h.lib, 2),
                  integer_arg(vm, *h.lib, 3), gray_arg(vm, *h.lib, 4));
  }
  return 0;
}

inline int l_api_text(api::state* vm) {
  auto& h = bound(vm);
  auto* display = display_of(h);
  const char* value = h.lib->to_string(vm, 3, nullptr);
  if (display != nullptr && value != nullptr) {
    text::draw(*display, integer_arg(vm, *h.lib, 1), integer_arg(vm, *h.lib, 2), value,
               integer_arg(vm, *h.lib, 4, 2), gray_arg(vm, *h.lib, 5));
  }
  return 0;
}

inline int l_api_center(api::state* vm) {
  auto& h = bound(vm);
  auto* display = display_of(h);
  const char* value = h.lib->to_string(vm, 5, nullptr);
  if (display != nullptr && value != nullptr) {
    text::center(*display,
                 {integer_arg(vm, *h.lib, 1), integer_arg(vm, *h.lib, 2),
                  integer_arg(vm, *h.lib, 3), integer_arg(vm, *h.lib, 4)},
                 value, integer_arg(vm, *h.lib, 6, 2), gray_arg(vm, *h.lib, 7));
  }
  return 0;
}


inline widget::style widget_style_args(api::state* vm, api::library& lib, int first) {
  widget::style s{};
  s.background = gray_arg(vm, lib, first + 0, s.background);
  s.foreground = gray_arg(vm, lib, first + 1, s.foreground);
  s.secondary = gray_arg(vm, lib, first + 2, s.secondary);
  s.focus_background = gray_arg(vm, lib, first + 3, s.focus_background);
  s.focus_foreground = gray_arg(vm, lib, first + 4, s.focus_foreground);
  s.divider = gray_arg(vm, lib, first + 5, s.divider);
  s.border_width = integer_arg(vm, lib, first + 6, s.border_width);
  s.text_scale = integer_arg(vm, lib, first + 7, s.text_scale);
  return s;
}

inline int l_api_status(api::state* vm) {
  auto& h = bound(vm); const char* title = h.lib->to_string(vm, 1, nullptr);
  if (display_of(h) != nullptr && h.app->shell.platform != nullptr && title != nullptr)
    widget::status(*display_of(h), *h.app->shell.platform, title, widget_style_args(vm, *h.lib, 2));
  return 0;
}

inline int l_api_row(api::state* vm) {
  auto& h = bound(vm); auto& lib = *h.lib; const char* primary = lib.to_string(vm, 5, nullptr);
  const char* secondary = lib.to_string(vm, 6, nullptr);
  if (display_of(h) != nullptr && primary != nullptr)
    widget::row(*display_of(h), {integer_arg(vm, lib, 1), integer_arg(vm, lib, 2), integer_arg(vm, lib, 3), integer_arg(vm, lib, 4)},
            primary, secondary, lib.to_boolean(vm, 7) != 0, widget_style_args(vm, lib, 8));
  return 0;
}

inline int l_api_button(api::state* vm) {
  auto& h = bound(vm); auto& lib = *h.lib; const char* label = lib.to_string(vm, 5, nullptr);
  if (display_of(h) != nullptr && label != nullptr)
    widget::button(*display_of(h), {integer_arg(vm, lib, 1), integer_arg(vm, lib, 2), integer_arg(vm, lib, 3), integer_arg(vm, lib, 4)},
               label, lib.to_boolean(vm, 6) != 0, widget_style_args(vm, lib, 7));
  return 0;
}

inline int l_api_progress(api::state* vm) {
  auto& h = bound(vm); auto& lib = *h.lib;
  if (display_of(h) != nullptr)
    widget::progress(*display_of(h), {integer_arg(vm, lib, 1), integer_arg(vm, lib, 2), integer_arg(vm, lib, 3), integer_arg(vm, lib, 4)},
                 integer_arg(vm, lib, 5), widget_style_args(vm, lib, 6));
  return 0;
}

inline int l_api_book_card(api::state* vm) {
  auto& h = bound(vm);
  auto& lib = *h.lib;
  const char* eyebrow = lib.to_string(vm, 5, nullptr);
  const char* title = lib.to_string(vm, 6, nullptr);
  if (display_of(h) != nullptr && eyebrow != nullptr && title != nullptr) {
    widget::book_card(*display_of(h),
                      {integer_arg(vm, lib, 1), integer_arg(vm, lib, 2),
                       integer_arg(vm, lib, 3), integer_arg(vm, lib, 4)},
                      eyebrow, title, lib.to_boolean(vm, 7) != 0,
                      widget_style_args(vm, lib, 8));
  }
  return 0;
}

inline int l_api_dialog(api::state* vm) {
  auto& h = bound(vm); auto& lib = *h.lib; const char* title = lib.to_string(vm, 1, nullptr); const char* body = lib.to_string(vm, 2, nullptr);
  if (display_of(h) != nullptr && title != nullptr && body != nullptr)
    widget::dialog(*display_of(h), title, body, widget_style_args(vm, lib, 3));
  return 0;
}
inline int l_api_present(api::state* vm) {
  auto& h = bound(vm);
  auto* display = display_of(h);
  if (display != nullptr) {
    display::update(*display, {0, 0, display->width, display->height}, refresh::mode::quality);
  }
  return 0;
}

inline int l_api_reload(api::state* vm) {
  auto* current = active_runtime();
  if (current != nullptr) current->reload_requested = true;
  (void)vm;
  return 0;
}

inline int l_api_battery(api::state* vm) {
  auto& h = bound(vm);
  const int value = h.app->shell.platform != nullptr
                        ? platform::battery_percent(*h.app->shell.platform)
                        : -1;
  h.lib->push_integer(vm, value);
  return 1;
}

inline int l_library_count(api::state* vm) {
  auto& h = bound(vm);
  h.lib->push_integer(vm, static_cast<api::integer>(h.app->reader.library.count));
  return 1;
}

inline int l_library_book(api::state* vm) {
  auto& h = bound(vm);
  int valid = 0;
  const int index = static_cast<int>(h.lib->to_integer(vm, 1, &valid));
  if (valid == 0 || index < 0 || index >= static_cast<int>(h.app->reader.library.count)) return 0;
  const auto& book = h.app->reader.library.books[static_cast<std::size_t>(index)];
  h.lib->create_table(vm, 0, 6);
  h.lib->push_string(vm, book.title.data());
  h.lib->set_field(vm, -2, "title");
  h.lib->push_string(vm, book.author.data());
  h.lib->set_field(vm, -2, "author");
  h.lib->push_integer(vm, book.progress);
  h.lib->set_field(vm, -2, "progress");
  h.lib->push_boolean(vm, book.favorite ? 1 : 0);
  h.lib->set_field(vm, -2, "favorite");
  h.lib->push_integer(vm, index);
  h.lib->set_field(vm, -2, "index");
  return 1;
}

inline int l_library_move(api::state* vm) {
  auto& h = bound(vm);
  app::move_library(*h.app, integer_arg(vm, *h.lib, 1));
  return 0;
}

inline int l_library_favorite(api::state* vm) {
  auto& h = bound(vm);
  const int index = app::selected_book_index(*h.app);
  if (index >= 0) {
    auto& item = h.app->reader.library.books[static_cast<std::size_t>(index)];
    item.favorite = !item.favorite;
    ++h.app->invalidations;
  }
  return 0;
}

inline int l_library_delete(api::state* vm) {
  auto& h = bound(vm);
  const int index = app::selected_book_index(*h.app);
  if (index >= 0) {
    library::remove(h.app->reader.library, static_cast<std::size_t>(index));
    state::set(*h.app->memory, "reader.library.count",
               static_cast<std::int64_t>(h.app->reader.library.count));
    state::set(*h.app->memory, "reader.library.selected", std::int64_t{0});
    ++h.app->invalidations;
  }
  return 0;
}

inline int l_reader_open_selected(api::state* vm) {
  auto& h = bound(vm);
  const int index = app::selected_book_index(*h.app);
  bool ok = false;
  if (index >= 0) {
    state::set(*h.app->memory, "reader.library.selected", static_cast<std::int64_t>(index));
    ok = reader::open_selected(h.app->reader, *h.app->memory);
  }
  h.lib->push_boolean(vm, ok ? 1 : 0);
  return 1;
}

inline int l_reader_next(api::state* vm) {
  auto& h = bound(vm);
  const bool chrome = state::get(*h.app->memory, "app.reader.chrome", false);
  const int top = chrome ? 60 : 28;
  const int bottom = chrome ? 88 : 36;
  const geometry::rect area{28, top, h.app->shell.display->width - 56,
                            h.app->shell.display->height - top - bottom - 24};
  const int scale = std::min(4, 2 + static_cast<int>(state::get(
                                  *h.app->memory, "reader.settings.font_size", std::int64_t{1})));
  reader::next_page(h.app->reader, *h.app->memory, area, scale);
  ++h.app->invalidations;
  return 0;
}

inline int l_reader_previous(api::state* vm) {
  auto& h = bound(vm);
  const bool chrome = state::get(*h.app->memory, "app.reader.chrome", false);
  const int top = chrome ? 60 : 28;
  const int bottom = chrome ? 88 : 36;
  const geometry::rect area{28, top, h.app->shell.display->width - 56,
                            h.app->shell.display->height - top - bottom - 24};
  const int scale = std::min(4, 2 + static_cast<int>(state::get(
                                  *h.app->memory, "reader.settings.font_size", std::int64_t{1})));
  reader::previous_page(h.app->reader, *h.app->memory, area, scale);
  ++h.app->invalidations;
  return 0;
}

inline int l_reader_adjust_font(api::state* vm) {
  auto& h = bound(vm);
  reader::adjust_font(*h.app->memory, integer_arg(vm, *h.lib, 1));
  ++h.app->invalidations;
  return 0;
}

struct wifi_scan_context {
  app::context* app = nullptr;
  int count = 0;
};

inline bool wifi_scan_entry(const wifi::access_point& point, void* user) {
  auto& scan = *static_cast<wifi_scan_context*>(user);
  if (scan.count == 0) {
    state::set(*scan.app->memory, "network.wifi.scan.first_ssid", point.ssid);
    state::set(*scan.app->memory, "network.wifi.scan.first_rssi",
               static_cast<std::int64_t>(point.rssi));
  }
  ++scan.count;
  return true;
}

inline int l_wifi_scan(api::state* vm) {
  auto& h = bound(vm);
  auto* device = capability::get<wifi::device>(*h.app->capabilities);
  if (device == nullptr) {
    state::set(*h.app->memory, "network.wifi.available", false);
    h.lib->push_boolean(vm, 0);
    return 1;
  }
  state::set(*h.app->memory, "network.wifi.available", true);
  wifi_scan_context scan{.app = h.app};
  const bool ok = wifi::scan(*device, wifi_scan_entry, &scan);
  state::set(*h.app->memory, "network.wifi.scan.count", static_cast<std::int64_t>(scan.count));
  ++h.app->invalidations;
  h.lib->push_boolean(vm, ok ? 1 : 0);
  return 1;
}

inline int l_wifi_connect_first(api::state* vm) {
  auto& h = bound(vm);
  auto* device = capability::get<wifi::device>(*h.app->capabilities);
  const char* ssid = state::get(*h.app->memory, "network.wifi.scan.first_ssid", "");
  const bool ok = device != nullptr && ssid[0] != '\0' &&
                  wifi::connect(*device, ssid, "test-password");
  if (ok) {
    state::set(*h.app->memory, "network.wifi.connected", true);
    state::set(*h.app->memory, "network.wifi.ssid", ssid);
    ++h.app->invalidations;
  }
  h.lib->push_boolean(vm, ok ? 1 : 0);
  return 1;
}

inline void push_function(runtime& self, api::c_function function) {
  self.api.push_lightuserdata(self.vm, &self.binding);
  self.api.push_cclosure(self.vm, function, 1);
}

inline void set_function(runtime& self, const char* name, api::c_function function) {
  push_function(self, function);
  self.api.set_field(self.vm, -2, name);
}

inline void set_module(runtime& self, const char* name, int functions,
                       const char* const* names, const api::c_function* callbacks) {
  self.api.create_table(self.vm, 0, functions);
  for (int i = 0; i < functions; ++i) set_function(self, names[i], callbacks[i]);
  self.api.set_field(self.vm, -2, name);
}

inline void register_shell(runtime& self) {
  self.api.create_table(self.vm, 0, 8);

  {
    const char* names[] = {"width", "height", "clear", "fill", "border", "hline",
                           "text", "center", "status", "row", "button", "progress",
                           "book_card", "dialog", "present", "battery", "reload"};
    const api::c_function callbacks[] = {l_api_width, l_api_height, l_api_clear, l_api_fill,
                                         l_api_border, l_api_hline, l_api_text, l_api_center,
                                         l_api_status, l_api_row, l_api_button, l_api_progress,
                                         l_api_book_card, l_api_dialog, l_api_present, l_api_battery, l_api_reload};
    set_module(self, "api", 17, names, callbacks);
  }
  {
    const char* names[] = {"get", "set"};
    const api::c_function callbacks[] = {l_state_get, l_state_set};
    set_module(self, "state", 2, names, callbacks);
  }
  {
    const char* names[] = {"count", "book", "move", "favorite", "delete"};
    const api::c_function callbacks[] = {l_library_count, l_library_book, l_library_move,
                                         l_library_favorite, l_library_delete};
    set_module(self, "library", 5, names, callbacks);
  }
  {
    const char* names[] = {"open_selected", "next", "previous", "adjust_font"};
    const api::c_function callbacks[] = {l_reader_open_selected, l_reader_next,
                                         l_reader_previous, l_reader_adjust_font};
    set_module(self, "reader", 4, names, callbacks);
  }

  self.api.set_global(self.vm, "shell");

  self.api.create_table(self.vm, 0, 1);
  if (capability::get<wifi::device>(*self.binding.app->capabilities) != nullptr) {
    const char* names[] = {"scan", "connect_first"};
    const api::c_function callbacks[] = {l_wifi_scan, l_wifi_connect_first};
    set_module(self, "wifi", 2, names, callbacks);
  }
  self.api.set_global(self.vm, "_native_modules");
}

inline const char* error_text(runtime& self) {
  const char* value = self.api.to_string(self.vm, -1, nullptr);
  return value != nullptr ? value : "unknown Lua error";
}

inline bool call0(runtime& self, const char* function, int results = 0) {
  self.api.get_global(self.vm, function);
  if (self.api.type(self.vm, -1) == api::type_nil) {
    api::pop(self.api, self.vm, 1);
    return false;
  }
  if (self.api.pcall(self.vm, 0, results, 0, 0, nullptr) != api::ok) {
    std::fprintf(stderr, "lua: %s: %s\n", function, error_text(self));
    api::pop(self.api, self.vm, 1);
    return false;
  }
  return true;
}

inline bool call_init(runtime& self, bool reload) {
  self.api.get_global(self.vm, "init");
  if (self.api.type(self.vm, -1) == api::type_nil) {
    api::pop(self.api, self.vm, 1);
    return false;
  }
  self.api.push_boolean(self.vm, reload ? 1 : 0);
  if (self.api.pcall(self.vm, 1, 1, 0, 0, nullptr) != api::ok) {
    std::fprintf(stderr, "lua: init: %s\n", error_text(self));
    api::pop(self.api, self.vm, 1);
    return false;
  }
  const bool ok = self.api.to_boolean(self.vm, -1) != 0;
  api::pop(self.api, self.vm, 1);
  return ok;
}

inline bool load_application(runtime& self, bool reload) {
  register_shell(self);
  if (self.api.load_file(self.vm, self.script, nullptr) != api::ok) {
    std::fprintf(stderr, "lua: load %s failed: %s\n", self.script, error_text(self));
    api::pop(self.api, self.vm, 1);
    return false;
  }
  if (self.api.pcall(self.vm, 0, 0, 0, 0, nullptr) != api::ok) {
    std::fprintf(stderr, "lua: execute %s failed: %s\n", self.script, error_text(self));
    api::pop(self.api, self.vm, 1);
    return false;
  }
  return call_init(self, reload);
}

inline bool init(runtime& self, app::context& application,
                 const char* script = "app/lua/init.lua") {
  if (!api::load(self.api)) return false;
  std::snprintf(self.script, sizeof(self.script), "%s", script);
  self.binding = {.lib = &self.api, .app = &application};
  self.vm = self.api.new_state();
  if (self.vm == nullptr) {
    api::unload(self.api);
    return false;
  }
  self.api.open_libs(self.vm);
  active_runtime() = &self;
  if (!load_application(self, false)) {
    self.api.close(self.vm);
    self.vm = nullptr;
    api::unload(self.api);
    active_runtime() = nullptr;
    return false;
  }
  self.generation = 1;
  return true;
}

/**
 * @brief Recreates only the Lua VM while keeping native app/domain/board state alive.
 *
 * Reload is transactional: syntax/runtime/init failures discard the candidate VM and
 * restore the previously-running VM unchanged.
 */
inline bool reload(runtime& self) {
  if (self.vm == nullptr || self.api.new_state == nullptr || self.binding.app == nullptr) {
    return false;
  }

  api::state* old_vm = self.vm;
  api::state* candidate = self.api.new_state();
  if (candidate == nullptr) return false;

  // Candidate scripts are allowed to use the normal native API during module load/init.
  // Preserve the in-memory domain model so a failed candidate cannot corrupt the
  // currently-running application. External side effects (for example a real Wi-Fi
  // connect) should not be performed from module top-level/init hooks.
  state::store memory_snapshot = *self.binding.app->memory;
  reader::context reader_snapshot = self.binding.app->reader;
  const int invalidations_snapshot = self.binding.app->invalidations;

  self.vm = candidate;
  self.api.open_libs(self.vm);
  active_runtime() = &self;
  const bool ok = load_application(self, true);
  if (!ok) {
    *self.binding.app->memory = memory_snapshot;
    self.binding.app->reader = reader_snapshot;
    self.binding.app->invalidations = invalidations_snapshot;
    self.api.close(candidate);
    self.vm = old_vm;
    active_runtime() = &self;
    self.reload_requested = false;
    return false;
  }

  self.api.close(old_vm);
  ++self.generation;
  self.reload_requested = false;
  return true;
}

inline void request_reload(runtime& self) { self.reload_requested = true; }

inline bool take_reload_request(runtime& self) {
  if (!self.reload_requested) return false;
  self.reload_requested = false;
  return true;
}

inline bool render(runtime& self) {
  active_runtime() = &self;
  return call0(self, "render");
}

inline bool on_event(runtime& self, const event::value& value) {
  active_runtime() = &self;
  self.api.get_global(self.vm, "on_event");
  if (self.api.type(self.vm, -1) == api::type_nil) {
    api::pop(self.api, self.vm, 1);
    return false;
  }
  self.api.create_table(self.vm, 0, 7);
  const char* kind = "none";
  switch (value.event_type) {
    case event::type::key: kind = "key"; break;
    case event::type::tap: kind = "tap"; break;
    case event::type::press: kind = "press"; break;
    case event::type::release: kind = "release"; break;
    default: break;
  }
  self.api.push_string(self.vm, kind);
  self.api.set_field(self.vm, -2, "type");
  const char* key = "none";
  switch (value.key) {
    case event::key_code::up: key = "up"; break;
    case event::key_code::down: key = "down"; break;
    case event::key_code::left: key = "left"; break;
    case event::key_code::right: key = "right"; break;
    case event::key_code::ok: key = "ok"; break;
    case event::key_code::back: key = "back"; break;
    case event::key_code::menu: key = "menu"; break;
    default: break;
  }
  self.api.push_string(self.vm, key);
  self.api.set_field(self.vm, -2, "key");
  self.api.push_integer(self.vm, value.x);
  self.api.set_field(self.vm, -2, "x");
  self.api.push_integer(self.vm, value.y);
  self.api.set_field(self.vm, -2, "y");
  self.api.push_integer(self.vm, value.duration_ms);
  self.api.set_field(self.vm, -2, "duration_ms");
  self.api.push_boolean(self.vm, value.long_press ? 1 : 0);
  self.api.set_field(self.vm, -2, "long_press");

  if (self.api.pcall(self.vm, 1, 1, 0, 0, nullptr) != api::ok) {
    std::fprintf(stderr, "lua: on_event: %s\n", error_text(self));
    api::pop(self.api, self.vm, 1);
    return false;
  }
  const bool handled = self.api.to_boolean(self.vm, -1) != 0;
  api::pop(self.api, self.vm, 1);
  return handled;
}

inline bool tick(runtime& self, std::uint32_t now_ms) {
  active_runtime() = &self;
  self.api.get_global(self.vm, "tick");
  if (self.api.type(self.vm, -1) == api::type_nil) {
    api::pop(self.api, self.vm, 1);
    return false;
  }
  self.api.push_integer(self.vm, now_ms);
  if (self.api.pcall(self.vm, 1, 0, 0, 0, nullptr) != api::ok) {
    std::fprintf(stderr, "lua: tick: %s\n", error_text(self));
    api::pop(self.api, self.vm, 1);
    return false;
  }
  return true;
}

inline void close(runtime& self) {
  if (self.vm != nullptr) self.api.close(self.vm);
  self.vm = nullptr;
  api::unload(self.api);
  if (active_runtime() == &self) active_runtime() = nullptr;
}

}  // namespace lua
