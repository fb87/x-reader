#pragma once

#include "../core/geometry.hpp"
#include "../core/state.hpp"
#include "../core/text.hpp"
#include "epub.hpp"
#include "library.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace reader {

inline constexpr std::size_t max_pages = 256;
inline constexpr std::uint16_t manifest_capacity = 640;
inline constexpr std::uint16_t spine_capacity = 640;
inline constexpr std::size_t scratch_size = 80U * 1024U;
inline constexpr std::size_t text_size = 64U * 1024U;

/** @brief Native EPUB session and bounded pagination state for the current book. */
struct session {
  epub::context document{};
  epub::file_view storage_view{};
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

/** @brief Loads one EPUB spine chapter. */
inline bool load_chapter(session& s, std::uint16_t chapter) {
  if (!s.epub_open || chapter >= s.document.spine_count) return false;
  std::uint32_t output = 0;
  if (epub::spine_text(s.document, chapter, s.scratch.data(), s.scratch.size(), s.text.data(),
                       s.text.size(), output) != epub::status::ok) return false;
  s.cover_placeholder = s.text[0] == '\0';
  if (s.cover_placeholder) std::strcpy(s.text.data(), "Cover");
  s.current_chapter = chapter;
  return true;
}

/** @brief Opens an EPUB and loads its first spine chapter. */
inline bool open(session& s, const epub::file_view& storage, const char* fallback_title) {
  s = session{};
  s.storage_view = storage;
  if (epub::open(s.document, s.storage_view, s.scratch.data(), s.scratch.size(), s.manifest.data(),
                 static_cast<std::uint16_t>(s.manifest.size()), s.spine.data(),
                 static_cast<std::uint16_t>(s.spine.size())) != epub::status::ok) return false;
  s.epub_open = true;
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
  while (*cursor != '\0' && *cursor != '\n') {
    const char* after = cursor;
    (void)text::next_codepoint(after);
    const std::size_t bytes = static_cast<std::size_t>(after - text);
    char line[256] = {0};
    if (bytes >= sizeof(line)) break;
    std::memcpy(line, text, bytes);
    if (text::width(line, scale) > max_width) break;
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
  const std::uint32_t current = static_cast<std::uint32_t>(s.page + 1) * 100U /
                                static_cast<std::uint32_t>(s.page_count);
  const std::uint32_t progress =
      (static_cast<std::uint32_t>(s.current_chapter) * 100U + current) /
      static_cast<std::uint32_t>(s.document.spine_count);
  return static_cast<int>(std::min<std::uint32_t>(100U, progress));
}

inline bool open_selected(context& self, state::store& shared) {
  const auto selected = state::get(shared, "reader.library.selected", std::int64_t{0});
  if (selected < 0 || selected >= static_cast<std::int64_t>(self.library.count) ||
      self.storage == nullptr) return false;
  auto& book = self.library.books[static_cast<int>(selected)];
  std::uint32_t size = 0;
  epub::file_view view{self.storage, book.path.data(), 0};
  if (!storage::size(*self.storage, view.path, size)) return false;
  view.size = size;
  if (!open(self.current, view, book.title.data())) return false;
  state::set(shared, "reader.book.current", selected);
  state::set(shared, "reader.book.page", std::int64_t{0});
  state::set(shared, "reader.book.progress", std::int64_t{0});
  state::set(shared, "reader.book.title", self.current.document.title);
  return true;
}

inline void next_page(context& self, state::store& shared, geometry::rect text_rect, int scale) {
  if (turn_page(self.current, 1, text_rect, scale)) {
    state::set(shared, "reader.book.page", static_cast<std::int64_t>(self.current.page));
    state::set(shared, "reader.book.progress", static_cast<std::int64_t>(progress_percent(self.current)));
  }
}

inline void previous_page(context& self, state::store& shared, geometry::rect text_rect, int scale) {
  if (turn_page(self.current, -1, text_rect, scale)) {
    state::set(shared, "reader.book.page", static_cast<std::int64_t>(self.current.page));
    state::set(shared, "reader.book.progress", static_cast<std::int64_t>(progress_percent(self.current)));
  }
}

inline void adjust_font(state::store& shared, int delta) {
  auto value = state::get(shared, "reader.settings.font_size", std::int64_t{1}) + delta;
  state::set(shared, "reader.settings.font_size", std::max<std::int64_t>(0, std::min<std::int64_t>(2, value)));
}

}  // namespace reader
