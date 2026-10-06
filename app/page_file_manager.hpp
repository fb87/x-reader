#pragma once

#include "assets.hpp"
#include "pages_fwd.hpp"

/** @brief The storage browser page, ported from app/page_file_manager.c. */
namespace app {

inline constexpr int file_manager_max_entries = 64;

enum class file_manager_action : std::uint16_t { up_back = 1 };

inline void up_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::arrow_back, g);
}
inline constexpr page::action file_manager_actions[] = {
    {static_cast<std::uint16_t>(file_manager_action::up_back), "Up / Back", event::key_code::none,
     up_icon},
};

inline bool is_epub(const char* name) {
  const std::size_t length = std::strlen(name);
  return (length > 5 && strcasecmp(name + length - 5, ".epub") == 0) ||
         (length > 4 && strcasecmp(name + length - 4, ".epu") == 0);
}

struct file_entry {
  char name[128] = {0};
  char display[128] = {0};
  bool directory = false;
};

struct file_manager_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
  widget::list list{};
  file_entry entries[file_manager_max_entries]{};
  char path[storage::path_max] = {0};
  char title[128] = {0};
  int count = 0;
};

inline file_manager_page& file_manager_of(page::context& p) {
  return *reinterpret_cast<file_manager_page*>(reinterpret_cast<char*>(&p) -
                                               offsetof(file_manager_page, base));
}

inline bool file_manager_collect_entry(const storage::entry& value, void* user) {
  auto& page = *static_cast<file_manager_page*>(user);
  if (value.name == nullptr || value.name[0] == '\0' ||
      (!value.directory && !is_epub(value.name))) {
    return true;
  }
  if (page.count >= file_manager_max_entries) return false;
  file_entry& entry = page.entries[page.count++];
  std::snprintf(entry.name, sizeof(entry.name), "%s", value.name);
  entry.directory = value.directory;
  book::make_title(entry.display, sizeof(entry.display), value.name);
  return true;
}

inline const char* path_name(const char* path) {
  const char* slash = std::strrchr(path, '/');
  return (slash != nullptr && slash[1] != '\0') ? slash + 1 : path;
}

inline void file_manager_load_directory(file_manager_page& page, const char* path) {
  std::snprintf(page.path, sizeof(page.path), "%s", path);
  page.count = 0;
  storage::device* storage_device =
      capability::get<storage::device>(*page.app->capabilities, capability::id::storage);
  storage::list(*storage_device, page.path, file_manager_collect_entry, &page);
  widget::set_count(page.list, page.count);
  std::snprintf(page.title, sizeof(page.title), "Files: %.120s", path_name(page.path));
  page::set_title(page.base, page.title);
}

inline void file_manager_row(widget::list& l, int index, const char** primary,
                             const char** secondary) {
  auto& page = *static_cast<file_manager_page*>(l.user);
  file_entry& entry = page.entries[index];
  *primary = entry.display;
  *secondary = entry.directory ? "Folder" : "EPUB";
}

