#pragma once

#include "../core/dialog.hpp"
#include "../core/page.hpp"
#include "../core/shell.hpp"
#include "../core/widget.hpp"
#include "../reader/library.hpp"
#include "../reader/session.hpp"
#include "assets.hpp"
#include "context.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <strings.h>

/**
 * @brief All application pages/dialogs, ported from app/page_*.c and
 * app/dlg_book_info.c. Combined into one header -- deviating from the
 * one-file-per-page aspiration in docs/DESIGN.md's migration mapping --
 * because every page can navigate to every other page (Home -> Library,
 * Library -> Home via Back, Settings -> Home, ...), and header-only
 * components can't form that mutual reference across separate files
 * the way old C translation units could (a .c file can call a function
 * only forward-declared in a shared .h; an inline function's body needs
 * the callee's declaration in the SAME translation unit, and the body
 * of one page's navigation callback needs another page's complete
 * factory-function definition to call it). The architecture reference
 * this migration follows the shape of made the identical choice for
 * the identical reason.
 *
 * Simplification flagged, not silently dropped: the old app's File
 * Manager could open a book beyond the normal 16-book library cap via
 * a dedicated 17th "transient" slot (app_open_storage_epub in the old
 * app/app.c). That capacity-management edge case is not replicated
 * here -- File Manager opens go through the same reader::library::add
 * used by scanning, capped at the same 16 books.
 */
namespace app {

struct pages;

inline page::context* page_splash(context& app, pages& nav);
inline page::context* page_home(context& app, pages& nav);
inline page::context* page_library(context& app, pages& nav);
inline page::context* page_favorites(context& app, pages& nav);
inline page::context* page_file_manager(context& app, pages& nav);
inline page::context* page_settings(context& app, pages& nav);
inline page::context* page_sleep(context& app, pages& nav);
inline page::context* page_reader(context& app, pages& nav);
inline void open_book(context& app, pages& nav, int index);
inline void show_book_info(context& app, pages& nav, int index);
struct library_page;
inline void show_delete_confirm(context& app, pages& nav, library_page& requester);
inline void show_about_confirm(context& app, pages& nav);

// --------------------------------------------------------------------- splash

struct splash_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
  std::uint32_t entered_ms = 0;
  widget::label name{}, tagline{}, status{};
};

inline splash_page& splash_of(page::context& p) {
  return *reinterpret_cast<splash_page*>(reinterpret_cast<char*>(&p) - offsetof(splash_page, base));
}

inline void splash_create(page::context& p) {
  splash_page& sp = splash_of(p);
  const shell::theme& t = theme_of(p);
  const geometry::rect a = p.area;
  const int cy = a.y + a.h / 2;
  widget::init(sp.name, geometry::make(a.x, cy + 20, a.w, 40), "X-Reader", *t.font_title,
               text::align::center);
  widget::init(sp.tagline, geometry::make(a.x, cy + 62, a.w, 28), "the ebook reader for e-ink",
               *t.font_normal, text::align::center);
  sp.tagline.gray = canvas::gray::dark;
  widget::init(sp.status, geometry::make(a.x, a.y + a.h - 70, a.w, 24), "Loading library...",
               *t.font_small, text::align::center);
  sp.status.gray = canvas::gray::dark;
  page::add(p, sp.name.base_widget);
  page::add(p, sp.tagline.base_widget);
  page::add(p, sp.status.base_widget);
}

inline void splash_render(page::context& p, canvas::surface& c) {
  const geometry::rect a = p.area;
  const int cx = a.x + a.w / 2;
  const int top = a.y + a.h / 2 - 110;
  const int pw = 64, ph = 96;
  canvas::fill(c, geometry::make(cx - pw - 4, top, pw, ph), canvas::gray::black);
  canvas::fill(c, geometry::make(cx + 4, top, pw, ph), canvas::gray::black);
  for (int i = 0; i < 5; ++i) {
    const int y = top + 18 + i * 14;
    canvas::fill(c, geometry::make(cx - pw + 8, y, pw - 24, 4), canvas::gray::white);
    canvas::fill(c, geometry::make(cx + 16, y, pw - 24, 4), canvas::gray::white);
  }
  widget::render_tree(p.scope.root, c);
}

inline void splash_enter(page::context& p) { splash_of(p).entered_ms = shell::now(*p.shell); }

inline void splash_tick(page::context& p, std::uint32_t now) {
  splash_page& sp = splash_of(p);
  if (now - sp.entered_ms >= 1500) shell::replace(*p.shell, *page_home(*sp.app, *sp.nav));
}

inline constexpr page::vtbl splash_vtbl{
    splash_create, nullptr, splash_enter, nullptr, nullptr, splash_render, nullptr, nullptr,
    splash_tick,
};

// ----------------------------------------------------------------------- home

struct book_card {
  widget::base base_widget{};
  context* app = nullptr;
  pages* nav = nullptr;
};

inline book_card& card_of(widget::base& w) {
  return *reinterpret_cast<book_card*>(reinterpret_cast<char*>(&w) - offsetof(book_card, base_widget));
}

inline void card_cover(canvas::surface& c, geometry::rect r, const book::item& b) {
  const char initial[2] = {b.title[0], '\0'};
  canvas::fill(c, r, canvas::gray::light);
  canvas::border(c, r, 2, canvas::gray::black);
  canvas::fill(c, geometry::make(r.x + 5, r.y + 5, r.w - 10, 16), canvas::gray::black);
  canvas::draw_text_in(c, xr_font_alegreya_bold_26, geometry::make(r.x + 4, r.y + 24, r.w - 8, r.h - 28),
                       initial, text::align::center, canvas::gray::black);
}

