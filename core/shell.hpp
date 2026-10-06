#pragma once

#include "canvas.hpp"
#include "dialog.hpp"
#include "display.hpp"
#include "event.hpp"
#include "geometry.hpp"
#include "page.hpp"
#include "platform.hpp"
#include "refresh.hpp"
#include "text.hpp"
#include "widget.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace dialog {
// Forward declaration: defined below once shell::context is complete (see the
// comment at the bottom of this file); shell::detail::compose needs it earlier.
void default_render(context& d, canvas::surface& c);
}  // namespace dialog

/**
 * @brief Owns the screen, ported from `xr_shell_t` (include/xr/xr_shell.h,
 * src/xr_shell.c).
 *
 * Regions (layout):   status bar | page area | dock
 * Layers (z-order):    0 background, 1 content (chrome + top page), 2 dialogs
 *
 * Everything that changes calls `shell::invalidate(rect, mode)`. The
 * board's main loop then calls `shell::flush()`, which recomposes each
 * dirty rect bottom-to-top into the single framebuffer and pushes it to
 * the panel with the requested waveform.
 *
 * This header also defines the shell-dependent halves of `page::` and
 * `dialog::` (declared, not defined, in core/page.hpp / core/dialog.hpp)
 * by reopening those namespaces now that `shell::context` is complete --
 * see the comments at the top of those two files for why.
 */
namespace shell {

inline constexpr int max_pages = 8;
inline constexpr int max_dialogs = 4;

/** @brief Fonts and layout constants shared by shell-drawn chrome (status bar, dock). */
struct theme {
  const text::font* font_small = nullptr;
  const text::font* font_normal = nullptr;
  const text::font* font_bold = nullptr;
  const text::font* font_title = nullptr;
  const text::font* font_body = nullptr;
  std::int16_t status_h = 0;
  std::int16_t dock_h = 0;
  std::int16_t pad = 0;
  std::int16_t row_h = 0;
};

struct context;

/** @brief Draws layer 0, under all chrome/pages/dialogs; nullptr = plain white fill. */
using background_fn = void (*)(context& self, canvas::surface& c, void* user);

/** @brief Owns the page stack, dialog stack, canvas, and refresh scheduler for one screen. */
struct context {
  display::device* display = nullptr;
  platform::device* platform = nullptr;
  const theme* theme_ptr = nullptr;
  canvas::surface canvas{};
  refresh::scheduler sched{};
  geometry::rect screen{};
  geometry::rect status_rect{};
  geometry::rect dock_rect{};

  std::array<page::context*, max_pages> pages{};
  std::uint8_t page_count = 0;
  int dock_focus = -1;  ///< selected dock action for keyboard navigation, -1 = page content.
  std::array<dialog::context*, max_dialogs> dialogs{};
  std::uint8_t dialog_count = 0;

  background_fn background = nullptr;
  void* background_user = nullptr;

