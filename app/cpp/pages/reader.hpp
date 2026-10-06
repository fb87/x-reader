#pragma once

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"

#include <cstdio>

namespace app::pages::reader {

inline void render(context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  const bool chrome = state::get(*self.memory, "app.reader.chrome", false);
  if (chrome) draw_status(self, state::get(*self.memory, "reader.book.title", "READING"));
  const int top = chrome ? 60 : 28;
  const int bottom = chrome ? 88 : 36;
  const auto page_number = state::get(*self.memory, "reader.book.page", std::int64_t{0});
  char line[64]{};
  std::snprintf(line, sizeof(line), "CHAPTER 1  PAGE %lld",
                static_cast<long long>(page_number + 1));
  text::draw(d, 28, top + 10, line, 2, canvas::gray::dark);
  const char* body[] = {
      "IT IS A TRUTH UNIVERSALLY ACKNOWLEDGED,",
      "THAT A SINGLE MAN IN POSSESSION OF A GOOD",
      "FORTUNE, MUST BE IN WANT OF A WIFE.",
      "",
      "HOWEVER LITTLE KNOWN THE FEELINGS OR VIEWS",
      "OF SUCH A MAN MAY BE ON HIS FIRST ENTERING",
      "A NEIGHBOURHOOD, THIS TRUTH IS SO WELL FIXED",
      "IN THE MINDS OF THE SURROUNDING FAMILIES.",
  };
  int y = top + 58;
  const int scale = 2 + static_cast<int>(
      state::get(*self.memory, "reader.settings.font_size", std::int64_t{1}));
  for (const char* paragraph : body) {
    text::draw(d, 28, y, paragraph, scale > 3 ? 3 : scale);
    y += 38;
  }
  if (state::get(*self.memory, "reader.settings.show_progress", true)) {
    const int progress = static_cast<int>(
        state::get(*self.memory, "reader.book.progress", std::int64_t{0}));
    canvas::border(d, {28, d.height - bottom, d.width - 56, 10}, 1, canvas::gray::light);
    canvas::fill(d, {29, d.height - bottom + 1, (d.width - 58) * progress / 100, 8},
                 canvas::gray::dark);
  }
  if (chrome) {
    const char* actions[] = {"CLOSE", "A-", "A+"};
    draw_dock(self, actions, 3);
  }
}

inline bool activate_dock(context& self) {
  const int action = static_cast<int>(
      state::get(*self.memory, "app.dock.selected", std::int64_t{0}));
  if (action == 0) {
    routes::set_page(self, page::home);
    return true;
  }
  ::reader::adjust_font(*self.memory, action == 1 ? -1 : 1);
  ++self.invalidations;
  return true;
}

inline bool event(context& self, const event::value& value) {
  if (value.event_type == event::type::tap) {
    const int third = self.shell.display->width / 3;
    if (value.x < third)
      ::reader::previous_page(*self.memory);
    else if (value.x >= third * 2)
      ::reader::next_page(*self.memory);
    else
      state::set(*self.memory, "app.reader.chrome",
                 !state::get(*self.memory, "app.reader.chrome", false));
    ++self.invalidations;
    return true;
  }
  if (value.event_type != event::type::key) return false;
  if (value.key == event::key_code::back) {
    routes::set_page(self, page::home);
    return true;
  }

  const auto area = static_cast<focus_area>(state::get(
      *self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content)));
  if (area == focus_area::dock) {
    auto selected = state::get(*self.memory, "app.dock.selected", std::int64_t{0});
    if (value.key == event::key_code::up) {
      state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::content));
      ++self.invalidations;
      return true;
    }
    if (value.key == event::key_code::left && selected > 0) {
      state::set(*self.memory, "app.dock.selected", selected - 1);
      ++self.invalidations;
      return true;
    }
    if (value.key == event::key_code::right && selected < 2) {
      state::set(*self.memory, "app.dock.selected", selected + 1);
      ++self.invalidations;
      return true;
    }
    if (value.key == event::key_code::down) return true;
    if (value.key == event::key_code::ok) return activate_dock(self);
  }

  if (value.key == event::key_code::right || value.key == event::key_code::down) {
    if (value.key == event::key_code::down &&
        state::get(*self.memory, "app.reader.chrome", false)) {
      state::set(*self.memory, "app.focus.area", static_cast<std::int64_t>(focus_area::dock));
    } else {
      ::reader::next_page(*self.memory);
    }
    ++self.invalidations;
    return true;
  }
  if (value.key == event::key_code::left || value.key == event::key_code::up) {
    ::reader::previous_page(*self.memory);
    ++self.invalidations;
    return true;
  }
  if (value.key == event::key_code::menu || value.key == event::key_code::ok) {
    state::set(*self.memory, "app.reader.chrome",
               !state::get(*self.memory, "app.reader.chrome", false));
    ++self.invalidations;
    return true;
  }
  return false;
}

}  // namespace app::pages::reader