inline void card_render(widget::base& w, canvas::surface& c) {
  book_card& card = card_of(w);
  context& app = *card.app;
  const shell::theme& t = *app.shell.theme_ptr;
  const geometry::rect r = w.rect;
  canvas::fill(c, r, canvas::gray::white);
  canvas::border(c, r, widget::has_focus(w) ? 5 : 2, canvas::gray::black);

  const geometry::rect in = geometry::inset(r, t.pad + 4);
  const int y = in.y;
  canvas::draw_text_in(c, *t.font_small, geometry::make(in.x, y, in.w, t.font_small->line_height),
                       "CONTINUE READING", text::align::left, canvas::gray::dark);

  const std::int64_t current = state::get(*app.memory, key::book_current, std::int64_t{-1});
  if (current < 0 || current >= app.library.count) {
    canvas::draw_text_in(c, *t.font_normal,
                         geometry::make(in.x, y + t.font_small->line_height + 6, in.w, 40),
                         "No book open", text::align::left, canvas::gray::black);
    return;
  }
  const book::item& b = app.library.books[current];
  const geometry::rect cover = geometry::make(in.x, y + t.font_small->line_height + 6, 82, 112);
  const geometry::rect details =
      geometry::make(cover.x + cover.w + 14, cover.y, in.x + in.w - (cover.x + cover.w + 14), cover.h);
  card_cover(c, cover, b);
  canvas::draw_text_in(c, *t.font_title, geometry::make(details.x, details.y, details.w, 42),
                       b.title.data(), text::align::left, canvas::gray::black);
  canvas::draw_text_in(
      c, *t.font_normal,
      geometry::make(details.x, details.y + 46, details.w, t.font_normal->line_height), b.author,
      text::align::left, canvas::gray::dark);

  char pct[8];
  std::snprintf(pct, sizeof(pct), "%d%%", b.progress);
  const int pw = text::width(*t.font_bold, pct, -1);
  const int by = details.y + details.h - 18;
  draw_progress(c, geometry::make(details.x, by, details.w - pw - t.pad, 18), b.progress);
  canvas::draw_text_in(c, *t.font_bold, geometry::make(details.x + details.w - pw, by - 6, pw, 30),
                       pct, text::align::right, canvas::gray::black);
}

inline bool card_event(widget::base& w, const event::value& ev) {
  if (ev.kind == event::type::tap || (ev.kind == event::type::key && ev.key == event::key_code::ok)) {
    book_card& card = card_of(w);
    const std::int64_t current = state::get(*card.app->memory, key::book_current, std::int64_t{-1});
    open_book(*card.app, *card.nav, static_cast<int>(current));
    return true;
  }
  return false;
}

inline constexpr widget::vtbl card_vtbl{"book_card", card_render, card_event, nullptr};

inline void library_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::book, g);
}
inline void favorite_icon_home(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::book, g);
}
inline void settings_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::settings, g);
}
inline void file_manager_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::folder, g);
}
inline void sleep_icon_home(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::close, g);
}

struct home_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
  book_card card{};
  widget::button library{}, favorites{}, file_manager{}, settings{}, sleep{};
  widget::label stats{};
  char stats_buf[48] = {0};
};

inline home_page& home_of(page::context& p) {
  return *reinterpret_cast<home_page*>(reinterpret_cast<char*>(&p) - offsetof(home_page, base));
}

inline void update_stats(home_page& hp) {
  int reading = 0, books = 0;
  for (int i = 0; i < hp.app->library.count; ++i) {
    if (!hp.app->library.books[i].transient) {
      ++books;
      const int progress = hp.app->library.books[i].progress;
      if (progress > 0 && progress < 100) ++reading;
    }
  }
  std::snprintf(hp.stats_buf, sizeof(hp.stats_buf), "%d books  |  %d in progress", books, reading);
}

inline void home_go_library(widget::base&, void* user) {
  auto& hp = *static_cast<home_page*>(user);
  shell::push(hp.app->shell, *page_library(*hp.app, *hp.nav));
}
inline void home_go_favorites(widget::base&, void* user) {
  auto& hp = *static_cast<home_page*>(user);
  shell::push(hp.app->shell, *page_favorites(*hp.app, *hp.nav));
}
inline void home_go_file_manager(widget::base&, void* user) {
  auto& hp = *static_cast<home_page*>(user);
  shell::push(hp.app->shell, *page_file_manager(*hp.app, *hp.nav));
}
inline void home_go_settings(widget::base&, void* user) {
  auto& hp = *static_cast<home_page*>(user);
  shell::push(hp.app->shell, *page_settings(*hp.app, *hp.nav));
}
inline void home_go_sleep(widget::base&, void* user) {
  auto& hp = *static_cast<home_page*>(user);
  shell::replace(hp.app->shell, *page_sleep(*hp.app, *hp.nav));
}

inline void home_create(page::context& p) {
  home_page& hp = home_of(p);
  const shell::theme& t = theme_of(p);
  const geometry::rect a = geometry::inset(p.area, t.pad);
  int y = a.y + 8;

  widget::init(hp.card.base_widget, card_vtbl, geometry::make(a.x, y, a.w, 190));
  hp.card.app = hp.app;
  hp.card.nav = hp.nav;
  hp.card.base_widget.flags |= widget::flag::focusable;
  y += 190 + t.pad * 2;

  widget::init(hp.library, geometry::make(a.x, y, a.w, t.row_h - 8), "Library", *t.font_bold,
               home_go_library, &hp);
  widget::set_icon(hp.library, library_icon);
  y += t.row_h;
  widget::init(hp.favorites, geometry::make(a.x, y, a.w, t.row_h - 8), "Favorites", *t.font_bold,
               home_go_favorites, &hp);
  widget::set_icon(hp.favorites, favorite_icon_home);
  y += t.row_h;
  widget::init(hp.file_manager, geometry::make(a.x, y, a.w, t.row_h - 8), "File Manager",
               *t.font_bold, home_go_file_manager, &hp);
  widget::set_icon(hp.file_manager, file_manager_icon);
  y += t.row_h;
  widget::init(hp.settings, geometry::make(a.x, y, a.w, t.row_h - 8), "Settings", *t.font_bold,
               home_go_settings, &hp);
  widget::set_icon(hp.settings, settings_icon);
  widget::init(hp.sleep, geometry::make(a.x + a.w - 112, a.y + a.h - 42, 112, 34), "Sleep",
               *t.font_small, home_go_sleep, &hp);
  widget::set_icon(hp.sleep, sleep_icon_home);

  update_stats(hp);
  widget::init(hp.stats, geometry::make(a.x, a.y + a.h - 30, a.w, 30), hp.stats_buf, *t.font_small,
               text::align::center);
  hp.stats.gray = canvas::gray::dark;

  page::add(p, hp.card.base_widget);
  page::add(p, hp.library.base_widget);
  page::add(p, hp.favorites.base_widget);
  page::add(p, hp.file_manager.base_widget);
  page::add(p, hp.settings.base_widget);
  page::add(p, hp.sleep.base_widget);
  page::add(p, hp.stats.base_widget);
}

inline void home_enter(page::context& p) {
  home_page& hp = home_of(p);
  update_stats(hp);
  widget::invalidate(hp.card.base_widget);
  widget::invalidate(hp.stats.base_widget);
}