inline void file_manager_render(page::context& base, canvas::surface& canvas) {
  file_manager_page& page = file_manager_of(base);
  widget::list& list = page.list;
  const int visible = widget::visible_rows(list);
  const bool focused = widget::has_focus(list.base_widget);
  const int row_width = list.base_widget.rect.w - (list.count > visible ? 10 : 0);

  for (int i = list.top; i < list.count && i < list.top + visible; ++i) {
    const geometry::rect row = geometry::make(list.base_widget.rect.x,
                                              list.base_widget.rect.y + (i - list.top) * list.row_h,
                                              row_width, list.row_h);
    if (!geometry::intersects(row, canvas.clip)) continue;
    const bool inverted = i == list.selected && focused;
    const std::uint8_t foreground = inverted ? canvas::gray::white : canvas::gray::black;
    const std::uint8_t secondary = inverted ? canvas::gray::white : canvas::gray::dark;
    file_entry& entry = page.entries[i];
    const icon entry_icon = entry.directory ? icon::folder : icon::description;
    const int icon_y = row.y + (row.h - 24) / 2;
    const int text_x = row.x + 42;
    const int primary_height = base.shell->theme_ptr->font_normal->line_height;
    const int secondary_height = base.shell->theme_ptr->font_small->line_height;
    const int text_y = row.y + (row.h - primary_height - secondary_height) / 2;

    canvas::fill(canvas, row, inverted ? canvas::gray::black : canvas::gray::white);
    if (i == list.selected && !focused) {
      canvas::fill(canvas, geometry::make(row.x, row.y + 6, 5, row.h - 12), canvas::gray::black);
    }
    if (!inverted) canvas::hline(canvas, row.x, row.y + row.h - 1, row.w, canvas::gray::light);
    draw_icon(canvas, geometry::make(row.x + 10, icon_y, 24, 24), entry_icon, foreground);
    canvas::draw_text_in(canvas, *base.shell->theme_ptr->font_normal,
                         geometry::make(text_x, text_y, row.x + row.w - text_x - 8, primary_height),
                         entry.display, text::align::left, foreground);
    canvas::draw_text_in(canvas, *base.shell->theme_ptr->font_small,
                         geometry::make(text_x, text_y + primary_height, row.x + row.w - text_x - 8,
                                        secondary_height),
                         entry.directory ? "Folder" : "EPUB", text::align::left, secondary);
  }
}

inline bool make_child_path(char* output, std::size_t capacity, const char* parent,
                            const char* name) {
  const int written =
      std::snprintf(output, capacity, "%s%s%s", parent,
                    (parent[0] != '\0' && parent[std::strlen(parent) - 1] == '/') ? "" : "/", name);
  return written >= 0 && static_cast<std::size_t>(written) < capacity;
}

inline void file_manager_selected(widget::list& list, int index) {
  auto& page = *static_cast<file_manager_page*>(list.user);
  if (index < 0 || index >= page.count) return;
  file_entry& entry = page.entries[index];
  char path[storage::path_max];
  if (!make_child_path(path, sizeof(path), page.path, entry.name)) return;
  if (entry.directory) {
    file_manager_load_directory(page, path);
    return;
  }
  const char* root = page.app->storage_root;
  const std::size_t root_length = std::strlen(root);
  const char* open_path = path;
  if (root_length != 0 && std::strncmp(path, root, root_length) == 0 &&
      (path[root_length] == '/' || path[root_length] == '\0')) {
    open_path = path + root_length;
    if (*open_path == '/') ++open_path;
  }
  const int opened = library::open_transient(page.app->library, open_path, entry.display);
  if (opened >= 0) open_book(*page.app, *page.nav, opened);
}

inline void file_manager_create(page::context& base) {
  file_manager_page& page = file_manager_of(base);
  const shell::theme& theme = theme_of(base);
  const geometry::rect area = base.area;
  widget::init(page.list, geometry::make(area.x + 4, area.y + 4, area.w - 12, area.h - 8),
               widget::list_style::two_line, theme.row_h, *theme.font_normal, *theme.font_small,
               file_manager_row, &page);
  page.list.on_select = file_manager_selected;
  page::add(base, page.list.base_widget);
  file_manager_load_directory(page, page.app->storage_root);
}

inline void file_manager_action(page::context& base, std::uint16_t action) {
  if (action != static_cast<std::uint16_t>(file_manager_action::up_back)) return;
  file_manager_page& page = file_manager_of(base);
  const char* root = page.app->storage_root;
  if (std::strcmp(page.path, root) == 0) {
    shell::pop(*base.shell);
    return;
  }
  char parent[storage::path_max];
  std::snprintf(parent, sizeof(parent), "%s", page.path);
  char* slash = std::strrchr(parent, '/');
  if (slash == nullptr || slash < parent + std::strlen(root)) {
    std::snprintf(parent, sizeof(parent), "%s", root);
  } else if (slash == parent) {
    slash[1] = '\0';
  } else {
    *slash = '\0';
  }
  file_manager_load_directory(page, parent);
}

inline constexpr page::vtbl file_manager_vtbl{
    file_manager_create, nullptr, nullptr, nullptr, nullptr, file_manager_render, nullptr,
    file_manager_action, nullptr,
};

}  // namespace app