  char clock[8] = {0};
  int battery = -1;
  std::uint32_t last_input_ms = 0;
};

inline void invalidate(context& s, geometry::rect r, refresh::mode mode) {
  refresh::invalidate(s.sched, r, mode);
}

namespace detail {
/** @brief Routes a widget-tree invalidation (opaque `context*`) into `shell::invalidate`. */
inline void widget_notify_trampoline(void* ctx, geometry::rect r, refresh::mode mode) {
  invalidate(*static_cast<context*>(ctx), r, mode);
}
}  // namespace detail

/** @brief Initializes a shell over a board's display/platform capabilities and a theme. */
inline void init(context& s, display::device& display, platform::device& platform,
                 const theme& t) {
  s = context{};
  s.display = &display;
  s.platform = &platform;
  s.theme_ptr = &t;
  canvas::init(s.canvas, display.framebuffer, display.width, display.height, display.stride,
              display.format);
  s.screen = geometry::make(0, 0, display.width, display.height);
  s.status_rect = geometry::make(0, 0, display.width, t.status_h);
  s.dock_rect = geometry::make(0, display.height - t.dock_h, display.width, t.dock_h);
  refresh::init(s.sched, s.screen, display.update_align);
  s.battery = -1;
  s.last_input_ms = 0;
  s.dock_focus = -1;
}

/** @brief Installs a custom background painter, invalidating the whole screen once. */
inline void set_background(context& s, background_fn fn, void* user) {
  s.background = fn;
  s.background_user = user;
  invalidate(s, s.screen, refresh::mode::quality);
}

/** @brief Promotes a QUALITY update to a full flash after N QUALITY updates (0 = never). */
inline void set_full_refresh_every(context& s, std::uint16_t n) { s.sched.full_every = n; }

/** @brief Current monotonic time from the shell's platform capability. */
inline std::uint32_t now(const context& s) { return platform::now_ms(*s.platform); }

/** @brief The content rect left over for a page requesting `chrome_flags`. */
inline geometry::rect page_area(const context& s, std::uint8_t chrome_flags) {
  const int y0 = (chrome_flags & page::chrome::status) ? s.status_rect.h : 0;
  const int y1 = (chrome_flags & page::chrome::dock) ? s.dock_rect.y : s.screen.h;
  return geometry::make(0, y0, s.screen.w, y1 - y0);
}

// --------------------------------------------------------------- navigation

/** @brief The page currently on top of the stack, or nullptr if none. */
inline page::context* top(const context& s) {
  return s.page_count != 0 ? s.pages[s.page_count - 1] : nullptr;
}

namespace detail {
inline void attach_page(context& s, page::context& p) {
  p.shell = &s;
  p.area = page_area(s, p.chrome);
  widget::init(p.scope, p.area);
  p.scope.root.notify = widget_notify_trampoline;
  p.scope.root.notify_context = &s;
}

/** Dialogs belong to the screen that opened them. */
inline void drop_dialogs(context& s) { s.dialog_count = 0; }

inline void enter_page(context& s, page::context& p, bool created) {
  s.dock_focus = -1;
  if (created) {
    attach_page(s, p);
    if (p.vt->on_create != nullptr) p.vt->on_create(p);
    if (p.scope.focus == nullptr) widget::focus_first(p.scope);
  }
  if (p.vt->on_enter != nullptr) p.vt->on_enter(p);
  invalidate(s, s.screen, p.enter_refresh);
}

inline void leave_page(page::context& p, bool destroy) {
  if (p.vt->on_exit != nullptr) p.vt->on_exit(p);
  if (destroy && p.vt->on_destroy != nullptr) p.vt->on_destroy(p);
}
}  // namespace detail

/** @brief Pushes a new top page; the previous top is left (not destroyed). */
inline void push(context& s, page::context& p) {
  if (s.page_count >= max_pages) return;
  detail::drop_dialogs(s);
  if (page::context* old_top = top(s)) detail::leave_page(*old_top, false);
  s.pages[s.page_count++] = &p;
  detail::enter_page(s, p, true);
}

/** @brief Pops the top page (destroying it) back to the one beneath; no-op with one page left. */
inline void pop(context& s) {
  if (s.page_count <= 1) return;
  detail::drop_dialogs(s);
  detail::leave_page(*s.pages[--s.page_count], true);
  detail::enter_page(s, *top(s), false);
}

/** @brief Replaces the top page with `p` (destroying the old top); pushes if the stack is empty. */
inline void replace(context& s, page::context& p) {
  if (s.page_count == 0) {
    push(s, p);
    return;
  }
  detail::drop_dialogs(s);
  detail::leave_page(*s.pages[s.page_count - 1], true);
  s.pages[s.page_count - 1] = &p;
  detail::enter_page(s, p, true);
}

/** @brief Changes a page's requested chrome, re-laying it out if it's currently on top. */
inline void set_chrome(context& s, page::context& p, std::uint8_t chrome_flags) {
  if (p.chrome == chrome_flags) return;
  p.chrome = chrome_flags;
  if (top(s) != &p) return;
  if ((chrome_flags & page::chrome::dock) == 0) s.dock_focus = -1;
  p.area = page_area(s, chrome_flags);
  p.scope.root.rect = p.area;
  if (p.vt->on_layout != nullptr) p.vt->on_layout(p);
  invalidate(s, s.screen, refresh::mode::quality);
}

// ------------------------------------------------------------------ dialogs

/** @brief Shows a dialog on top of the current screen, laying it out within the padded screen. */
inline void show_dialog(context& s, dialog::context& d) {
  if (s.dialog_count >= max_dialogs) return;
  d.shell = &s;
  const geometry::rect bounds = geometry::inset(s.screen, s.theme_ptr->pad);
  d.vt->layout(d, bounds);
  d.scope.root.notify = detail::widget_notify_trampoline;
  d.scope.root.notify_context = &s;
  if (d.scope.focus == nullptr) widget::focus_first(d.scope);
  s.dialogs[s.dialog_count++] = &d;
  invalidate(s, dialog::dirty_rect(d), refresh::mode::quality);
}

/** @brief Closes a dialog, invalidating what was underneath, then fires its result callback. */
inline void close_dialog(context& s, dialog::context& d, dialog::result result) {
  int idx = -1;
  for (int i = 0; i < s.dialog_count; ++i) {
    if (s.dialogs[i] == &d) idx = i;
  }
  if (idx < 0) return;
  for (int i = idx; i < s.dialog_count - 1; ++i) s.dialogs[i] = s.dialogs[i + 1];
  --s.dialog_count;
  // What was underneath is recomposed from the lower layers.
  invalidate(s, dialog::dirty_rect(d), refresh::mode::quality);
  // Callback last: it may navigate or open another dialog.
  if (d.on_result != nullptr) d.on_result(d, result, d.user);
}

// ------------------------------------------------------------------- input

namespace detail {
inline int dock_action_at(const context& s, const page::context& p, int x) {
  if (p.action_count == 0) return -1;
  const int i = x * p.action_count / s.screen.w;
  return (i >= 0 && i < p.action_count) ? i : -1;
}

inline void fire_action(page::context& p, int i) {
  if (p.vt->on_action != nullptr) p.vt->on_action(p, p.actions[i].id);
}

inline void set_dock_focus(context& s, int focus) {
  if (s.dock_focus == focus) return;
  s.dock_focus = focus;
  invalidate(s, s.dock_rect, refresh::mode::fast);
}
}  // namespace detail

/** @brief Routes one input event through dialogs (modal), then shell chrome, then the page. */
inline void dispatch(context& s, const event::value& ev) {
  s.last_input_ms = now(s);
  // Layer 2: the top dialog is modal and takes everything.
  if (s.dialog_count != 0) {
    dialog::context& d = *s.dialogs[s.dialog_count - 1];
    if (ev.kind == event::type::tap && !geometry::contains(d.rect, ev.x, ev.y)) {
      close_dialog(s, d, dialog::result::cancel);
      return;
    }
    if (d.vt->on_event != nullptr && d.vt->on_event(d, ev)) return;
    if (widget::handle_event(d.scope, ev)) return;
    if (ev.kind == event::type::key && ev.key == event::key_code::back) {
      close_dialog(s, d, dialog::result::cancel);
    }
    return;
  }

  page::context* page_ptr = top(s);
  if (page_ptr == nullptr) return;
  page::context& p = *page_ptr;

  if (ev.kind == event::type::key && ev.long_press) {
    // Rotary holds move directly between long content and the dock.
    if (ev.key == event::key_code::down && (p.chrome & page::chrome::dock) != 0 &&
        p.action_count != 0) {
      detail::set_dock_focus(s, 0);
      return;
    }
    if (ev.key == event::key_code::up && s.dock_focus >= 0) {
      detail::set_dock_focus(s, -1);
      return;
    }
  }

  if (ev.kind == event::type::key && s.dock_focus >= 0) {
    const int n = p.action_count;
    if ((p.chrome & page::chrome::dock) == 0 || s.dock_focus >= n) {
      detail::set_dock_focus(s, -1);
    } else {
      switch (ev.key) {
        case event::key_code::down:
          detail::set_dock_focus(s, (s.dock_focus + 1) % n);
          return;
        case event::key_code::up:
          if (s.dock_focus == 0) detail::set_dock_focus(s, -1);
          else detail::set_dock_focus(s, s.dock_focus - 1);
          return;
        case event::key_code::ok:
          detail::fire_action(p, s.dock_focus);
          return;
        default:
          break;
      }
    }
  }

  // Shell chrome.
  if (ev.kind == event::type::tap) {
    if ((p.chrome & page::chrome::dock) != 0 && geometry::contains(s.dock_rect, ev.x, ev.y)) {
      const int i = detail::dock_action_at(s, p, ev.x);
      if (i >= 0) detail::fire_action(p, i);
      return;
    }
    if ((p.chrome & page::chrome::status) != 0 && geometry::contains(s.status_rect, ev.x, ev.y)) {
      return;
    }
  }

  // Layer 1: the page, then its focused widget / focus navigation.
  if (p.vt->on_event != nullptr && p.vt->on_event(p, ev)) return;
  if (widget::handle_event(p.scope, ev)) return;

  if (ev.kind == event::type::key) {
    if (ev.key == event::key_code::down && (p.chrome & page::chrome::dock) != 0 &&
        p.action_count != 0) {
      detail::set_dock_focus(s, 0);
      return;
    }
    for (int i = 0; i < p.action_count; ++i) {
      if (p.actions[i].key != event::key_code::none && p.actions[i].key == ev.key) {
        detail::fire_action(p, i);
        return;
      }
    }
    if (ev.key == event::key_code::back) pop(s);
  }
}

// -------------------------------------------------------------------- time

/** @brief Advances the top page's on_tick and redraws the status bar if clock/battery changed. */
inline void tick(context& s) {
  page::context* page_ptr = top(s);
  if (page_ptr != nullptr && page_ptr->vt->on_tick != nullptr) {
    page_ptr->vt->on_tick(*page_ptr, now(s));
  }

  int hh = 0;
  int mm = 0;
  const bool have_wall_time = platform::wall_time(*s.platform, hh, mm);
  char new_clock[8] = {0};
  std::snprintf(new_clock, sizeof(new_clock), have_wall_time ? "%02d:%02d" : "--:--",
                hh % 24, mm % 60);
  const int batt = platform::battery_percent(*s.platform);
  if (std::strcmp(new_clock, s.clock) != 0 || batt != s.battery) {
    std::memcpy(s.clock, new_clock, sizeof(new_clock));
    s.battery = batt;
    page_ptr = top(s);
    if (page_ptr != nullptr && (page_ptr->chrome & page::chrome::status) != 0) {
      invalidate(s, s.status_rect, refresh::mode::fast);
    }
  }
}

// ---------------------------------------------------------------- rendering

namespace detail {
inline void render_status(context& s, canvas::surface& c, const page::context& p) {
  const theme& t = *s.theme_ptr;
  const geometry::rect r = s.status_rect;
  canvas::fill(c, r, canvas::gray::white);
  canvas::fill(c, geometry::make(r.x, r.y + r.h - 2, r.w, 2), canvas::gray::black);

  const geometry::rect inner = geometry::make(r.x + t.pad, r.y, r.w - 2 * t.pad, r.h - 2);
  int x = inner.x + inner.w;

  if (s.battery >= 0) {  // battery icon + percent, right aligned.
    const int bw = 26;
    const int bh = 13;
    const int by = inner.y + (inner.h - bh) / 2;
    x -= 3;
    canvas::fill(c, geometry::make(x, by + 4, 3, bh - 8), canvas::gray::black);
    x -= bw;
    canvas::border(c, geometry::make(x, by, bw, bh), 2, canvas::gray::black);
    const int fill = (bw - 6) * s.battery / 100;
    canvas::fill(c, geometry::make(x + 3, by + 3, fill, bh - 6), canvas::gray::black);
    char pct[8] = {0};
    std::snprintf(pct, sizeof(pct), "%d%%", s.battery);
    const int pw = text::width(*t.font_small, pct, -1);
    x -= pw + 6;
    canvas::draw_text_in(c, *t.font_small, geometry::make(x, inner.y, pw, inner.h), pct,
                         text::align::left, canvas::gray::black);
  }
  const int cw = text::width(*t.font_bold, s.clock, -1);
  x -= cw + t.pad;
  canvas::draw_text_in(c, *t.font_bold, geometry::make(x, inner.y, cw, inner.h), s.clock,
                       text::align::left, canvas::gray::black);

  if (p.title != nullptr) {
    canvas::draw_text_in(c, *t.font_bold,
                         geometry::make(inner.x, inner.y, x - inner.x - t.pad, inner.h), p.title,
                         text::align::left, canvas::gray::black);
  }
}

inline void render_dock(context& s, canvas::surface& c, const page::context& p) {
  const theme& t = *s.theme_ptr;
  const geometry::rect r = s.dock_rect;
  canvas::fill(c, r, canvas::gray::white);
  canvas::fill(c, geometry::make(r.x, r.y, r.w, 2), canvas::gray::black);
  const int n = p.action_count;
  for (int i = 0; i < n; ++i) {
    const int x0 = r.w * i / n;
    const int x1 = r.w * (i + 1) / n;
    const bool focused = s.dock_focus == i;
    if (focused) canvas::fill(c, geometry::make(x0, r.y + 2, x1 - x0, r.h - 2), canvas::gray::black);
    if (i > 0) canvas::vline(c, x0, r.y + 14, r.h - 28, canvas::gray::light);
    const std::uint8_t fg = focused ? canvas::gray::white : canvas::gray::black;
    geometry::rect label = geometry::make(x0 + 4, r.y + 2, x1 - x0 - 8, r.h - 2);
    if (p.actions[i].icon != nullptr) {
      const geometry::rect icon = geometry::make(x0 + (x1 - x0 - 18) / 2, r.y + 5, 18, 18);
      p.actions[i].icon(c, icon, fg);
      label = geometry::make(x0 + 4, r.y + 27, x1 - x0 - 8, r.h - 29);
    }
    canvas::draw_text_in(c, *t.font_normal, label, p.actions[i].label, text::align::center, fg);
  }
}

inline void compose(context& s, geometry::rect clip) {
  canvas::surface& c = s.canvas;
  const geometry::rect saved = canvas::set_clip(c, clip);

  // Layer 0: background.
  if (s.background != nullptr) s.background(s, c, s.background_user);
  else canvas::fill(c, clip, canvas::gray::white);

  // Layer 1: content = chrome + top page (pages below are fully covered).
  if (page::context* page_ptr = top(s)) {
    page::context& p = *page_ptr;
    if ((p.chrome & page::chrome::status) != 0 && geometry::intersects(clip, s.status_rect)) {
      canvas::set_clip(c, geometry::intersect(clip, s.status_rect));
      render_status(s, c, p);
    }
    if (geometry::intersects(clip, p.area)) {
      canvas::set_clip(c, geometry::intersect(clip, p.area));
      if (p.vt->render != nullptr) p.vt->render(p, c);
      else widget::render_tree(p.scope.root, c);
    }
    if ((p.chrome & page::chrome::dock) != 0 && geometry::intersects(clip, s.dock_rect)) {
      canvas::set_clip(c, geometry::intersect(clip, s.dock_rect));
      render_dock(s, c, p);
    }
  }

  // Layer 2: dialogs, bottom to top.
  for (int i = 0; i < s.dialog_count; ++i) {
    dialog::context& d = *s.dialogs[i];
    const geometry::rect dr = dialog::dirty_rect(d);
    if (!geometry::intersects(clip, dr)) continue;
    canvas::set_clip(c, geometry::intersect(clip, dr));
    if (d.vt->render != nullptr) d.vt->render(d, c);
    else dialog::default_render(d, c);
  }

  canvas::set_clip(c, saved);
}
}  // namespace detail

/** @brief Composes and pushes every pending dirty region to the panel, then waits for it. */
inline void flush(context& s) {
  refresh::dirty next{};
  while (refresh::take(s.sched, next)) {
    detail::compose(s, next.rect);
    display::update(*s.display, next.rect, next.kind);
  }
}

}  // namespace shell