inline void home_tick(page::context& p, std::uint32_t now) {
  home_page& hp = home_of(p);
  const std::int64_t timeout = state::get(*hp.app->memory, key::sleep_timeout_minutes, std::int64_t{10});
  if (timeout > 0 && now - p.shell->last_input_ms >= static_cast<std::uint32_t>(timeout) * 60000u) {
    shell::replace(*p.shell, *page_sleep(*hp.app, *hp.nav));
  }
}

inline constexpr page::vtbl home_vtbl{
    home_create, nullptr, home_enter, nullptr, nullptr, nullptr, nullptr, nullptr, home_tick,
};

// ------------------------------------------------------------ library / favorites

enum class library_action : std::uint16_t { favorite = 1, del, back };

inline void favorite_icon_lib(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::book, g);
}
inline void delete_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::del, g);
}
inline void back_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::arrow_back, g);
}

inline constexpr page::action library_actions[] = {
    {static_cast<std::uint16_t>(library_action::favorite), "Favorite", event::key_code::none,
     favorite_icon_lib},
    {static_cast<std::uint16_t>(library_action::del), "Delete", event::key_code::none, delete_icon},
    {static_cast<std::uint16_t>(library_action::back), "Back", event::key_code::none, back_icon},
};
inline constexpr page::action favorites_actions[] = {
    {static_cast<std::uint16_t>(library_action::favorite), "Remove", event::key_code::none,
     favorite_icon_lib},
    {static_cast<std::uint16_t>(library_action::del), "Delete", event::key_code::none, delete_icon},
    {static_cast<std::uint16_t>(library_action::back), "Back", event::key_code::none, back_icon},
};

struct library_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
  widget::list list{};
  char title[32] = {0};
  char secondary[64] = {0};
  char confirm_msg[96] = {0};
  int book_index[library::max_books] = {0};
  int pending_book = -1;
  bool favorites_only = false;
};

inline library_page& library_of(page::context& p) {
  return *reinterpret_cast<library_page*>(reinterpret_cast<char*>(&p) - offsetof(library_page, base));
}

inline void library_row(widget::list& l, int i, const char** primary, const char** secondary) {
  library_page& lp = *static_cast<library_page*>(l.user);
  const book::item& b = lp.app->library.books[lp.book_index[i]];
  *primary = b.title.data();
  if (b.progress == 0) std::snprintf(lp.secondary, sizeof(lp.secondary), "%s  |  New", b.author);
  else if (b.progress >= 100) std::snprintf(lp.secondary, sizeof(lp.secondary), "%s  |  Finished", b.author);
  else std::snprintf(lp.secondary, sizeof(lp.secondary), "%s  |  %d%%", b.author, b.progress);
  *secondary = lp.secondary;
}

inline void draw_cover(canvas::surface& c, geometry::rect r, const book::item& book, int index) {
  const char initial[2] = {book.title[0], '\0'};
  const std::uint8_t band = (index % 3 == 0) ? canvas::gray::black : canvas::gray::dark;
  canvas::fill(c, r, canvas::gray::light);
  canvas::border(c, r, 2, canvas::gray::black);
  canvas::fill(c, geometry::make(r.x + 3, r.y + 3, r.w - 6, 11), band);
  canvas::draw_text_in(c, xr_font_alegreya_bold_26, geometry::make(r.x + 2, r.y + 16, r.w - 4, r.h - 18),
                       initial, text::align::center, canvas::gray::black);
}

inline void library_render(page::context& p, canvas::surface& c) {
  library_page& lp = library_of(p);
  widget::list& list = lp.list;
  const int visible = widget::visible_rows(list);
  const bool focused = widget::has_focus(list.base_widget);
  const int row_w = list.base_widget.rect.w - ((list.count > visible) ? 10 : 0);

  for (int i = list.top; i < list.count && i < list.top + visible; ++i) {
    const geometry::rect row = geometry::make(
        list.base_widget.rect.x, list.base_widget.rect.y + (i - list.top) * list.row_h, row_w,
        list.row_h);
    if (!geometry::intersects(row, c.clip)) continue;

    const bool selected = i == list.selected;
    const bool inverted = selected && focused;
    const std::uint8_t fg = inverted ? canvas::gray::white : canvas::gray::black;
    const std::uint8_t secondary_fg = inverted ? canvas::gray::white : canvas::gray::dark;
    char number[12];
    const char* title = nullptr;
    const char* author = nullptr;
    library_row(list, i, &title, &author);
    std::snprintf(number, sizeof(number), "%d", i + 1);

    canvas::fill(c, row, inverted ? canvas::gray::black : canvas::gray::white);
    if (selected && !focused) {
      canvas::fill(c, geometry::make(row.x, row.y + 6, 5, row.h - 12), canvas::gray::black);
    }
    if (!inverted) canvas::hline(c, row.x, row.y + row.h - 1, row.w, canvas::gray::light);

    constexpr int index_w = 28, cover_w = 42, cover_h = 58;
    const geometry::rect index_rect = geometry::make(row.x + 2, row.y, index_w, row.h);
    const geometry::rect cover = geometry::make(index_rect.x + index_rect.w + 6,
                                                row.y + (row.h - cover_h) / 2, cover_w, cover_h);
    const int text_x = cover.x + cover.w + 10;
    const geometry::rect text = geometry::make(text_x, row.y, row.x + row.w - text_x - 8, row.h);
    const int title_h = p.shell->theme_ptr->font_normal->line_height;
    const int secondary_h = p.shell->theme_ptr->font_small->line_height;
    const int text_y = row.y + (row.h - title_h - secondary_h) / 2;

    canvas::draw_text_in(c, *p.shell->theme_ptr->font_small, index_rect, number, text::align::right,
                         fg);
    draw_cover(c, cover, lp.app->library.books[lp.book_index[i]], lp.book_index[i]);
    canvas::draw_text_in(c, *p.shell->theme_ptr->font_normal,
                         geometry::make(text.x, text_y, text.w, title_h), title, text::align::left,
                         fg);
    canvas::draw_text_in(c, *p.shell->theme_ptr->font_small,
                         geometry::make(text.x, text_y + title_h, text.w, secondary_h), author,
                         text::align::left, secondary_fg);
  }

  if (list.count > visible) {
    const geometry::rect track = geometry::make(
        list.base_widget.rect.x + list.base_widget.rect.w - 6, list.base_widget.rect.y, 6,
        visible * list.row_h);
    canvas::fill(c, track, canvas::gray::white);
    canvas::vline(c, track.x + 3, track.y, track.h, canvas::gray::light);
    const int thumb_h = std::max(track.h * visible / list.count, 16);
    const int thumb_y = track.y + (track.h - thumb_h) * list.top / std::max(list.count - visible, 1);
    canvas::fill(c, geometry::make(track.x, thumb_y, 6, thumb_h), canvas::gray::black);
  }
}

