#pragma once

#include "dlg_book_info.hpp"
#include "page_file_manager.hpp"
#include "page_home.hpp"
#include "page_library.hpp"
#include "page_reader.hpp"
#include "page_settings.hpp"
#include "page_sleep.hpp"
#include "page_splash.hpp"
#include "pages_fwd.hpp"

/**
 * @brief Composition root: every page/dialog instance (`struct pages`), the handful of
 * cross-page functions that need it complete, and app-level init/scan/pump, ported from
 * app/app.c and app/app_internal.h.
 *
 * Each page/dialog's own struct, lifecycle callbacks, and vtbl live in its own header
 * (app/page_*.hpp, app/dlg_book_info.hpp), declared against `app/pages_fwd.hpp`'s forward
 * declarations rather than this file -- none of them touch `nav.<specific_page>` fields
 * directly, only call another page's factory through a pointer. The eight `page_*`
 * factories below are the exception: they set `nav.<page>.app`/`.nav` directly, so they
 * (plus show_book_info/show_delete_confirm/show_about_confirm, which reach into
 * `nav.book_info`/`.delete_confirm`/`.about_confirm`) need `pages` complete, and so stay
 * here rather than in their page's own file.
 */
namespace app {

inline void about_confirm_result(dialog::context&, dialog::result, void*) {}

/** @brief Opens book `index` for reading, loading its EPUB if needed, then pushes the reader. */
inline void open_book(context& app, pages& nav, int index) {
  if (index < 0 || index >= app.library.count) return;
  state::set(*app.memory, key::book_current, static_cast<std::int64_t>(index));
  book::item& b = app.library.books[index];
  if (b.epub_source) {
    auto* storage_device =
        capability::get<storage::device>(*app.capabilities, capability::id::storage);
    epub::file_view view{storage_device, b.path.data(), 0};
    if (!storage::file_size(*storage_device, view.path, view.size)) return;
    if (!reader::open(app.session, view, b.title.data())) return;
  }
  shell::push(app.shell, *page_reader(app, nav));
}

/** @brief Every page/dialog instance the app owns, one each, reused across navigations. */
struct pages {
  splash_page splash{};
  home_page home{};
  library_page library{}, favorites{};
  file_manager_page file_manager{};
  settings_page settings{};
  sleep_page sleep{};
  reader_page reader{};
  book_info_dialog book_info{};
  dialog::confirm delete_confirm{}, about_confirm{};
};

inline void show_delete_confirm(context& app, pages& nav, library_page& requester) {
  dialog::show_confirm(app.shell, nav.delete_confirm, "Delete book", requester.confirm_msg,
                       "Delete", "Cancel", delete_confirm_result, &requester);
  dialog::set_icon(nav.delete_confirm.base, delete_icon);
}

inline void show_about_confirm(context& app, pages& nav) {
  // app_confirm always hardcoded "Cancel" as the no-button label, even for this
  // non-destructive notice -- preserved exactly, odd as it looks.
  dialog::show_confirm(app.shell, nav.about_confirm, "About X-Reader",
                       "Author: X-Reader Team\nSoftware: X-Reader 0.1\nRelease: 2026-10-04",
                       "Close", "Cancel", about_confirm_result, nullptr);
}

inline page::context* page_splash(context& app, pages& nav) {
  nav.splash.app = &app;
  nav.splash.nav = &nav;
  page::init(nav.splash.base, splash_vtbl, "X-Reader", page::chrome::none);
  return &nav.splash.base;
}

inline page::context* page_home(context& app, pages& nav) {
  nav.home.app = &app;
  nav.home.nav = &nav;
  page::init(nav.home.base, home_vtbl, "Home", page::chrome::status);
  return &nav.home.base;
}

inline page::context* page_library(context& app, pages& nav) {
  nav.library.app = &app;
  nav.library.nav = &nav;
  nav.library.favorites_only = false;
  page::init(nav.library.base, library_vtbl, "Library", page::chrome::all);
  page::set_actions(nav.library.base, library_actions,
                    sizeof(library_actions) / sizeof(library_actions[0]));
  return &nav.library.base;
}

inline page::context* page_favorites(context& app, pages& nav) {
  nav.favorites.app = &app;
  nav.favorites.nav = &nav;
  nav.favorites.favorites_only = true;
  page::init(nav.favorites.base, library_vtbl, "Favorites", page::chrome::all);
  page::set_actions(nav.favorites.base, favorites_actions,
                    sizeof(favorites_actions) / sizeof(favorites_actions[0]));
  return &nav.favorites.base;
}

inline page::context* page_file_manager(context& app, pages& nav) {
  nav.file_manager.app = &app;
  nav.file_manager.nav = &nav;
  page::init(nav.file_manager.base, file_manager_vtbl, "File Manager", page::chrome::all);
  page::set_actions(nav.file_manager.base, file_manager_actions,
                    sizeof(file_manager_actions) / sizeof(file_manager_actions[0]));
  return &nav.file_manager.base;
}

inline page::context* page_settings(context& app, pages& nav) {
  nav.settings.app = &app;
  nav.settings.nav = &nav;
  page::init(nav.settings.base, settings_vtbl, "Settings", page::chrome::all);
  page::set_actions(nav.settings.base, settings_actions,
                    sizeof(settings_actions) / sizeof(settings_actions[0]));
  return &nav.settings.base;
}

inline page::context* page_sleep(context& app, pages& nav) {
  nav.sleep.app = &app;
  nav.sleep.nav = &nav;
  page::init(nav.sleep.base, sleep_vtbl, "Sleep", page::chrome::none);
  return &nav.sleep.base;
}

inline page::context* page_reader(context& app, pages& nav) {
  nav.reader.app = &app;
  nav.reader.nav = &nav;
  page::init(nav.reader.base, reader_vtbl, "Reading", page::chrome::none);
  page::set_actions(nav.reader.base, reader_actions,
                    sizeof(reader_actions) / sizeof(reader_actions[0]));
  return &nav.reader.base;
}

inline void show_book_info(context& app, pages& nav, int index) {
  if (index < 0 || index >= app.library.count) return;
  dialog::init(nav.book_info.base, info_vtbl, "Book info");
  dialog::set_icon(nav.book_info.base, book_icon);
  nav.book_info.app = &app;
  nav.book_info.nav = &nav;
  nav.book_info.book = index;
  nav.book_info.base.on_result = info_result;
  shell::show_dialog(app.shell, nav.book_info.base);
}

/**
 * @brief Wires an app context to a board's capabilities and starts the splash page,
 * ported from `app_start`/`app_t` defaults (app/app.c). `storage_root` is the path
 * handed to both the initial library scan and the file manager's starting directory.
 */
inline bool init(context& self, pages& nav, capability::registry& capabilities,
                 state::store& memory, state::store& persistent, const shell::theme& theme,
                 const char* storage_root) {
  self.capabilities = &capabilities;
  self.memory = &memory;
  self.persistent = &persistent;
  std::snprintf(self.storage_root, sizeof(self.storage_root), "%s", storage_root);

  auto* display = capability::get<display::device>(capabilities, capability::id::display);
  auto* input = capability::get<input::device>(capabilities, capability::id::input);
  auto* platform = capability::get<platform::device>(capabilities, capability::id::platform);
  auto* storage_device = capability::get<storage::device>(capabilities, capability::id::storage);
  if (display == nullptr || input == nullptr || platform == nullptr || storage_device == nullptr) {
    return false;
  }
  self.input = input;
  shell::init(self.shell, *display, *platform, theme);

  restore_persistent(self);
  if (state::find(memory, key::font_size) == nullptr)
    state::set(memory, key::font_size, std::int64_t{1});
  if (state::find(memory, key::full_refresh_every) == nullptr) {
    state::set(memory, key::full_refresh_every, std::int64_t{6});
  }
  if (state::find(memory, key::show_progress) == nullptr)
    state::set(memory, key::show_progress, true);
  if (state::find(memory, key::sleep_timeout_minutes) == nullptr) {
    state::set(memory, key::sleep_timeout_minutes, std::int64_t{10});
  }
  if (state::find(memory, key::book_current) == nullptr) {
    state::set(memory, key::book_current, std::int64_t{-1});
  }
  shell::set_full_refresh_every(self.shell, static_cast<std::uint16_t>(state::get(
                                                memory, key::full_refresh_every, std::int64_t{6})));

  shell::push(self.shell, *page_splash(self, nav));
  return true;
}

/**
 * @brief Scans the storage root into the library. Deliberately NOT part of init():
 * the old app_main.cpp showed the splash/boot screen before walking the SD card
 * (`xr_shell_tick/flush` the splash, then `mount_and_scan_sd()`), so a real card
 * with many files doesn't leave the user staring at a blank screen. Callers should
 * call this only after an initial tick+flush of the pushed splash page -- see
 * app_main.cpp.
 */
inline void scan_library(context& self) {
  auto* storage_device =
      capability::get<storage::device>(*self.capabilities, capability::id::storage);
  if (storage_device != nullptr) library::scan(self.library, *storage_device, self.storage_root);
}

/** @brief Polls queued input, dispatches it, ticks the top page, and flushes any redraw. */
inline void pump(context& self) {
  event::value ev{};
  while (input::poll(*self.input, ev)) shell::dispatch(self.shell, ev);
  shell::tick(self.shell);
  shell::flush(self.shell);
}

}  // namespace app
