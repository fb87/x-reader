#pragma once

#include "assets.hpp"
#include "pages_fwd.hpp"

/** @brief The home page (book card + navigation rows), ported from app/page_home.c. */
namespace app {

struct book_card {
  widget::base base_widget{};
  context* app = nullptr;
  pages* nav = nullptr;
};

inline book_card& card_of(widget::base& w) {
  return *reinterpret_cast<book_card*>(reinterpret_cast<char*>(&w) -
                                       offsetof(book_card, base_widget));
}

inline void card_cover(canvas::surface& c, geometry::rect r, const book::item& b) {
  const char initial[2] = {b.title[0], '\0'};
  canvas::fill(c, r, canvas::gray::light);
  canvas::border(c, r, 2, canvas::gray::black);
  canvas::fill(c, geometry::make(r.x + 5, r.y + 5, r.w - 10, 16), canvas::gray::black);
  canvas::draw_text_in(c, xr_font_alegreya_bold_26,
                       geometry::make(r.x + 4, r.y + 24, r.w - 8, r.h - 28), initial,
                       text::align::center, canvas::gray::black);
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
  const geometry::rect details = geometry::make(cover.x + cover.w + 14, cover.y,
                                                in.x + in.w - (cover.x + cover.w + 14), cover.h);
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
  if (ev.kind == event::type::tap ||
      (ev.kind == event::type::key && ev.key == event::key_code::ok)) {
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
  const std::int64_t timeout =
      state::get(*hp.app->memory, key::sleep_timeout_minutes, std::int64_t{10});
  if (timeout > 0 && now - p.shell->last_input_ms >= static_cast<std::uint32_t>(timeout) * 60000u) {
    shell::replace(*p.shell, *page_sleep(*hp.app, *hp.nav));
  }
}

inline constexpr page::vtbl home_vtbl{
    home_create, nullptr, home_enter, nullptr, nullptr, nullptr, nullptr, nullptr, home_tick,
};

}  // namespace app