inline void update_title(library_page& lp) {
  std::snprintf(lp.title, sizeof(lp.title), "%s (%d)", lp.favorites_only ? "Favorites" : "Library",
               lp.list.count);
  page::set_title(lp.base, lp.title);
}

inline void library_sync(library_page& lp) {
  int n = 0;
  for (int i = 0; i < lp.app->library.count; ++i) {
    if (!lp.app->library.books[i].transient &&
        (!lp.favorites_only || lp.app->library.books[i].favorite)) {
      lp.book_index[n++] = i;
    }
  }
  widget::set_count(lp.list, n);
  update_title(lp);
}

inline int selected_book(const library_page& lp) {
  return (lp.list.selected >= 0 && lp.list.selected < lp.list.count) ? lp.book_index[lp.list.selected]
                                                                      : -1;
}

inline void library_selected(widget::list& l, int index) {
  library_page& lp = *static_cast<library_page*>(l.user);
  if (index >= 0 && index < l.count) show_book_info(*lp.app, *lp.nav, lp.book_index[index]);
}

inline void library_create(page::context& p) {
  library_page& lp = library_of(p);
  const shell::theme& t = theme_of(p);
  const geometry::rect a = p.area;
  const geometry::rect lr = geometry::make(a.x + 4, a.y + 4, a.w - 12, a.h - 8);
  widget::init(lp.list, lr, widget::list_style::two_line, t.row_h, *t.font_normal, *t.font_small,
               library_row, &lp);
  lp.list.on_select = library_selected;
  library_sync(lp);
  page::add(p, lp.list.base_widget);
  update_title(lp);
}

inline void library_action(page::context& p, std::uint16_t id) {
  library_page& lp = library_of(p);
  const int book = selected_book(lp);
  if (book < 0 && id != static_cast<std::uint16_t>(library_action::back)) return;
  if (id == static_cast<std::uint16_t>(library_action::favorite)) {
    lp.app->library.books[book].favorite = !lp.favorites_only;
    if (lp.favorites_only) library_sync(lp);
    else widget::invalidate_row(lp.list, lp.list.selected, refresh::mode::quality);
  } else if (id == static_cast<std::uint16_t>(library_action::del)) {
    std::snprintf(lp.confirm_msg, sizeof(lp.confirm_msg), "Delete \"%s\" from the device?",
                 lp.app->library.books[book].title.data());
    lp.pending_book = book;
    show_delete_confirm(*lp.app, *lp.nav, lp);
  } else if (id == static_cast<std::uint16_t>(library_action::back)) {
    shell::pop(*p.shell);
  }
}

inline constexpr page::vtbl library_vtbl{
    library_create, nullptr, nullptr, nullptr, nullptr, library_render, nullptr, library_action,
    nullptr,
};

// -------------------------------------------------------------------- file manager

inline constexpr int file_manager_max_entries = 64;

enum class file_manager_action : std::uint16_t { up_back = 1 };

inline void up_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::arrow_back, g);
}
inline constexpr page::action file_manager_actions[] = {
    {static_cast<std::uint16_t>(file_manager_action::up_back), "Up / Back", event::key_code::none,
     up_icon},
};

inline bool is_epub(const char* name) {
  const std::size_t length = std::strlen(name);
  return (length > 5 && strcasecmp(name + length - 5, ".epub") == 0) ||
         (length > 4 && strcasecmp(name + length - 4, ".epu") == 0);
}

struct file_entry {
  char name[128] = {0};
  char display[128] = {0};
  bool directory = false;
};

struct file_manager_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
  widget::list list{};
  file_entry entries[file_manager_max_entries]{};
  char path[storage::path_max] = {0};
  char title[128] = {0};
  int count = 0;
};

inline file_manager_page& file_manager_of(page::context& p) {
  return *reinterpret_cast<file_manager_page*>(reinterpret_cast<char*>(&p) -
                                               offsetof(file_manager_page, base));
}

inline bool file_manager_collect_entry(const storage::entry& value, void* user) {
  auto& page = *static_cast<file_manager_page*>(user);
  if (value.name == nullptr || value.name[0] == '\0' || (!value.directory && !is_epub(value.name))) {
    return true;
  }
  if (page.count >= file_manager_max_entries) return false;
  file_entry& entry = page.entries[page.count++];
  std::snprintf(entry.name, sizeof(entry.name), "%s", value.name);
  entry.directory = value.directory;
  book::make_title(entry.display, sizeof(entry.display), value.name);
  return true;
}

inline const char* path_name(const char* path) {
  const char* slash = std::strrchr(path, '/');
  return (slash != nullptr && slash[1] != '\0') ? slash + 1 : path;
}

inline void file_manager_load_directory(file_manager_page& page, const char* path) {
  std::snprintf(page.path, sizeof(page.path), "%s", path);
  page.count = 0;
  storage::device* storage_device =
      capability::get<storage::device>(*page.app->capabilities, capability::id::storage);
  storage::list(*storage_device, page.path, file_manager_collect_entry, &page);
  widget::set_count(page.list, page.count);
  std::snprintf(page.title, sizeof(page.title), "Files: %.120s", path_name(page.path));
  page::set_title(page.base, page.title);
}

inline void file_manager_row(widget::list& l, int index, const char** primary,
                             const char** secondary) {
  auto& page = *static_cast<file_manager_page*>(l.user);
  file_entry& entry = page.entries[index];
  *primary = entry.display;
  *secondary = entry.directory ? "Folder" : "EPUB";
}

