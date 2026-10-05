#pragma once

#include "canvas.hpp"
#include "event.hpp"
#include "geometry.hpp"
#include "widget.hpp"

#include <algorithm>
#include <cstdint>

/**
 * @brief Modal popups on the dialog layer, ported from `xr_dialog_t`
 * (include/xr/xr_dialog.h, src/xr_dialog.c).
 *
 * A dialog sizes itself to its content in `layout`, is centered by the
 * shell, takes all input while on top, and reports a result through a
 * callback when closed.
 *
 * Unlike the old C split, `place()` here does NOT touch the shell at all
 * (the old `xr_dialog_place` copied `d->shell` onto the widget root's
 * `shell` field; this tree's widget root instead carries an opaque
 * notify callback that only `shell::show_dialog` knows how to wire up).
 * `header_height`/`render_frame`/`default_render`/`close`, and the stock
 * confirm dialog (which needs the shell's theme for fonts/padding), all
 * need the complete `shell::context` type and so live in core/shell.hpp,
 * which reopens `namespace dialog` once that type exists.
 */
namespace shell {
struct context;
}  // namespace shell

namespace dialog {

inline constexpr int shadow = 6;

/** @brief Result passed to a dialog's close callback. */
enum class result { cancel = -1, no = 0, yes = 1 };

struct context;

/** @brief Fired when a dialog closes, with its result and the caller-supplied `user`. */
using result_fn = void (*)(context& self, result value, void* user);

/** @brief Function-pointer vtable for one dialog type. */
struct vtbl {
  /** Sets self.rect (sized from content, placed inside `bounds`) and builds widgets. The
   * shell centers nothing for you: use dialog::place(). */
  void (*layout)(context& self, geometry::rect bounds) = nullptr;
  void (*render)(context& self, canvas::surface& c) = nullptr;  ///< nullptr = default.
  bool (*on_event)(context& self, const event::value& ev) = nullptr;
};

/** @brief One dialog instance's state; subclass by embedding this as the first member. */
struct context {
  const vtbl* vt = nullptr;
  shell::context* shell = nullptr;
  const char* title = nullptr;  ///< nullptr = no title bar.
  widget::icon_fn icon = nullptr;
  geometry::rect rect{};
  widget::scope scope{};
  result_fn on_result = nullptr;
  void* user = nullptr;
};

/** @brief Initializes a dialog with an empty scope; not yet placed or shown. */
inline void init(context& d, const vtbl& vt, const char* title) {
  d = context{};
  d.vt = &vt;
  d.title = title;
  widget::init(d.scope, geometry::make(0, 0, 0, 0));
}

/** @brief Sets (or clears) the dialog's title-bar icon. */
inline void set_icon(context& d, widget::icon_fn icon) { d.icon = icon; }

/** @brief Centers a `w` x `h` dialog in `bounds` and resets its widget tree to that rect. */
inline void place(context& d, geometry::rect bounds, int w, int h) {
  w = std::min(w, static_cast<int>(bounds.w));
  h = std::min(h, static_cast<int>(bounds.h));
  d.rect = geometry::make(bounds.x + (bounds.w - w) / 2, bounds.y + (bounds.h - h) / 2, w, h);
  widget::init(d.scope, d.rect);
}

/** @brief The dialog's rect plus its drop-shadow margin; used to invalidate/clip it. */
inline geometry::rect dirty_rect(const context& d) {
  return geometry::make(d.rect.x, d.rect.y, d.rect.w + shadow, d.rect.h + shadow);
}

}  // namespace dialog