// -------------------------------------------------------------------------
// Shell-dependent halves of page:: and dialog:: (see core/page.hpp and
// core/dialog.hpp for why these live here instead of there).
// -------------------------------------------------------------------------

namespace page {

/** @brief Replaces a page's dock actions, invalidating the dock if this page is on top. */
inline void set_actions(context& p, const action* actions, std::uint8_t count) {
  p.actions = actions;
  p.action_count = count;
  if (p.shell != nullptr && shell::top(*p.shell) == &p && (p.chrome & chrome::dock) != 0) {
    shell::invalidate(*p.shell, p.shell->dock_rect, refresh::mode::quality);
  }
}

/** @brief Replaces a page's status-bar title, invalidating the status bar if this page is on top. */
inline void set_title(context& p, const char* title) {
  p.title = title;
  if (p.shell != nullptr && shell::top(*p.shell) == &p && (p.chrome & chrome::status) != 0) {
    shell::invalidate(*p.shell, p.shell->status_rect, refresh::mode::quality);
  }
}

/** @brief Invalidates the page's own content area. */
inline void invalidate(context& p, refresh::mode mode) {
  if (p.shell != nullptr) shell::invalidate(*p.shell, p.area, mode);
}

}  // namespace page

namespace dialog {

/** @brief Height of the title bar, or 0 when the dialog has no title. */
inline int header_height(const context& d) {
  if (d.title == nullptr) return 0;
  const shell::theme& t = *d.shell->theme_ptr;
  return t.font_bold->line_height + t.pad;
}

/** @brief Draws the dialog's shadow, border, and optional title bar (not its content widgets). */
inline void render_frame(context& d, canvas::surface& c) {
  const geometry::rect r = d.rect;
  canvas::stipple(c, geometry::make(r.x + shadow, r.y + shadow, r.w, r.h), canvas::gray::black);
  canvas::fill(c, r, canvas::gray::white);
  canvas::border(c, r, 3, canvas::gray::black);
  if (d.title != nullptr) {
    const shell::theme& t = *d.shell->theme_ptr;
    const int hh = header_height(d);
    geometry::rect title_rect = geometry::make(r.x + t.pad, r.y + 3, r.w - 2 * t.pad, hh);
    if (d.icon != nullptr) {
      const geometry::rect icon_rect =
          geometry::make(title_rect.x, title_rect.y + (title_rect.h - 18) / 2, 18, 18);
      d.icon(c, icon_rect, canvas::gray::black);
      title_rect.x += icon_rect.w + 8;
      title_rect.w -= icon_rect.w + 8;
    }
    canvas::draw_text_in(c, *t.font_bold, title_rect, d.title, text::align::left,
                         canvas::gray::black);
    canvas::hline(c, r.x + 3, r.y + 3 + hh, r.w - 6, canvas::gray::black);
  }
}

/** @brief Default render: frame plus the dialog's own widget tree. */
inline void default_render(context& d, canvas::surface& c) {
  render_frame(d, c);
  widget::render_tree(d.scope.root, c);
}

/** @brief Closes the dialog through its owning shell, if attached. */
inline void close(context& d, result value) {
  if (d.shell != nullptr) shell::close_dialog(*d.shell, d, value);
}

// ------------------------------------------------------------- confirm (Yes/No)

/** @brief A two-button Yes/No confirmation dialog, sized to its message text. */
struct confirm {
  context base{};
  const char* message = nullptr;
  const char* yes_text = nullptr;
  const char* no_text = nullptr;
  widget::label label{};
  widget::button yes{};
  widget::button no{};
};

namespace detail {
/** @brief Recovers `confirm` from its embedded `dialog::context` (named `base`). */
inline confirm& confirm_of(context& d) {
  return *reinterpret_cast<confirm*>(reinterpret_cast<char*>(&d) - offsetof(confirm, base));
}

inline void confirm_clicked(widget::base& w, void* user) {
  confirm& cd = *static_cast<confirm*>(user);
  close(cd.base, (&w == &cd.yes.base_widget) ? result::yes : result::no);
}

inline void confirm_layout(context& d, geometry::rect bounds) {
  confirm& cd = confirm_of(d);
  const shell::theme& t = *d.shell->theme_ptr;
  const int pad = t.pad;
  const int w = std::min(bounds.w * 88 / 100, 460);
  const int inner_w = w - 2 * pad;
  const int msg_h = text::measure_height(*t.font_normal, cd.message, inner_w);
  const int head = header_height(d);
  const int btn_h = t.row_h - 8;
  const int h = head + pad + msg_h + pad + btn_h + pad;

  place(d, bounds, w, h);
  const geometry::rect r = d.rect;
  int y = r.y + head + pad;

  widget::init(cd.label, geometry::make(r.x + pad, y, inner_w, msg_h), cd.message, *t.font_normal,
               text::align::left);
  cd.label.wrap = true;
  y += msg_h + pad;

  const int bw = (inner_w - pad) / 2;
  widget::init(cd.yes, geometry::make(r.x + pad, y, bw, btn_h), cd.yes_text, *t.font_bold,
               confirm_clicked, &cd);
  widget::init(cd.no, geometry::make(r.x + pad + bw + pad, y, bw, btn_h), cd.no_text, *t.font_bold,
               confirm_clicked, &cd);

  widget::add(d.scope.root, cd.label.base_widget);
  widget::add(d.scope.root, cd.yes.base_widget);
  widget::add(d.scope.root, cd.no.base_widget);
  widget::set_focus(d.scope, &cd.no.base_widget);
}

inline constexpr vtbl confirm_vtbl{confirm_layout, nullptr, nullptr};
}  // namespace detail

/** @brief Shows a Yes/No confirmation. Focus starts on "No": a stray press must not be
 * destructive. */
inline void show_confirm(shell::context& s, confirm& cd, const char* title, const char* message,
                         const char* yes_text, const char* no_text, result_fn cb, void* user) {
  init(cd.base, detail::confirm_vtbl, title);
  cd.message = message;
  cd.yes_text = yes_text != nullptr ? yes_text : "Yes";
  cd.no_text = no_text != nullptr ? no_text : "No";
  cd.base.on_result = cb;
  cd.base.user = user;
  shell::show_dialog(s, cd.base);
}

}  // namespace dialog