inline void file_manager_render(page::context& base, canvas::surface& canvas) {
  file_manager_page& page = file_manager_of(base);
  widget::list& list = page.list;
  const int visible = widget::visible_rows(list);
  const bool focused = widget::has_focus(list.base_widget);
  const int row_width = list.base_widget.rect.w - (list.count > visible ? 10 : 0);

  for (int i = list.top; i < list.count && i < list.top + visible; ++i) {
    const geometry::rect row = geometry::make(
        list.base_widget.rect.x, list.base_widget.rect.y + (i - list.top) * list.row_h, row_width,
        list.row_h);
    if (!geometry::intersects(row, canvas.clip)) continue;
    const bool inverted = i == list.selected && focused;
    const std::uint8_t foreground = inverted ? canvas::gray::white : canvas::gray::black;
    const std::uint8_t secondary = inverted ? canvas::gray::white : canvas::gray::dark;
    file_entry& entry = page.entries[i];
    const icon entry_icon = entry.directory ? icon::folder : icon::description;
    const int icon_y = row.y + (row.h - 24) / 2;
    const int text_x = row.x + 42;
    const int primary_height = base.shell->theme_ptr->font_normal->line_height;
    const int secondary_height = base.shell->theme_ptr->font_small->line_height;
    const int text_y = row.y + (row.h - primary_height - secondary_height) / 2;

    canvas::fill(canvas, row, inverted ? canvas::gray::black : canvas::gray::white);
    if (i == list.selected && !focused) {
      canvas::fill(canvas, geometry::make(row.x, row.y + 6, 5, row.h - 12), canvas::gray::black);
    }
    if (!inverted) canvas::hline(canvas, row.x, row.y + row.h - 1, row.w, canvas::gray::light);
    draw_icon(canvas, geometry::make(row.x + 10, icon_y, 24, 24), entry_icon, foreground);
    canvas::draw_text_in(canvas, *base.shell->theme_ptr->font_normal,
                         geometry::make(text_x, text_y, row.x + row.w - text_x - 8, primary_height),
                         entry.display, text::align::left, foreground);
    canvas::draw_text_in(canvas, *base.shell->theme_ptr->font_small,
                         geometry::make(text_x, text_y + primary_height,
                                        row.x + row.w - text_x - 8, secondary_height),
                         entry.directory ? "Folder" : "EPUB", text::align::left, secondary);
  }
}

inline bool make_child_path(char* output, std::size_t capacity, const char* parent,
                            const char* name) {
  const int written = std::snprintf(output, capacity, "%s%s%s", parent,
                                    (parent[0] != '\0' && parent[std::strlen(parent) - 1] == '/')
                                        ? ""
                                        : "/",
                                    name);
  return written >= 0 && static_cast<std::size_t>(written) < capacity;
}

inline void file_manager_selected(widget::list& list, int index) {
  auto& page = *static_cast<file_manager_page*>(list.user);
  if (index < 0 || index >= page.count) return;
  file_entry& entry = page.entries[index];
  char path[storage::path_max];
  if (!make_child_path(path, sizeof(path), page.path, entry.name)) return;
  if (entry.directory) {
    file_manager_load_directory(page, path);
    return;
  }
  const char* root = page.app->storage_root;
  const std::size_t root_length = std::strlen(root);
  const char* open_path = path;
  if (root_length != 0 && std::strncmp(path, root, root_length) == 0 &&
      (path[root_length] == '/' || path[root_length] == '\0')) {
    open_path = path + root_length;
    if (*open_path == '/') ++open_path;
  }
  if (library::add(page.app->library, open_path, entry.display)) {
    open_book(*page.app, *page.nav, page.app->library.count - 1);
  }
}

inline void file_manager_create(page::context& base) {
  file_manager_page& page = file_manager_of(base);
  const shell::theme& theme = theme_of(base);
  const geometry::rect area = base.area;
  widget::init(page.list, geometry::make(area.x + 4, area.y + 4, area.w - 12, area.h - 8),
               widget::list_style::two_line, theme.row_h, *theme.font_normal, *theme.font_small,
               file_manager_row, &page);
  page.list.on_select = file_manager_selected;
  page::add(base, page.list.base_widget);
  file_manager_load_directory(page, page.app->storage_root);
}

inline void file_manager_action(page::context& base, std::uint16_t action) {
  if (action != static_cast<std::uint16_t>(file_manager_action::up_back)) return;
  file_manager_page& page = file_manager_of(base);
  const char* root = page.app->storage_root;
  if (std::strcmp(page.path, root) == 0) {
    shell::pop(*base.shell);
    return;
  }
  char parent[storage::path_max];
  std::snprintf(parent, sizeof(parent), "%s", page.path);
  char* slash = std::strrchr(parent, '/');
  if (slash == nullptr || slash < parent + std::strlen(root)) {
    std::snprintf(parent, sizeof(parent), "%s", root);
  } else if (slash == parent) {
    slash[1] = '\0';
  } else {
    *slash = '\0';
  }
  file_manager_load_directory(page, parent);
}

inline constexpr page::vtbl file_manager_vtbl{
    file_manager_create, nullptr, nullptr, nullptr, nullptr, file_manager_render, nullptr,
    file_manager_action, nullptr,
};

// ------------------------------------------------------------------------ settings

enum settings_row {
  row_wifi = 0,
  row_bluetooth,
  row_font,
  row_refresh,
  row_progress,
  row_sleep,
  row_about,
  row_count,
};

inline constexpr int refresh_choices[] = {1, 3, 6, 10, 0};
inline constexpr page::action settings_actions[] = {
    {1, "Back", event::key_code::none, back_icon},
};

struct settings_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
  widget::list list{};
  char value[32] = {0};
};

inline settings_page& settings_of(page::context& p) {
  return *reinterpret_cast<settings_page*>(reinterpret_cast<char*>(&p) - offsetof(settings_page, base));
}

inline void settings_row_fn(widget::list& l, int i, const char** primary, const char** secondary) {
  settings_page& sp = *static_cast<settings_page*>(l.user);
  state::store& memory = *sp.app->memory;
  switch (i) {
    case row_wifi:
      *primary = "Wi-Fi";
      *secondary = state::get(memory, key::wifi_connected, false) ? "Connected" : "Off";
      break;
    case row_bluetooth:
      *primary = "Bluetooth";
      *secondary = state::get(memory, key::bluetooth_enabled, false) ? "Connected" : "Off";
      break;
    case row_font:
      *primary = "Font size";
      *secondary = font_names[state::get(memory, key::font_size, std::int64_t{1})];
      break;
    case row_refresh: {
      *primary = "Full refresh";
      const std::int64_t every = state::get(memory, key::full_refresh_every, std::int64_t{6});
      if (every == 0) *secondary = "Never";
      else if (every == 1) *secondary = "Every page";
      else {
        std::snprintf(sp.value, sizeof(sp.value), "Every %lld pages", static_cast<long long>(every));
        *secondary = sp.value;
      }
      break;
    }
    case row_progress:
      *primary = "Progress bar";
      *secondary = state::get(memory, key::show_progress, true) ? "On" : "Off";
      break;
    case row_sleep:
      *primary = "Sleep timeout";
      std::snprintf(sp.value, sizeof(sp.value), "%lld minutes",
                    static_cast<long long>(state::get(memory, key::sleep_timeout_minutes,
                                                      std::int64_t{10})));
      *secondary = sp.value;
      break;
    default:
      *primary = "About";
      *secondary = "X-Reader 0.1";
      break;
  }
}

