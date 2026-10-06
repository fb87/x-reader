#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

#include <cstdio>
#include <cstring>

namespace app::pages::files {

inline bool collect_entry(const storage::entry& value, void* user) {
  auto& self = *static_cast<context*>(user);
  if (value.name == nullptr || value.name[0] == '\0' ||
      (!value.directory && !::library::supported(value.name)))
    return true;
  if (self.file_count >= static_cast<int>(self.files.size())) return false;
  auto& entry = self.files[static_cast<std::size_t>(self.file_count++)];
  std::snprintf(entry.name, sizeof(entry.name), "%s", value.name);
  entry.directory = value.directory;
  book::make_title(entry.display, sizeof(entry.display), value.name);
  return true;
}

inline void load_directory(context& self, const char* path) {
  auto* device = capability::get<storage::device>(*self.capabilities, capability::id::storage);
  if (device == nullptr) return;
  std::snprintf(self.file_path, sizeof(self.file_path), "%s", path == nullptr ? "" : path);
  self.file_count = 0;
  storage::list(*device, self.file_path, collect_entry, &self);
  state::set(*self.memory, "app.menu.selected", std::int64_t{0});
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
  if (self.file_count == 0 && self.file_path[0] == '\0') enter(self);
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  char title[64]{};
  std::snprintf(title, sizeof(title), "FILES: %.48s", path_name(self.file_path));
  draw_status(self, title);
  for (int row = 0; row < self.file_count && row < 10; ++row) {
    const auto& entry = self.files[static_cast<std::size_t>(row)];
    draw_row(self, row, 58 + row * 76, entry.display, entry.directory ? "FOLDER" : "EPUB");
  }
  if (self.file_count == 0)
    text::center(d, {0, 300, d.width, 80}, "NO FILES", 3, canvas::gray::dark);
  const char* actions[] = {"UP / BACK"};
  draw_dock(self, actions, 1);
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
  if (!::library::add(self.reader.library, path)) return false;
  const auto selected = static_cast<std::int64_t>(self.reader.library.count - 1);
  state::set(*self.memory, "reader.library.selected", selected);
  if (!::reader::open_selected(self.reader, *self.memory)) return false;
  routes::set_page(self, page::reader);
  return true;
}

inline bool event(context& self, const event::value& value) {
  if (value.event_type == event::type::tap) {
    if (value.y < 58 || value.y >= 58 + self.file_count * 76) return false;
    state::set(*self.memory, "app.menu.selected", static_cast<std::int64_t>((value.y - 58) / 76));
    return open_entry(self, static_cast<int>(selection(self)));
  }
  if (value.event_type != event::type::key) return false;
  if (value.key == event::key_code::back) {
    auto* device = capability::get<storage::device>(*self.capabilities, capability::id::storage);
    const char* root = device != nullptr && device->root != nullptr ? device->root : "";
    if (std::strcmp(self.file_path, root) == 0 || self.file_path[0] == '\0') {
      routes::set_page(self, page::home);
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
    move_selection(self, 1, self.file_count);
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
