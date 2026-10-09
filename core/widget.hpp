#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "canvas.hpp"
#include "event.hpp"
#include "geometry.hpp"
#include "refresh.hpp"
#include "text.hpp"

/**
 * @brief Widget base, focus scope, and the stock widgets (label/button/
 * list), ported from `xr_widget_t`/`xr_scope_t`/`xr_label_t`/`xr_button_t`/
 * `xr_list_t` (include/xr/xr_widget.h, src/xr_widget.c).
 *
 * Function-pointer vtables, no virtual functions. Widgets are statically
 * allocated by their owner (page/dialog); the tree is intrusive
 * (parent/first_child/next_sibling), so this layer never allocates.
 *
 * The old C code called `xr_shell_invalidate` directly from widget code,
 * resolved at the .c-file level via a forward-declared `struct xr_shell`.
 * Header-only components can't do that (it would make widget.hpp and
 * shell.hpp mutually dependent), so instead a tree ROOT carries an opaque
 * `notify` callback + context, set once when the shell attaches a page's
 * widget tree. This is the same "widget doesn't know what a shell is"
 * decoupling, expressed as a function pointer instead of a forward
 * declaration.
 */
namespace widget {

/** @brief Bit flags on `base::flags`. */
namespace flag {
inline constexpr std::uint8_t visible = 1u << 0;
inline constexpr std::uint8_t focusable = 1u << 1;
inline constexpr std::uint8_t focused = 1u << 2;
inline constexpr std::uint8_t disabled = 1u << 3;
}  // namespace flag

struct base;

/** @brief Function-pointer vtable; every widget subclass points at one static instance. */
struct vtbl {
  const char* type_name = nullptr;
  void (*render)(base& self, canvas::surface& c) = nullptr;
  /** Key events reach the focused widget; taps reach the hit widget. */
  bool (*on_event)(base& self, const event::value& ev) = nullptr;
  void (*on_focus)(base& self, bool focused) = nullptr;  ///< optional.
};

/** @brief "Clicked" callback, run by the stock button widget on activation. */
using activate_fn = void (*)(base& self, void* user);

/** @brief Intrusive widget tree node; every widget subclass embeds one as its first member. */
struct base {
  const vtbl* vt = nullptr;
  geometry::rect rect{};  ///< absolute screen coordinates.
  std::uint8_t flags = 0;
  refresh::mode refresh_hint = refresh::mode::fast;  ///< mode used when it invalidates itself.
  base* parent = nullptr;
  base* first_child = nullptr;
  base* next_sibling = nullptr;
  void (*notify)(void* context, geometry::rect area, refresh::mode mode) = nullptr;  ///< root only.
  void* notify_context = nullptr;                                                    ///< root only.
  activate_fn on_activate = nullptr;
  void* user = nullptr;
};

/** @brief Recovers the concrete widget owning `w`, assuming `w` is that type's first member. */
template <typename T>
inline T& container_of(base& w) {
  return *reinterpret_cast<T*>(reinterpret_cast<char*>(&w) - offsetof(T, base_widget));
}

namespace detail {
inline void container_render(base&, canvas::surface&) {}
}  // namespace detail

/** @brief The do-nothing vtable used for pure container nodes (e.g. a focus scope's root). */
inline constexpr vtbl container_vtbl{"container", detail::container_render, nullptr, nullptr};

/** @brief Initializes a widget node: visible, fast-refresh, no tree links yet. */
inline void init(base& w, const vtbl& vt, geometry::rect rect) {
  w = base{};
  w.vt = &vt;
  w.rect = rect;
  w.flags = flag::visible;
  w.refresh_hint = refresh::mode::fast;
}

/** @brief Appends `child` as the last child of `parent`. */
inline void add(base& parent, base& child) {
  child.parent = &parent;
  child.next_sibling = nullptr;
  base** pp = &parent.first_child;
  while (*pp != nullptr) pp = &(*pp)->next_sibling;
  *pp = &child;
}

/** @brief True when the focused flag is set. */
inline bool has_focus(const base& w) { return (w.flags & flag::focused) != 0; }

/** @brief Walks up to the tree root (the widget carrying the shell's notify callback). */
inline base& root_of(base& w) {
  base* cur = &w;
  while (cur->parent != nullptr) cur = cur->parent;
  return *cur;
}

/** @brief Requests a display update for `r`, routed to whatever shell attached this tree. */
inline void invalidate_rect(base& w, geometry::rect r, refresh::mode mode) {
  base& root = root_of(w);
  if (root.notify != nullptr) root.notify(root.notify_context, r, mode);
}

/** @brief Requests a display update for the widget's own rect, using its refresh hint. */
inline void invalidate(base& w) { invalidate_rect(w, w.rect, w.refresh_hint); }

/** @brief Shows or hides a widget, invalidating its rect on change. */
inline void set_visible(base& w, bool visible) {
  const bool was = (w.flags & flag::visible) != 0;
  if (was == visible) return;
  if (visible)
    w.flags |= flag::visible;
  else
    w.flags &= static_cast<std::uint8_t>(~flag::visible);
  invalidate_rect(w, w.rect, refresh::mode::quality);
}

/** @brief Renders a widget and its visible children, clipping each to the parent's clip. */
inline void render_tree(base& w, canvas::surface& c) {
  if ((w.flags & flag::visible) == 0) return;
  const geometry::rect clip = geometry::intersect(c.clip, w.rect);
  if (geometry::empty(clip)) return;
  const geometry::rect old = canvas::set_clip(c, clip);
  if (w.vt != nullptr && w.vt->render != nullptr) w.vt->render(w, c);
  for (base* ch = w.first_child; ch != nullptr; ch = ch->next_sibling) render_tree(*ch, c);
  canvas::set_clip(c, old);
}

namespace detail {
inline bool can_focus(const base& w) {
  return (w.flags & (flag::visible | flag::focusable | flag::disabled)) ==
         (flag::visible | flag::focusable);
}
}  // namespace detail

/** @brief Finds the topmost (last-added, on-top) focusable/hit widget containing (x, y). */
inline base* hit_test(base& root, int x, int y) {
  if ((root.flags & flag::visible) == 0 || !geometry::contains(root.rect, x, y)) return nullptr;
  base* found = nullptr;
  for (base* ch = root.first_child; ch != nullptr; ch = ch->next_sibling) {
    if (base* h = hit_test(*ch, x, y)) found = h;  // later siblings are on top.
  }
  if (found != nullptr) return found;
  return detail::can_focus(root) ? &root : nullptr;
}

/** @brief A widget tree plus its focused widget; pages and dialogs each own one. */
struct scope {
  base root{};
  base* focus = nullptr;
};

inline constexpr int max_focusable = 32;

namespace detail {
inline int collect_focusable(base& w, base** out, int n) {
  if ((w.flags & flag::visible) == 0) return n;
  if (can_focus(w) && n < max_focusable) out[n++] = &w;
  for (base* ch = w.first_child; ch != nullptr; ch = ch->next_sibling) {
    n = collect_focusable(*ch, out, n);
  }
  return n;
}
}  // namespace detail

/** @brief Initializes an empty focus scope over `rect`. */
inline void init(scope& s, geometry::rect rect) {
  init(s.root, container_vtbl, rect);
  s.focus = nullptr;
}

/** @brief Moves focus to `w` (or clears it for nullptr), firing on_focus callbacks. */
inline void set_focus(scope& s, base* w) {
  if (s.focus == w) return;
  base* old = s.focus;
  s.focus = w;
  if (old != nullptr) {
    old->flags &= static_cast<std::uint8_t>(~flag::focused);
    if (old->vt->on_focus != nullptr) old->vt->on_focus(*old, false);
    invalidate(*old);
  }
  if (w != nullptr) {
    w->flags |= flag::focused;
    if (w->vt->on_focus != nullptr) w->vt->on_focus(*w, true);
    invalidate(*w);
  }
}

/** @brief Focuses the first focusable widget in tree order, or clears focus if there is none. */
inline void focus_first(scope& s) {
  base* list[max_focusable];
  const int n = detail::collect_focusable(s.root, list, 0);
  set_focus(s, n != 0 ? list[0] : nullptr);
}

/** @brief Moves focus by `dir` (+1 next, -1 prev) among focusable widgets; false at an edge. */
inline bool move_focus(scope& s, int dir) {
  base* list[max_focusable];
  const int n = detail::collect_focusable(s.root, list, 0);
  if (n == 0) return false;
  int cur = -1;
  for (int i = 0; i < n; ++i) {
    if (list[i] == s.focus) cur = i;
  }
  const int next = (cur < 0) ? (dir < 0 ? n - 1 : 0) : cur + dir;
  if (next < 0 || next >= n) return false;
  if (list[next] == s.focus) return false;
  set_focus(s, list[next]);
  return true;
}

/** @brief Default event routing: focused widget, then arrow-key focus moves, then taps. */
inline bool handle_event(scope& s, const event::value& ev) {
  if (ev.kind == event::type::key) {
    base* f = s.focus;
    if (f != nullptr && f->vt->on_event != nullptr && f->vt->on_event(*f, ev)) return true;
    switch (ev.key) {
      case event::key_code::up:
      case event::key_code::left:
        return move_focus(s, -1);
      case event::key_code::down:
      case event::key_code::right:
        return move_focus(s, +1);
      default:
        return false;
    }
  }
  if (ev.kind == event::type::tap) {
    base* w = hit_test(s.root, ev.x, ev.y);
    if (w == nullptr) return false;
    set_focus(s, w);
    if (w->vt->on_event != nullptr) w->vt->on_event(*w, ev);
    return true;
  }
  return false;
}

// ------------------------------------------------------------------- label

/** @brief Static or wrapped text widget. */
struct label {
  base base_widget{};
  const char* text = nullptr;
  const text::font* font = nullptr;
  text::align align = text::align::left;
  std::uint8_t gray = canvas::gray::black;
  bool wrap = false;
};

namespace detail {
inline void label_render(base& w, canvas::surface& c) {
  label& l = container_of<label>(w);
  if (l.text == nullptr || l.font == nullptr) return;
  if (l.wrap)
    canvas::draw_text_wrapped(c, *l.font, w.rect, l.text, l.align, l.gray);
  else
    canvas::draw_text_in(c, *l.font, w.rect, l.text, l.align, l.gray);
}
inline constexpr vtbl label_vtbl{"label", label_render, nullptr, nullptr};
}  // namespace detail

/** @brief Initializes a label; defaults to single-line, black, no wrap. */
inline void init(label& l, geometry::rect r, const char* text, const text::font& font,
                 text::align align) {
  init(l.base_widget, detail::label_vtbl, r);
  l.base_widget.refresh_hint = refresh::mode::quality;
  l.text = text;
  l.font = &font;
  l.align = align;
  l.gray = canvas::gray::black;
  l.wrap = false;
}

/** @brief Replaces the label's text and invalidates its rect. */
inline void set_text(label& l, const char* text) {
  l.text = text;
  invalidate(l.base_widget);
}

// ------------------------------------------------------------------ button

/** @brief Draws a small icon glyph inside `r` in the given gray; used for icon-only buttons. */
using icon_fn = void (*)(canvas::surface& c, geometry::rect r, std::uint8_t gray);

/** @brief A focusable, activatable button with optional leading icon. */
struct button {
  base base_widget{};
  const char* text = nullptr;
  const text::font* font = nullptr;
  icon_fn icon = nullptr;
};

namespace detail {
inline void button_render(base& w, canvas::surface& c) {
  button& b = container_of<button>(w);
  const bool focused = has_focus(w);
  const bool disabled = (w.flags & flag::disabled) != 0;
  if (focused) {
    // Inverted = the clearest focus cue on a 2-level panel.
    canvas::fill(c, w.rect, canvas::gray::black);
  } else {
    canvas::fill(c, w.rect, canvas::gray::white);
    canvas::border(c, w.rect, 2, disabled ? canvas::gray::light : canvas::gray::black);
  }
  const std::uint8_t fg =
      focused ? canvas::gray::white : (disabled ? canvas::gray::light : canvas::gray::black);
  geometry::rect text_rect = geometry::inset(w.rect, 8);
  text::align align = text::align::center;
  if (b.icon != nullptr) {
    const geometry::rect icon_rect =
        geometry::make(text_rect.x, text_rect.y + (text_rect.h - 24) / 2, 24, 24);
    b.icon(c, icon_rect, fg);
    text_rect.x = icon_rect.x + icon_rect.w + 8;
    text_rect.w = w.rect.x + w.rect.w - text_rect.x - 8;
    align = text::align::left;
  }
  canvas::draw_text_in(c, *b.font, text_rect, b.text, align, fg);
}

inline bool button_event(base& w, const event::value& ev) {
  const bool activate =
      ev.kind == event::type::tap || (ev.kind == event::type::key && ev.key == event::key_code::ok);
  if (!activate) return false;
  if (w.on_activate != nullptr) w.on_activate(w, w.user);
  return true;
}

inline constexpr vtbl button_vtbl{"button", button_render, button_event, nullptr};
}  // namespace detail

/** @brief Initializes a focusable button; `fn`/`user` back `on_activate`. */
inline void init(button& b, geometry::rect r, const char* text, const text::font& font,
                 activate_fn fn, void* user) {
  init(b.base_widget, detail::button_vtbl, r);
  b.base_widget.flags |= flag::focusable;
  b.base_widget.on_activate = fn;
  b.base_widget.user = user;
  b.text = text;
  b.font = &font;
  b.icon = nullptr;
}

/** @brief Sets (or clears) the button's leading icon and invalidates it. */
inline void set_icon(button& b, icon_fn icon) {
  b.icon = icon;
  invalidate(b.base_widget);
}

// -------------------------------------------------------------------- list

/** @brief Row layout: two stacked lines (library) or primary-left/secondary-right (settings). */
enum class list_style { two_line, value };

/** @brief Supplies the primary/secondary text for row `index`. */
using row_fn = void (*)(struct list& self, int index, const char** primary, const char** secondary);
/** @brief Fired when a row is activated by tap or the OK key. */
using select_fn = void (*)(struct list& self, int index);

/** @brief A focusable, scrollable row list with a thumb-style scrollbar. */
struct list {
  base base_widget{};
  list_style style = list_style::two_line;
  int count = 0;
  int selected = 0;
  int top = 0;
  std::int16_t row_h = 0;
  const text::font* font = nullptr;
  const text::font* font_secondary = nullptr;
  row_fn get_row = nullptr;
  select_fn on_select = nullptr;
  void* user = nullptr;
};

inline constexpr int list_pad = 14;
inline constexpr int list_scrollbar_w = 6;

/** @brief Number of fully visible rows given the list's current height. */
inline int visible_rows(const list& l) {
  const int n = l.base_widget.rect.h / l.row_h;
  return n < 1 ? 1 : n;
}

namespace detail {
inline geometry::rect list_row_rect(const list& l, int i) {
  const geometry::rect r = l.base_widget.rect;
  const int w = r.w - ((l.count > visible_rows(l)) ? list_scrollbar_w + 4 : 0);
  return geometry::make(r.x, r.y + (i - l.top) * l.row_h, w, l.row_h);
}

inline void list_render(base& w, canvas::surface& c) {
  list& l = container_of<list>(w);
  const int vis = visible_rows(l);
  const bool focused = has_focus(w);

  for (int i = l.top; i < l.count && i < l.top + vis; ++i) {
    const geometry::rect rr = list_row_rect(l, i);
    if (!geometry::intersects(rr, c.clip)) continue;

    const char* primary = "";
    const char* secondary = nullptr;
    l.get_row(l, i, &primary, &secondary);
    const bool sel = (i == l.selected);
    const bool inverted = sel && focused;
    const std::uint8_t fg = inverted ? canvas::gray::white : canvas::gray::black;
    const std::uint8_t fg2 = inverted ? canvas::gray::white : canvas::gray::dark;

    canvas::fill(c, rr, inverted ? canvas::gray::black : canvas::gray::white);
    if (sel && !focused) {
      canvas::fill(c, geometry::make(rr.x, rr.y + 6, 5, rr.h - 12), canvas::gray::black);
    }
    if (!inverted) canvas::hline(c, rr.x, rr.y + rr.h - 1, rr.w, canvas::gray::light);

    const geometry::rect inner = geometry::make(rr.x + list_pad, rr.y, rr.w - 2 * list_pad, rr.h);
    if (l.style == list_style::two_line && secondary != nullptr) {
      const int h1 = l.font->line_height;
      const int h2 = l.font_secondary->line_height;
      const int y = rr.y + (rr.h - h1 - h2) / 2;
      canvas::draw_text_in(c, *l.font, geometry::make(inner.x, y, inner.w, h1), primary,
                           text::align::left, fg);
      canvas::draw_text_in(c, *l.font_secondary, geometry::make(inner.x, y + h1, inner.w, h2),
                           secondary, text::align::left, fg2);
    } else {
      const int sw =
          secondary != nullptr ? text::width(*l.font_secondary, secondary, -1) + list_pad : 0;
      canvas::draw_text_in(c, *l.font, geometry::make(inner.x, inner.y, inner.w - sw, inner.h),
                           primary, text::align::left, fg);
      if (secondary != nullptr) {
        canvas::draw_text_in(c, *l.font_secondary, inner, secondary, text::align::right, fg2);
      }
    }
  }

  if (l.count > vis) {  // scrollbar: track + thumb.
    const geometry::rect r = w.rect;
    const geometry::rect track =
        geometry::make(r.x + r.w - list_scrollbar_w, r.y, list_scrollbar_w, vis * l.row_h);
    canvas::fill(c, track, canvas::gray::white);
    canvas::vline(c, track.x + list_scrollbar_w / 2, track.y, track.h, canvas::gray::light);
    const int th = std::max(track.h * vis / l.count, 16);
    const int ty = track.y + (track.h - th) * l.top / std::max(l.count - vis, 1);
    canvas::fill(c, geometry::make(track.x, ty, list_scrollbar_w, th), canvas::gray::black);
  }
}
}  // namespace detail

/** @brief Invalidates one row's rect if it's currently visible. */
inline void invalidate_row(list& l, int index, refresh::mode mode) {
  if (index < l.top || index >= l.top + visible_rows(l)) return;
  invalidate_rect(l.base_widget, detail::list_row_rect(l, index), mode);
}

/** @brief Moves the selection, scrolling a full page (paginated, not smooth) when needed. */
inline void select(list& l, int index) {
  if (l.count == 0) {
    l.selected = 0;
    return;
  }
  if (index < 0) index = 0;
  if (index >= l.count) index = l.count - 1;
  if (index == l.selected) return;

  const int vis = visible_rows(l);
  const int old = l.selected;
  l.selected = index;
  if (index < l.top || index >= l.top + vis) {
    l.top = (index / vis) * vis;
    invalidate_rect(l.base_widget, l.base_widget.rect, refresh::mode::quality);
  } else {
    // Both rows must clear/repaint cleanly; DU can leave the previous inverted
    // row visible when the rotary is turned rapidly on the e-ink panel.
    invalidate_row(l, old, refresh::mode::quality);
    invalidate_row(l, index, refresh::mode::quality);
  }
}

/** @brief Resets the row count, clamping selection/scroll and invalidating the whole list. */
inline void set_count(list& l, int count) {
  l.count = count;
  if (l.selected >= count) l.selected = count > 0 ? count - 1 : 0;
  const int vis = visible_rows(l);
  l.top = (l.selected / vis) * vis;
  invalidate_rect(l.base_widget, l.base_widget.rect, refresh::mode::quality);
}

namespace detail {
inline bool list_event(base& w, const event::value& ev) {
  list& l = container_of<list>(w);
  const int vis = visible_rows(l);
  if (ev.kind == event::type::tap) {
    const int i = l.top + (ev.y - w.rect.y) / l.row_h;
    if (i >= l.count) return true;
    select(l, i);
    if (l.on_select != nullptr) l.on_select(l, i);
    return true;
  }
  switch (ev.key) {
    case event::key_code::up:
      if (l.selected == 0) return false;  // let focus leave the list.
      select(l, l.selected - 1);
      return true;
    case event::key_code::down:
      if (l.selected >= l.count - 1) return false;
      select(l, l.selected + 1);
      return true;
    case event::key_code::next:
      select(l, std::min(l.top + vis, l.count - 1));
      return true;
    case event::key_code::prev:
      select(l, std::max(l.top - vis, 0));
      return true;
    case event::key_code::ok:
      if (l.count != 0 && l.on_select != nullptr) l.on_select(l, l.selected);
      return true;
    default:
      return false;
  }
}
inline constexpr vtbl list_vtbl{"list", list_render, list_event, nullptr};
}  // namespace detail

/** @brief Initializes an empty, focusable list. */
inline void init(list& l, geometry::rect r, list_style style, std::int16_t row_h,
                 const text::font& font, const text::font& font_secondary, row_fn get_row,
                 void* user) {
  init(l.base_widget, detail::list_vtbl, r);
  l.base_widget.flags |= flag::focusable;
  l.base_widget.refresh_hint = refresh::mode::fast;
  l.style = style;
  l.count = l.selected = l.top = 0;
  l.row_h = row_h;
  l.font = &font;
  l.font_secondary = &font_secondary;
  l.get_row = get_row;
  l.on_select = nullptr;
  l.user = user;
}

}  // namespace widget