inline void settings_selected(widget::list& l, int i) {
  settings_page& sp = *static_cast<settings_page*>(l.user);
  state::store& memory = *sp.app->memory;
  switch (i) {
    case row_wifi:
      state::set(memory, key::wifi_connected, !state::get(memory, key::wifi_connected, false));
      break;
    case row_bluetooth:
      state::set(memory, key::bluetooth_enabled, !state::get(memory, key::bluetooth_enabled, false));
      break;
    case row_font:
      state::set(memory, key::font_size, (state::get(memory, key::font_size, std::int64_t{1}) + 1) % 3);
      break;
    case row_refresh: {
      constexpr int n = sizeof(refresh_choices) / sizeof(refresh_choices[0]);
      const std::int64_t current = state::get(memory, key::full_refresh_every, std::int64_t{6});
      int k = 0;
      for (int j = 0; j < n; ++j) {
        if (refresh_choices[j] == current) k = j;
      }
      const std::int64_t next = refresh_choices[(k + 1) % n];
      state::set(memory, key::full_refresh_every, next);
      shell::set_full_refresh_every(sp.app->shell, static_cast<std::uint16_t>(next));
      break;
    }
    case row_progress:
      state::set(memory, key::show_progress, !state::get(memory, key::show_progress, true));
      break;
    case row_sleep: {
      const std::int64_t current = state::get(memory, key::sleep_timeout_minutes, std::int64_t{10});
      state::set(memory, key::sleep_timeout_minutes,
                static_cast<std::int64_t>(current == 10 ? 30 : current == 30 ? 60 : 10));
      break;
    }
    case row_about:
      show_about_confirm(*sp.app, *sp.nav);
      return;
    default:
      return;
  }
  // Only the value text changed: refresh just that row.
  widget::invalidate_row(l, i, refresh::mode::quality);
}

inline void settings_create(page::context& p) {
  settings_page& sp = settings_of(p);
  const shell::theme& t = theme_of(p);
  const geometry::rect a = p.area;
  widget::init(sp.list, geometry::make(a.x + 4, a.y + 4, a.w - 8, a.h - 8), widget::list_style::value,
               t.row_h - 8, *t.font_normal, *t.font_normal, settings_row_fn, &sp);
  sp.list.on_select = settings_selected;
  widget::set_count(sp.list, row_count);
  page::add(p, sp.list.base_widget);
}

inline void settings_action(page::context& p, std::uint16_t) { shell::pop(*p.shell); }

inline constexpr page::vtbl settings_vtbl{
    settings_create, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, settings_action,
    nullptr,
};

// ---------------------------------------------------------------------------- sleep

struct sleep_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
};

inline sleep_page& sleep_of(page::context& p) {
  return *reinterpret_cast<sleep_page*>(reinterpret_cast<char*>(&p) - offsetof(sleep_page, base));
}

inline void sleep_render(page::context& p, canvas::surface& c) {
  const shell::theme& t = theme_of(p);
  const geometry::rect a = p.area;
  const int cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  canvas::fill(c, a, canvas::gray::black);
  canvas::border(c, geometry::make(cx - 38, cy - 52, 76, 104), 3, canvas::gray::white);
  canvas::hline(c, cx - 20, cy - 20, 40, canvas::gray::white);
  canvas::hline(c, cx - 20, cy - 4, 40, canvas::gray::white);
  canvas::hline(c, cx - 20, cy + 12, 28, canvas::gray::white);
  canvas::draw_text_in(c, *t.font_small, geometry::make(a.x, cy + 130, a.w, 30), "Sleeping",
                       text::align::center, canvas::gray::white);
}

inline bool sleep_event(page::context& p, const event::value& ev) {
  if (ev.kind == event::type::key || ev.kind == event::type::tap) {
    sleep_page& sp = sleep_of(p);
    shell::replace(*p.shell, *page_home(*sp.app, *sp.nav));
    return true;
  }
  return false;
}

inline constexpr page::vtbl sleep_vtbl{
    nullptr, nullptr, nullptr, nullptr, nullptr, sleep_render, sleep_event, nullptr, nullptr,
};

// --------------------------------------------------------------------------- reader

inline constexpr int reader_margin = 28;

enum class reader_action : std::uint16_t { smaller = 1, larger, close };

inline void smaller_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::remove, g);
}
inline void larger_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::add, g);
}
inline void close_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::close, g);
}
inline constexpr page::action reader_actions[] = {
    {static_cast<std::uint16_t>(reader_action::close), "Close", event::key_code::none, close_icon},
    {static_cast<std::uint16_t>(reader_action::smaller), "A-", event::key_code::none, smaller_icon},
    {static_cast<std::uint16_t>(reader_action::larger), "A+", event::key_code::none, larger_icon},
};

struct reader_page {
  page::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
  geometry::rect text_rect{};
  geometry::rect footer_rect{};
};

inline reader_page& reader_of(page::context& p) {
  return *reinterpret_cast<reader_page*>(reinterpret_cast<char*>(&p) - offsetof(reader_page, base));
}

inline const text::font& reader_body_font(context& app) {
  return *body_fonts[state::get(*app.memory, key::font_size, std::int64_t{1})];
}

inline void reader_layout(page::context& p) {
  reader_page& rp = reader_of(p);
  const shell::theme& t = theme_of(p);
  const int footer_h = t.font_small->line_height + 12;
  const geometry::rect a = p.area;
  rp.text_rect =
      geometry::make(a.x + reader_margin, a.y + reader_margin, a.w - 2 * reader_margin,
                     a.h - reader_margin - footer_h - 8);
  rp.footer_rect = geometry::make(a.x + reader_margin, a.y + a.h - footer_h - 4,
                                  a.w - 2 * reader_margin, footer_h);
  reader::paginate(rp.app->session, reader_body_font(*rp.app), rp.text_rect);
  rp.app->session.total_pages = rp.app->session.page_count;
  const int percent = reader::progress_percent(rp.app->session, rp.app->session.total_pages);
  const std::int64_t current = state::get(*rp.app->memory, key::book_current, std::int64_t{-1});
  if (percent >= 0 && current >= 0 && current < rp.app->library.count) {
    rp.app->library.books[current].progress = static_cast<std::uint8_t>(percent);
  }
}

