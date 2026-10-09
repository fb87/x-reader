#pragma once

#include <cstdint>

#include "canvas.hpp"
#include "event.hpp"
#include "geometry.hpp"
#include "refresh.hpp"
#include "widget.hpp"

/**
 * @brief A full screen of UI (Home, Library, Reader, ...), ported from
 * `xr_page_t` (include/xr/xr_page.h, src/xr_page.c).
 *
 * Pages live on the shell's navigation stack. Each declares which shell
 * chrome it wants (status bar / dock) and gets `area`, the rect left
 * over for its content. Lifecycle: push -> on_create/on_enter (covered
 * page gets on_exit); pop -> on_exit/on_destroy (revealed page gets
 * on_enter); replace -> old on_exit/on_destroy, new on_create/on_enter;
 * chrome/area change -> on_layout.
 *
 * `context::shell` is a pointer to an as-yet-incomplete `shell::context`
 * (forward-declared below, never dereferenced in this file) -- exactly
 * the same role as the old C code's forward-declared `struct xr_shell
 * *shell`. Functions that actually need to read shell state (whether
 * this page is on top, the shell's theme/dock/status rects) are defined
 * in core/shell.hpp, which reopens `namespace page` once `shell::context`
 * is a complete type. This mirrors how the old .c file saw the full
 * xr_shell.h while the .h file only forward-declared it; a header-only
 * tree can't split that way, so shell.hpp takes over that role.
 */
namespace shell {
struct context;
}  // namespace shell

namespace page {

/** @brief Chrome a page requests from the shell. */
namespace chrome {
inline constexpr std::uint8_t none = 0;
inline constexpr std::uint8_t status = 1u << 0;
inline constexpr std::uint8_t dock = 1u << 1;
inline constexpr std::uint8_t all = status | dock;
}  // namespace chrome

struct context;

/** @brief A dock button; `key` is an optional hardware shortcut. */
struct action {
  std::uint16_t id = 0;
  const char* label = nullptr;
  event::key_code key = event::key_code::none;
  widget::icon_fn icon = nullptr;
};

/** @brief Function-pointer vtable for one page type. */
struct vtbl {
  void (*on_create)(context& self) = nullptr;  ///< build widgets inside self.area.
  void (*on_layout)(context& self) = nullptr;  ///< self.area changed (chrome toggle).
  void (*on_enter)(context& self) = nullptr;
  void (*on_exit)(context& self) = nullptr;
  void (*on_destroy)(context& self) = nullptr;
  /** nullptr = render the widget tree. Custom pages can draw directly. */
  void (*render)(context& self, canvas::surface& c) = nullptr;
  /** Sees events before the default focus routing. Return true if used. */
  bool (*on_event)(context& self, const event::value& ev) = nullptr;
  void (*on_action)(context& self, std::uint16_t action_id) = nullptr;
  void (*on_tick)(context& self, std::uint32_t now_ms) = nullptr;
};

/** @brief One page instance's state; subclass by embedding this as the first member. */
struct context {
  const vtbl* vt = nullptr;
  shell::context* shell = nullptr;
  const char* title = nullptr;  ///< shown in the status bar.
  std::uint8_t chrome = chrome::none;
  refresh::mode enter_refresh = refresh::mode::full;  ///< how the screen refreshes on navigation.
  const action* actions = nullptr;
  std::uint8_t action_count = 0;
  geometry::rect area{};  ///< content rect, set by the shell.
  widget::scope scope{};  ///< widget tree + focus.
};

/** @brief Initializes a page: full-refresh on navigation (flash once, no ghosts), empty scope. */
inline void init(context& p, const vtbl& vt, const char* title, std::uint8_t chrome_flags) {
  p = context{};
  p.vt = &vt;
  p.title = title;
  p.chrome = chrome_flags;
  p.enter_refresh = refresh::mode::full;
  widget::init(p.scope, geometry::make(0, 0, 0, 0));
}

/** @brief Adds a widget to the page's content tree. */
inline void add(context& p, widget::base& w) { widget::add(p.scope.root, w); }

}  // namespace page
