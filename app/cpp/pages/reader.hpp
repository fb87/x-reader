#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

namespace app::pages::reader {

inline int scale(const context& self) {
  return 2 +
         static_cast<int>(state::get(*self.memory, "reader.settings.font_size", std::int64_t{1}));
}

inline geometry::rect text_rect(const context& self) {
  const bool chrome = state::get(*self.memory, "app.reader.chrome", false);
  const int top = chrome ? 60 : 28;
  const int bottom = chrome ? 88 : 36;
  return geometry::rect{28, top, self.shell.display->width - 56,
                        self.shell.display->height - top - bottom - 24};
}

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  const bool chrome = state::get(*self.memory, "app.reader.chrome", false);
  if (chrome) draw_status(self, state::get(*self.memory, "reader.book.title", "Reading"));

  const int font_scale = std::min(scale(self), 4);
  const auto area = text_rect(self);
  ::reader::paginate(self.reader.current, area, font_scale);
  state::set(*self.memory, "reader.book.page", static_cast<std::int64_t>(self.reader.current.page));
  state::set(*self.memory, "reader.book.chapter",
             static_cast<std::int64_t>(self.reader.current.current_chapter));
  state::set(*self.memory, "reader.book.progress",
             static_cast<std::int64_t>(::reader::progress_percent(self.reader.current)));
  const char* body = ::reader::current_text(self.reader.current);
  std::size_t offset = self.reader.current.page_start[self.reader.current.page];
  int y = area.y;
  for (int line = 0; line < self.reader.current.lines_per_page && body[offset] != '\0'; ++line) {
    std::size_t next = 0;
    const std::size_t length = ::reader::line_span(body + offset, area.w, font_scale, next);
    char text_line[256]{};
    const std::size_t copied = std::min(length, sizeof(text_line) - 1);
    std::memcpy(text_line, body + offset, copied);
    text::draw(d, area.x, y, text_line, font_scale, canvas::gray::black);
    y += 7 * font_scale + 4;
    offset += next;
  }

  const int bottom = chrome ? 88 : 36;
  const int footer_y = d.height - bottom - 20;
  if (state::get(*self.memory, "reader.settings.show_progress", true)) {
    const int progress =
        std::max(0, static_cast<int>(::reader::progress_percent(self.reader.current)));
    canvas::hline(d, 28, footer_y, d.width - 56, canvas::gray::light);
    canvas::fill(d, {28, footer_y - 1, (d.width - 56) * progress / 100, 3}, canvas::gray::dark);
  }
  char pages[32]{};
  std::snprintf(pages, sizeof(pages), "%d / %d", self.reader.current.page + 1,
                self.reader.current.page_count);
  text::draw(d, 28, footer_y + 16, ::reader::current_chapter_title(self.reader.current), 1,
             canvas::gray::dark);
  text::draw(d, d.width - text::width(pages, 1) - 28, footer_y + 16, pages, 1, canvas::gray::dark);
  if (chrome) {
    const char* actions[] = {"Close", "A-", "A+"};
    draw_dock(self, actions, 3);
  }
}

inline bool event(context& self, const event::value& value) {
  const auto area = text_rect(self);
  const int font_scale = std::min(scale(self), 4);
  if (const int dock = dock_tap(self, value, 3);
      dock >= 0 && state::get(*self.memory, "app.reader.chrome", false)) {
    state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::dock));
    state::set(*self.memory, "app.dock.selected", static_cast<std::int64_t>(dock));
    if (dock == 0) {
      if (!routes::back(self)) routes::set_page(self, page::home);
    } else {
      ::reader::adjust_font(*self.memory, dock == 1 ? -1 : 1);
      self.next_refresh = refresh::mode::quality;
      ++self.invalidations;
    }
    return true;
  }
  if (value.event_type == event::type::tap) {
    const int third = self.shell.display->width / 3;
    if (value.x < third)
      ::reader::previous_page(self.reader, *self.memory, area, font_scale);
    else if (value.x >= third * 2)
      ::reader::next_page(self.reader, *self.memory, area, font_scale);
    else
      state::set(*self.memory, "app.reader.chrome",
                 !state::get(*self.memory, "app.reader.chrome", false));
    self.next_refresh = refresh::mode::quality;
    ++self.invalidations;
    return true;
  }
  if (value.event_type != event::type::key) return false;
  if (static_cast<focus_area>(state::get(*self.memory, "app.focus.area",
                                         static_cast<std::int64_t>(focus_area::content))) ==
      focus_area::dock) {
    const auto selected = state::get(*self.memory, "app.dock.selected", std::int64_t{0});
    if (value.key == event::key_code::up) {
      if (selected > 0)
        state::set(*self.memory, "app.dock.selected", selected - 1);
      else
        state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content));
      ++self.invalidations;
      self.next_refresh = refresh::mode::quality;
      return true;
    }
    if (value.key == event::key_code::left || value.key == event::key_code::right) {
      auto next = selected + (value.key == event::key_code::left ? -1 : 1);
      next = std::max<std::int64_t>(0, std::min<std::int64_t>(2, next));
      state::set(*self.memory, "app.dock.selected", next);
      ++self.invalidations;
      self.next_refresh = refresh::mode::quality;
      return true;
    }
    if (value.key == event::key_code::down) {
      if (selected < 2) {
        state::set(*self.memory, "app.dock.selected", selected + 1);
        ++self.invalidations;
      }
      self.next_refresh = refresh::mode::quality;
      return true;
    }
    if (value.key == event::key_code::ok) {
      if (selected == 0) {
        if (!routes::back(self)) routes::set_page(self, page::home);
      } else {
        ::reader::adjust_font(*self.memory, selected == 1 ? -1 : 1);
        self.next_refresh = refresh::mode::quality;
        ++self.invalidations;
      }
      return true;
    }
  }
  if (value.key == event::key_code::back) {
    if (!routes::back(self)) routes::set_page(self, page::home);
    ++self.invalidations;
    return true;
  }
  if (value.key == event::key_code::right || value.key == event::key_code::down) {
    if (value.key == event::key_code::down &&
        state::get(*self.memory, "app.reader.chrome", false)) {
      state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::dock));
    } else {
      ::reader::next_page(self.reader, *self.memory, area, font_scale);
    }
    self.next_refresh = refresh::mode::quality;
    ++self.invalidations;
    return true;
  }
  if (value.key == event::key_code::left || value.key == event::key_code::up) {
    ::reader::previous_page(self.reader, *self.memory, area, font_scale);
    self.next_refresh = refresh::mode::quality;
    ++self.invalidations;
    return true;
  }
  if (value.key == event::key_code::menu || value.key == event::key_code::ok) {
    state::set(*self.memory, "app.reader.chrome",
               !state::get(*self.memory, "app.reader.chrome", false));
    self.next_refresh = refresh::mode::quality;
    ++self.invalidations;
    return true;
  }
  return false;
}

}  // namespace app::pages::reader