inline void reader_create(page::context& p) {
  reader_page& rp = reader_of(p);
  const std::int64_t current = state::get(*rp.app->memory, key::book_current, std::int64_t{-1});
  if (current >= 0 && current < rp.app->library.count) {
    p.title = rp.app->library.books[current].title.data();
  }
  reader_layout(p);
}

inline void reader_render(page::context& p, canvas::surface& c) {
  reader_page& rp = reader_of(p);
  reader::session& s = rp.app->session;
  const shell::theme& t = theme_of(p);
  const text::font& f = reader_body_font(*rp.app);

  if (reader::is_cover_placeholder(s)) {
    const geometry::rect cover =
        geometry::make(p.area.x + p.area.w / 2 - 125, p.area.y + 80, 250, 360);
    canvas::fill(c, cover, canvas::gray::light);
    canvas::border(c, cover, 4, canvas::gray::black);
    canvas::fill(c, geometry::make(cover.x + 18, cover.y + 22, cover.w - 36, 38), canvas::gray::black);
    canvas::draw_text_in(c, *t.font_small,
                         geometry::make(cover.x + 12, cover.y + 25, cover.w - 24, 32), "X-READER",
                         text::align::center, canvas::gray::white);
    canvas::draw_text_wrapped(c, *t.font_title,
                              geometry::make(cover.x + 24, cover.y + 100, cover.w - 48, 100), p.title,
                              text::align::center, canvas::gray::black);
    canvas::hline(c, cover.x + 42, cover.y + 250, cover.w - 84, canvas::gray::dark);
    canvas::hline(c, cover.x + 62, cover.y + 270, cover.w - 124, canvas::gray::dark);
    return;
  }

  const char* body = reader::current_text(s);
  std::size_t off = s.page_start[s.page];
  int y = rp.text_rect.y;
  for (int l = 0; l < s.lines_per_page && body[off] != '\0'; ++l) {
    std::size_t next = 0;
    const std::size_t n = text::wrap(f, body + off, rp.text_rect.w, next);
    canvas::draw_text(c, f, rp.text_rect.x, y, body + off, static_cast<int>(n), canvas::gray::black);
    y += f.line_height;
    off += next;
  }

  const geometry::rect fr = rp.footer_rect;
  char pages[24];
  std::snprintf(pages, sizeof(pages), "%d / %d", s.page + 1, s.page_count);
  const int pw = text::width(*t.font_small, pages, -1);
  canvas::draw_text_in(c, *t.font_small, geometry::make(fr.x, fr.y, fr.w - pw - t.pad, fr.h),
                       reader::current_chapter_title(s), text::align::left, canvas::gray::dark);
  canvas::draw_text_in(c, *t.font_small, fr, pages, text::align::right, canvas::gray::dark);
  if (state::get(*rp.app->memory, key::show_progress, true)) {
    const int done = fr.w * (s.page + 1) / s.page_count;
    canvas::hline(c, fr.x, fr.y, fr.w, canvas::gray::light);
    canvas::fill(c, geometry::make(fr.x, fr.y - 1, done, 3), canvas::gray::black);
  }
}

inline void reader_turn(reader_page& rp, int delta) {
  reader::session& s = rp.app->session;
  const text::font& f = reader_body_font(*rp.app);
  if (!reader::turn_page(s, delta, f, rp.text_rect)) return;
  const int percent = reader::progress_percent(s, s.total_pages);
  const std::int64_t current = state::get(*rp.app->memory, key::book_current, std::int64_t{-1});
  if (percent >= 0 && current >= 0 && current < rp.app->library.count) {
    rp.app->library.books[current].progress = static_cast<std::uint8_t>(percent);
  }
  page::invalidate(rp.base, refresh::mode::quality);
}

inline void reader_toggle_chrome(page::context& p) {
  shell::set_chrome(*p.shell, p, p.chrome != 0 ? page::chrome::none : page::chrome::all);
}

inline bool reader_event(page::context& p, const event::value& ev) {
  reader_page& rp = reader_of(p);
  if (ev.kind == event::type::tap) {
    const int third = p.area.w / 3;
    if (ev.x < p.area.x + third) reader_turn(rp, -1);
    else if (ev.x >= p.area.x + 2 * third) reader_turn(rp, +1);
    else reader_toggle_chrome(p);
    return true;
  }
  switch (ev.key) {
    case event::key_code::next:
    case event::key_code::right:
      reader_turn(rp, +1);
      return true;
    case event::key_code::down:
      if (p.chrome != 0) return false;  // with chrome visible, Down moves into the dock.
      reader_turn(rp, +1);
      return true;
    case event::key_code::prev:
    case event::key_code::left:
    case event::key_code::up:
      reader_turn(rp, -1);
      return true;
    case event::key_code::menu:
    case event::key_code::ok:
      reader_toggle_chrome(p);
      return true;
    case event::key_code::back:
      if (p.chrome != 0) {
        reader_toggle_chrome(p);
        return true;
      }
      return false;  // shell pops the page.
    default:
      return false;
  }
}

inline void reader_set_font(reader_page& rp, std::int64_t idx) {
  if (idx < 0 || idx > 2 || idx == state::get(*rp.app->memory, key::font_size, std::int64_t{1})) {
    return;
  }
  state::set(*rp.app->memory, key::font_size, idx);
  // Font changes alter every chapter's page count, so rebuild the whole-book page map
  // before updating the progress bar.
  reader_layout(rp.base);
  page::invalidate(rp.base, refresh::mode::quality);
}

inline void reader_action(page::context& p, std::uint16_t id) {
  reader_page& rp = reader_of(p);
  const std::int64_t font_size = state::get(*rp.app->memory, key::font_size, std::int64_t{1});
  if (id == static_cast<std::uint16_t>(reader_action::smaller)) reader_set_font(rp, font_size - 1);
  else if (id == static_cast<std::uint16_t>(reader_action::larger)) reader_set_font(rp, font_size + 1);
  else if (id == static_cast<std::uint16_t>(reader_action::close)) shell::pop(*p.shell);
}

inline constexpr page::vtbl reader_vtbl{
    reader_create, reader_layout, nullptr, nullptr, nullptr, reader_render, reader_event,
    reader_action, nullptr,
};

// --------------------------------------------------------------------- book info dialog

