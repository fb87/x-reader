#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "../core/geometry.hpp"
#include "../core/state.hpp"
#include "../core/text.hpp"
#include "epub.hpp"
#include "library.hpp"

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_timer.h"
#endif

namespace reader {

inline constexpr std::size_t max_pages = 256;
inline constexpr std::uint16_t manifest_capacity = 640;
inline constexpr std::uint16_t spine_capacity = 640;
inline constexpr std::size_t scratch_size = 80U * 1024U;
inline constexpr std::size_t text_size = 64U * 1024U;

struct cache_header {
  std::uint32_t magic = 0x58524550U;
  std::uint32_t file_size = 0;
  std::uint32_t path_hash = 0;
  std::uint16_t manifest_count = 0;
  std::uint16_t spine_count = 0;
  std::uint32_t zip_tail_start = 0;
  std::uint32_t zip_tail_size = 0;
};

/** @brief Native EPUB session and bounded pagination state for the current book. */
struct session {
  epub::context document{};
  epub::file_view storage_view{};
  char storage_path[storage::path_max]{};
  std::array<epub::manifest_item, manifest_capacity> manifest{};
  std::array<epub::spine_item, spine_capacity> spine{};
  std::array<std::uint8_t, scratch_size> scratch{};
  std::array<char, text_size> text{};
  bool epub_open = false;
  bool cover_placeholder = false;
  std::uint16_t current_chapter = 0;
  std::array<std::uint32_t, max_pages> page_start{};
  int page_count = 0;
  int page = 0;
  int lines_per_page = 1;
  int total_pages = 0;
  int paginated_width = 0;
  int paginated_height = 0;
  int paginated_scale = 0;
  std::uint16_t paginated_chapter = 0;
  bool pagination_valid = false;
  char chapter_title[96] = {0};
};

/** @brief Runtime reader-domain context shared by application pages. */
struct context {
  library::index library{};
  storage::device* storage = nullptr;
  session current{};
};

inline void init(context&, state::store& shared) {
  if (state::find(shared, "reader.settings.font_size") == nullptr)
    state::set(shared, "reader.settings.font_size", std::int64_t{1});
  if (state::find(shared, "reader.settings.full_refresh_every") == nullptr)
    state::set(shared, "reader.settings.full_refresh_every", std::int64_t{6});
  if (state::find(shared, "reader.settings.show_progress") == nullptr)
    state::set(shared, "reader.settings.show_progress", true);
  if (state::find(shared, "reader.library.selected") == nullptr)
    state::set(shared, "reader.library.selected", std::int64_t{0});
  if (state::find(shared, "reader.book.current") == nullptr)
    state::set(shared, "reader.book.current", std::int64_t{-1});
  if (state::find(shared, "reader.book.page") == nullptr)
    state::set(shared, "reader.book.page", std::int64_t{0});
}

inline std::uint32_t cache_hash(const char* path) {
  std::uint32_t hash = 2166136261U;
  while (path != nullptr && *path != '\0')
    hash = (hash ^ static_cast<unsigned char>(*path++)) * 16777619U;
  return hash;
}

inline bool cache_path(const char* path, char* output, std::size_t capacity) {
  if (path == nullptr || output == nullptr || capacity == 0) return false;
  const bool is_sdcard_path =
      std::strncmp(path, "/sdcard/", 8) == 0 || std::strcmp(path, "/sdcard") == 0;
  const int written =
      std::snprintf(output, capacity, "%s/.xreader_epub_%08x.cache",
                    is_sdcard_path ? "/sdcard" : ".", static_cast<unsigned>(cache_hash(path)));
  return written > 0 && static_cast<std::size_t>(written) < capacity;
}

inline void bind_document(session& current) {
  current.document.storage = &current.storage_view;
  current.document.manifest = current.manifest.data();
  current.document.manifest_capacity = static_cast<std::uint16_t>(current.manifest.size());
  current.document.spine = current.spine.data();
  current.document.spine_capacity = static_cast<std::uint16_t>(current.spine.size());
  current.document.scratch = current.scratch.data();
  current.document.scratch_size = current.scratch.size();
}

inline bool load_cache(session& current, const char* path, std::uint32_t file_size) {
  char cache[storage::path_max]{};
  if (!cache_path(path, cache, sizeof(cache))) return false;
  std::FILE* file = std::fopen(cache, "rb");
  if (file == nullptr) return false;
  cache_header header{};
  const bool ok =
      std::fread(&header, sizeof(header), 1, file) == 1 && header.magic == 0x58524550U &&
      header.file_size == file_size && header.path_hash == cache_hash(path) &&
      header.manifest_count <= current.manifest.size() &&
      header.spine_count <= current.spine.size() &&
      std::fread(current.document.package_path, sizeof(current.document.package_path), 1, file) ==
          1 &&
      std::fread(current.document.title, sizeof(current.document.title), 1, file) == 1 &&
      std::fread(current.manifest.data(), sizeof(current.manifest[0]), header.manifest_count,
                 file) == header.manifest_count &&
      std::fread(current.spine.data(), sizeof(current.spine[0]), header.spine_count, file) ==
          header.spine_count;
  if (!ok) {
    std::fclose(file);
    return false;
  }
  current.document.manifest_count = header.manifest_count;
  current.document.spine_count = header.spine_count;
  bind_document(current);
  current.document.package_path[sizeof(current.document.package_path) - 1] = '\0';
  current.document.title[sizeof(current.document.title) - 1] = '\0';
  current.document.zip_tail_start = header.zip_tail_start;
  current.document.zip_tail_size = header.zip_tail_size;
  current.document.zip_tail_valid = header.zip_tail_size != 0;
  if (header.zip_tail_size > current.document.zip_tail.size() ||
      (header.zip_tail_size != 0 &&
       std::fread(current.document.zip_tail.data(), 1, header.zip_tail_size, file) !=
           header.zip_tail_size)) {
    std::fclose(file);
    return false;
  }
  std::fclose(file);
  return true;
}

inline void save_cache(const session& current, const char* path, std::uint32_t file_size) {
  char cache[storage::path_max]{};
  if (!cache_path(path, cache, sizeof(cache))) return;
  std::FILE* file = std::fopen(cache, "wb");
  if (file == nullptr) return;
  const cache_header header{0x58524550U,
                            file_size,
                            cache_hash(path),
                            current.document.manifest_count,
                            current.document.spine_count,
                            current.document.zip_tail_start,
                            current.document.zip_tail_valid ? current.document.zip_tail_size : 0};
  if (std::fwrite(&header, sizeof(header), 1, file) == 1 &&
      std::fwrite(current.document.package_path, sizeof(current.document.package_path), 1, file) ==
          1 &&
      std::fwrite(current.document.title, sizeof(current.document.title), 1, file) == 1 &&
      std::fwrite(current.manifest.data(), sizeof(current.manifest[0]), header.manifest_count,
                  file) == header.manifest_count &&
      std::fwrite(current.spine.data(), sizeof(current.spine[0]), header.spine_count, file) ==
          header.spine_count &&
      header.zip_tail_size != 0)
    std::fwrite(current.document.zip_tail.data(), 1, header.zip_tail_size, file);
  std::fclose(file);
}

/** @brief Loads one EPUB spine chapter. */
inline bool load_chapter(session& s, std::uint16_t chapter) {
  if (!s.epub_open || chapter >= s.document.spine_count) return false;
  std::uint32_t output = 0;
  if (epub::spine_text(s.document, chapter, s.scratch.data(), s.scratch.size(), s.text.data(),
                       s.text.size(), output) != epub::status::ok)
    return false;
  s.cover_placeholder = s.text[0] == '\0';
  if (s.cover_placeholder) std::strcpy(s.text.data(), "Cover");
  s.current_chapter = chapter;
  s.pagination_valid = false;
  return true;
}

/** @brief Opens an EPUB and loads its first spine chapter. */
inline bool open(session& s, const epub::file_view& storage, const char* fallback_title) {
  char path_copy[storage::path_max]{};
  std::snprintf(path_copy, sizeof(path_copy), "%s", storage.path == nullptr ? "" : storage.path);
  s.~session();
  new (&s) session{};
  std::snprintf(s.storage_path, sizeof(s.storage_path), "%s", path_copy);
  s.storage_view = storage;
  s.storage_view.path = s.storage_path;
  if (load_cache(s, s.storage_path, s.storage_view.size)) {
    s.epub_open = true;
    bind_document(s);
    return load_chapter(s, 0);
  }
  if (epub::open(s.document, s.storage_view, s.scratch.data(), s.scratch.size(), s.manifest.data(),
                 static_cast<std::uint16_t>(s.manifest.size()), s.spine.data(),
                 static_cast<std::uint16_t>(s.spine.size())) != epub::status::ok)
    return false;
  char title_copy[book::title_size]{};
  std::snprintf(title_copy, sizeof(title_copy), "%s", s.document.title);
  book::repair_mojibake(s.document.title, sizeof(s.document.title), title_copy);
  s.epub_open = true;
  save_cache(s, s.storage_path, s.storage_view.size);
  (void)fallback_title;
  return load_chapter(s, 0);
}

inline const char* current_text(const session& s) {
  return s.epub_open ? s.text.data() : "No EPUB open";
}

inline const char* current_chapter_title(session& s) {
  const char* value = current_text(s);
  std::size_t n = 0;
  while (value[n] != '\0' && value[n] != '\n' && n + 1 < sizeof(s.chapter_title)) ++n;
  std::memcpy(s.chapter_title, value, n);
  s.chapter_title[n] = '\0';
  return s.chapter_title[0] != '\0' ? s.chapter_title : "Cover";
}

inline bool is_cover_placeholder(const session& s) { return s.epub_open && s.cover_placeholder; }

/** @brief Measures one wrapped line and returns its byte span. */
inline std::size_t line_span(const char* text, int max_width, int scale, std::size_t& next) {
  const char* cursor = text;
  const char* last_space = nullptr;
  int width = 0;
  while (*cursor != '\0' && *cursor != '\n') {
    const char* after = cursor;
    const auto codepoint = text::next_codepoint(after);
    const int next_width = width + text::advance(codepoint, scale);
    if (next_width > max_width) break;
    width = next_width;
    if (*cursor == ' ') last_space = cursor;
    cursor = after;
  }
  const char* end = cursor;
  if (end == text && *end != '\0') {
    const char* after = end;
    (void)text::next_codepoint(after);
    end = after;
  } else if (*end != '\0' && *end != '\n' && last_space != nullptr) {
    end = last_space;
  }
  next = static_cast<std::size_t>(end - text);
  while (text[next] == ' ') ++next;
  if (text[next] == '\n') ++next;
  return static_cast<std::size_t>(end - text);
}

/** @brief Paginates the current chapter using the native bitmap text metrics. */
inline void paginate(session& s, geometry::rect text_rect, int scale) {
  if (s.pagination_valid && s.paginated_width == text_rect.w && s.paginated_height == text_rect.h &&
      s.paginated_scale == scale && s.paginated_chapter == s.current_chapter) {
    return;
  }
  s.page_count = 0;
  s.lines_per_page = std::max(text_rect.h / (7 * scale + 4), 1);
  std::size_t offset = 0;
  const std::size_t length = std::strlen(s.text.data());
  while (offset < length && s.page_count < static_cast<int>(s.page_start.size())) {
    s.page_start[s.page_count++] = static_cast<std::uint32_t>(offset);
    for (int line = 0; line < s.lines_per_page && offset < length; ++line) {
      std::size_t next = 0;
      line_span(s.text.data() + offset, text_rect.w, scale, next);
      offset += next;
    }
  }
  if (s.page_count == 0) s.page_count = 1;
  if (s.page >= s.page_count) s.page = s.page_count - 1;
  s.total_pages = s.page_count;
  s.paginated_width = text_rect.w;
  s.paginated_height = text_rect.h;
  s.paginated_scale = scale;
  s.paginated_chapter = s.current_chapter;
  s.pagination_valid = true;
}

inline bool turn_page(session& s, int delta, geometry::rect text_rect, int scale) {
  const int next = s.page + delta;
  if (next >= 0 && next < s.page_count) {
    s.page = next;
    return true;
  }
  const int chapter = static_cast<int>(s.current_chapter) + delta;
  if (!s.epub_open || chapter < 0 || chapter >= s.document.spine_count) return false;
  if (!load_chapter(s, static_cast<std::uint16_t>(chapter))) return false;
  paginate(s, text_rect, scale);
  s.page = delta > 0 ? 0 : s.page_count - 1;
  return true;
}

inline int progress_percent(const session& s) {
  if (!s.epub_open || s.document.spine_count == 0 || s.page_count == 0) return -1;
  const std::uint32_t current =
      static_cast<std::uint32_t>(s.page + 1) * 100U / static_cast<std::uint32_t>(s.page_count);
  const std::uint32_t progress = (static_cast<std::uint32_t>(s.current_chapter) * 100U + current) /
                                 static_cast<std::uint32_t>(s.document.spine_count);
  return static_cast<int>(std::min<std::uint32_t>(100U, progress));
}

inline bool open_path(context& self, state::store& shared, const char* path, const char* title,
                      std::int64_t current = -1) {
  if (path == nullptr || self.storage == nullptr) return false;
  const auto saved_current = state::get(shared, "reader.book.current", std::int64_t{-1});
  const auto saved_chapter = state::get(shared, "reader.book.chapter", std::int64_t{0});
  const auto saved_page = state::get(shared, "reader.book.page", std::int64_t{0});
  const auto saved_progress = state::get(shared, "reader.book.progress", std::int64_t{0});
#ifdef ESP_PLATFORM
  const auto started = esp_timer_get_time();
  ESP_LOGI("reader", "open %s", path);
#endif
  std::uint32_t size = 0;
  epub::file_view view{self.storage, path, 0};
  if (!storage::size(*self.storage, view.path, size)) return false;
  view.size = size;
  if (!open(self.current, view, title != nullptr ? title : path)) return false;
  const bool restore_position = current >= 0 && current == saved_current;
  if (restore_position && saved_chapter > 0 &&
      saved_chapter < static_cast<std::int64_t>(self.current.document.spine_count)) {
    if (!load_chapter(self.current, static_cast<std::uint16_t>(saved_chapter))) return false;
  }
  self.current.page = restore_position ? std::max<std::int64_t>(0, saved_page) : 0;
#ifdef ESP_PLATFORM
  ESP_LOGI("reader", "load first chapter");
#endif
  state::set(shared, "reader.book.current", current);
  state::set(shared, "reader.book.chapter", restore_position ? saved_chapter : std::int64_t{0});
  state::set(shared, "reader.book.page", restore_position ? saved_page : std::int64_t{0});
  state::set(shared, "reader.book.progress", restore_position ? saved_progress : std::int64_t{0});
  state::set(shared, "reader.book.title", self.current.document.title);
#ifdef ESP_PLATFORM
  ESP_LOGI("reader", "opened EPUB in %lld ms, spine=%u",
           static_cast<long long>((esp_timer_get_time() - started) / 1000),
           static_cast<unsigned>(self.current.document.spine_count));
#endif
  return true;
}

inline bool open_selected(context& self, state::store& shared) {
  const auto selected = state::get(shared, "reader.library.selected", std::int64_t{0});
  if (selected < 0 || selected >= static_cast<std::int64_t>(self.library.count)) return false;
  auto& book = self.library.books[static_cast<int>(selected)];
  return open_path(self, shared, book.path.data(), book.title.data(), selected);
}

inline void next_page(context& self, state::store& shared, geometry::rect text_rect, int scale) {
  if (turn_page(self.current, 1, text_rect, scale)) {
    state::set(shared, "reader.book.page", static_cast<std::int64_t>(self.current.page));
    state::set(shared, "reader.book.chapter",
               static_cast<std::int64_t>(self.current.current_chapter));
    state::set(shared, "reader.book.progress",
               static_cast<std::int64_t>(progress_percent(self.current)));
  }
}

inline void previous_page(context& self, state::store& shared, geometry::rect text_rect,
                          int scale) {
  if (turn_page(self.current, -1, text_rect, scale)) {
    state::set(shared, "reader.book.page", static_cast<std::int64_t>(self.current.page));
    state::set(shared, "reader.book.chapter",
               static_cast<std::int64_t>(self.current.current_chapter));
    state::set(shared, "reader.book.progress",
               static_cast<std::int64_t>(progress_percent(self.current)));
  }
}

inline void adjust_font(state::store& shared, int delta) {
  auto value = state::get(shared, "reader.settings.font_size", std::int64_t{1}) + delta;
  state::set(shared, "reader.settings.font_size",
             std::max<std::int64_t>(0, std::min<std::int64_t>(2, value)));
}

}  // namespace reader
