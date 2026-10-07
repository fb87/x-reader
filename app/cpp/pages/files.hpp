#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

#include <cstdio>
#include <cstring>

namespace app::pages::files {

inline bool return_home(context& self) {
  if (!routes::back(self)) routes::set_page(self, page::home);
  state::set(*self.memory, "app.menu.selected", std::int64_t{2});
  state::set(*self.memory, "app.home.card.focused", false);
  return true;
}

inline bool collect_entry(const storage::entry& value, void* user) {
  auto& self = *static_cast<context*>(user);
  if (value.name == nullptr || value.name[0] == '\0' ||
      (!value.directory && !::library::supported(value.name)))
    return true;
  if (self.file_count >= static_cast<int>(self.files.size())) return false;
  auto& entry = self.files[static_cast<std::size_t>(self.file_count++)];
  std::snprintf(entry.name, sizeof(entry.name), "%s", value.name);
  entry.directory = value.directory;
  std::snprintf(entry.display, sizeof(entry.display), "%s", value.name);
  return true;
}

inline void load_directory(context& self, const char* path) {
  auto* device = capability::get<storage::device>(*self.capabilities, capability::id::storage);
  if (device == nullptr) return;
  std::snprintf(self.file_path, sizeof(self.file_path), "%s", path == nullptr ? "" : path);
  self.file_count = 0;
  self.file_offset = 0;
  storage::list(*device, self.file_path, collect_entry, &self);
  state::set(*self.memory, "app.files.selected", std::int64_t{0});
  ++self.invalidations;
}

inline void enter(context& self);

inline const char* path_name(const char* path) {
  const char* slash = std::strrchr(path, '/');
  return slash != nullptr && slash[1] != '\0' ? slash + 1 : path;
}

inline bool child_path(char* output, std::size_t capacity, const char* parent, const char* name) {
  const bool slash = parent[0] != '\0' && parent[std::strlen(parent) - 1] == '/';
  const int written = std::snprintf(output, capacity, "%s%s%s", parent, slash ? "" : "/", name);
  return written >= 0 && static_cast<std::size_t>(written) < capacity;
}

inline void render(context& self) {
  if (!state::get(*self.memory, "app.files.entered", false)) {
    enter(self);
    state::set(*self.memory, "app.files.entered", true);
  }
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  char title[64]{};
  std::snprintf(title, sizeof(title), "Files: %.48s", path_name(self.file_path));
  draw_status(self, title);
  const int visible_rows = list_visible_rows(self);
  const int visible = std::min(visible_rows, self.file_count - self.file_offset);
  for (int row = 0; row < visible; ++row) {
    const int index = self.file_offset + row;
    const auto& entry = self.files[static_cast<std::size_t>(index)];
    widget::row(d, {12, 58 + row * 76, d.width - 24, 68}, entry.display,
                entry.directory ? "Folder" : "EPUB",
                selection(self) == index && static_cast<focus_area>(state::get(
                    *self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content))) ==
                    focus_area::content,
                {}, entry.directory ? XR_ICON_FOLDER : XR_ICON_DESCRIPTION);
  }
  if (self.file_count > visible_rows) {
    const geometry::rect track{d.width - 18, 58, 6, visible_rows * 76};
    canvas::fill(d, track, canvas::gray::white);
    canvas::vline(d, track.x + 3, track.y, track.h, canvas::gray::light);
    const int thumb_h = std::max(track.h * visible_rows / self.file_count, 16);
    const int thumb_y = track.y + (track.h - thumb_h) * self.file_offset /
                        std::max(self.file_count - visible_rows, 1);
    canvas::fill(d, {track.x, thumb_y, 6, thumb_h}, canvas::gray::black);
  }
  if (self.file_count == 0)
    text::center(d, {0, 300, d.width, 80}, "No files", 3, canvas::gray::dark);
  const char* actions[] = {"Up / Back"};
  if (static_cast<focus_area>(state::get(*self.memory, "app.focus.area",
                                         static_cast<std::int64_t>(focus_area::content))) ==
      focus_area::dock)
    draw_file_dock(self, actions);
}

inline bool open_entry(context& self, int index) {
  if (index < 0 || index >= self.file_count) return false;
  const auto& entry = self.files[static_cast<std::size_t>(index)];
  char path[storage::path_max]{};
  if (!child_path(path, sizeof(path), self.file_path, entry.name)) return false;
  if (entry.directory) {
    load_directory(self, path);
    return true;
  }
  if (!::reader::open_path(self.reader, *self.memory, path, entry.display)) return false;
  routes::set_page(self, page::reader);
  return true;
}

inline bool event(context& self, const event::value& value) {
  if (value.event_type == event::type::tap) {
    if (value.y < 58 || value.y >= 58 + self.file_count * 76) return false;
    state::set(*self.memory, "app.files.selected",
               static_cast<std::int64_t>(self.file_offset + (value.y - 58) / 76));
    return open_entry(self, static_cast<int>(selection(self)));
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
      return_home(self);
      return true;
    }
    if (value.key == event::key_code::down) return true;
  }
  if (value.key == event::key_code::back) {
    auto* device = capability::get<storage::device>(*self.capabilities, capability::id::storage);
    const char* root = device != nullptr && device->root != nullptr ? device->root : "";
    if (std::strcmp(self.file_path, root) == 0 || self.file_path[0] == '\0') {
      return_home(self);
    } else {
      char parent[storage::path_max]{};
      std::snprintf(parent, sizeof(parent), "%s", self.file_path);
      char* slash = std::strrchr(parent, '/');
      if (slash == nullptr || slash <= parent + std::strlen(root))
        std::snprintf(parent, sizeof(parent), "%s", root);
      else
        *slash = '\0';
      load_directory(self, parent);
    }
    return true;
  }
  if (value.key == event::key_code::down) {
    const auto before = selection(self);
    move_selection(self, 1, self.file_count);
    if (before == selection(self))
      state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::dock));
    return true;
  }
  if (value.key == event::key_code::up) {
    move_selection(self, -1, self.file_count);
    return true;
  }
  if (value.key == event::key_code::ok) return open_entry(self, static_cast<int>(selection(self)));
  return false;
}

inline void enter(context& self) {
  auto* device = capability::get<storage::device>(*self.capabilities, capability::id::storage);
  load_directory(self, device != nullptr && device->root != nullptr ? device->root : "");
}

}  // namespace app::pages::files