inline void book_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  canvas::border(c, geometry::make(r.x + 3, r.y + 1, r.w - 6, r.h - 2), 2, g);
  canvas::vline(c, r.x + r.w / 2, r.y + 3, r.h - 6, g);
}

struct book_info_dialog {
  dialog::context base{};
  context* app = nullptr;
  pages* nav = nullptr;
  int book = -1;
  widget::label title{}, author{}, meta{};
  widget::button read{}, close{};
  geometry::rect progress_rect{};
  char meta_buf[64] = {0};
};

inline book_info_dialog& info_of(dialog::context& d) {
  return *reinterpret_cast<book_info_dialog*>(reinterpret_cast<char*>(&d) -
                                              offsetof(book_info_dialog, base));
}

inline void info_clicked(widget::base& w, void* user) {
  auto& bi = *static_cast<book_info_dialog*>(user);
  dialog::close(bi.base, (&w == &bi.read.base_widget) ? dialog::result::yes : dialog::result::no);
}

inline void info_layout(dialog::context& d, geometry::rect bounds) {
  book_info_dialog& bi = info_of(d);
  const shell::theme& t = app::theme_of(d);
  const book::item& b = bi.app->library.books[bi.book];
  const int pad = t.pad;
  const int w = std::min(bounds.w * 90 / 100, 480);
  const int iw = w - 2 * pad;

  std::snprintf(bi.meta_buf, sizeof(bi.meta_buf), "%s  |  %u pages  |  %u KB", b.format,
               static_cast<unsigned>(b.pages), static_cast<unsigned>(b.size_kb));

  const int title_h = text::measure_height(*t.font_title, b.title.data(), iw);
  const int line_h = t.font_normal->line_height;
  const int small_h = t.font_small->line_height;
  const int btn_h = t.row_h - 8;
  const int head = dialog::header_height(d);
  const int h = head + pad + title_h + line_h + small_h + pad + 18 + pad + btn_h + pad;

  dialog::place(d, bounds, w, h);
  const geometry::rect r = d.rect;
  int x = r.x + pad, y = r.y + head + pad;

  widget::init(bi.title, geometry::make(x, y, iw, title_h), b.title.data(), *t.font_title,
               text::align::left);
  bi.title.wrap = true;
  y += title_h;
  widget::init(bi.author, geometry::make(x, y, iw, line_h), b.author, *t.font_normal,
               text::align::left);
  y += line_h;
  widget::init(bi.meta, geometry::make(x, y, iw, small_h), bi.meta_buf, *t.font_small,
               text::align::left);
  bi.meta.gray = canvas::gray::dark;
  y += small_h + pad;
  bi.progress_rect = geometry::make(x, y, iw, 18);
  y += 18 + pad;

  const int bw = (iw - pad) / 2;
  widget::init(bi.read, geometry::make(x, y, bw, btn_h), b.progress != 0 ? "Continue" : "Read",
               *t.font_bold, info_clicked, &bi);
  widget::init(bi.close, geometry::make(x + bw + pad, y, bw, btn_h), "Close", *t.font_bold,
               info_clicked, &bi);

  widget::add(d.scope.root, bi.title.base_widget);
  widget::add(d.scope.root, bi.author.base_widget);
  widget::add(d.scope.root, bi.meta.base_widget);
  widget::add(d.scope.root, bi.read.base_widget);
  widget::add(d.scope.root, bi.close.base_widget);
  widget::set_focus(d.scope, &bi.read.base_widget);
}

inline void info_render(dialog::context& d, canvas::surface& c) {
  book_info_dialog& bi = info_of(d);
  dialog::default_render(d, c);
  draw_progress(c, bi.progress_rect, bi.app->library.books[bi.book].progress);
}

inline void info_result(dialog::context& d, dialog::result result, void*) {
  book_info_dialog& bi = info_of(d);
  if (result == dialog::result::yes) open_book(*bi.app, *bi.nav, bi.book);
}

inline constexpr dialog::vtbl info_vtbl{info_layout, info_render, nullptr};

// ------------------------------------------------------------------- services / navigation

inline void delete_confirm_result(dialog::context& d, dialog::result result, void* user) {
  (void)d;
  auto& lp = *static_cast<library_page*>(user);
  if (result != dialog::result::yes) return;
  library::remove(lp.app->library, lp.pending_book);
  library_sync(lp);
}

inline void about_confirm_result(dialog::context&, dialog::result, void*) {}
// show_delete_confirm/show_about_confirm are defined after `struct pages` below (they take
// pages&, which must be complete).

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

// --------------------------------------------------------------------------- composition

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
  dialog::show_confirm(app.shell, nav.delete_confirm, "Delete book", requester.confirm_msg, "Delete",
                       "Cancel", delete_confirm_result, &requester);
  dialog::set_icon(nav.delete_confirm.base, delete_icon);
}

inline void show_about_confirm(context& app, pages& nav) {
  // app_confirm always hardcoded "Cancel" as the no-button label, even for this
  // non-destructive notice -- preserved exactly, odd as it looks.
  dialog::show_confirm(app.shell, nav.about_confirm, "About X-Reader",
                       "Author: X-Reader Team\nSoftware: X-Reader 0.1\nRelease: 2026-10-04", "Close",
                       "Cancel", about_confirm_result, nullptr);
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
  if (state::find(memory, key::font_size) == nullptr) state::set(memory, key::font_size, std::int64_t{1});
  if (state::find(memory, key::full_refresh_every) == nullptr) {
    state::set(memory, key::full_refresh_every, std::int64_t{6});
  }
  if (state::find(memory, key::show_progress) == nullptr) state::set(memory, key::show_progress, true);
  if (state::find(memory, key::sleep_timeout_minutes) == nullptr) {
    state::set(memory, key::sleep_timeout_minutes, std::int64_t{10});
  }
  if (state::find(memory, key::book_current) == nullptr) {
    state::set(memory, key::book_current, std::int64_t{-1});
  }
  shell::set_full_refresh_every(self.shell,
                                static_cast<std::uint16_t>(state::get(memory, key::full_refresh_every,
                                                                      std::int64_t{6})));

  library::scan(self.library, *storage_device, storage_root);

  shell::push(self.shell, *page_splash(self, nav));
  return true;
}

/** @brief Polls queued input, dispatches it, ticks the top page, and flushes any redraw. */
inline void pump(context& self) {
  event::value ev{};
  while (input::poll(*self.input, ev)) shell::dispatch(self.shell, ev);
  shell::tick(self.shell);
  shell::flush(self.shell);
}

}  // namespace app
