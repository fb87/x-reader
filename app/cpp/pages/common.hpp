#pragma once

#include "core/canvas.hpp"
#include "core/platform.hpp"
#include "core/state.hpp"
#include "core/text.hpp"
#include "core/widgets/all.hpp"
#include "app/cpp/model.hpp"

namespace app::pages {

inline void draw_status(context& self, const char* title) {
  widget::status(*self.shell.display, *self.shell.platform, title);
}

inline void draw_row(context& self, int index, int y, const char* primary,
                     const char* secondary = nullptr) {
  const bool selected = selection(self) == index &&
      static_cast<focus_area>(state::get(*self.memory, "app.focus.area",
          static_cast<std::int64_t>(focus_area::content))) == focus_area::content;
  widget::row(*self.shell.display, {12, y, self.shell.display->width - 24, 68}, primary, secondary,
              selected, {}, icon::for_label(primary));
}

inline void draw_dock(context& self, const char* const* labels, int count) {
  const bool focused = static_cast<focus_area>(state::get(*self.memory, "app.focus.area",
      static_cast<std::int64_t>(focus_area::content))) == focus_area::dock;
  const int selected = static_cast<int>(
      state::get(*self.memory, "app.dock.selected", std::int64_t{0}));
  widget::dock(*self.shell.display, labels, count, selected, focused);
}

inline void draw_file_dock(context& self, const char* const* labels) {
  const bool focused = static_cast<focus_area>(state::get(*self.memory, "app.focus.area",
      static_cast<std::int64_t>(focus_area::content))) == focus_area::dock;
  const int selected = static_cast<int>(state::get(*self.memory, "app.dock.selected", std::int64_t{0}));
  const int icons[] = {XR_ICON_ARROW_BACK};
  widget::dock(*self.shell.display, labels, 1, selected, focused, {}, icons);
}

inline void draw_dialog(context& self, const char* title, const char* body) {
  widget::dialog(*self.shell.display, title, body);
}

inline bool event_is_activation(const event::value& value) {
  return value.event_type == event::type::tap ||
         (value.event_type == event::type::key && value.key == event::key_code::ok);
}

}  // namespace app::pages
