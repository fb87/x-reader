#pragma once

#include "assets.hpp"
#include "pages_fwd.hpp"

/** @brief The reading page, ported from app/page_reader.c. */
namespace app {

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
  rp.text_rect = geometry::make(a.x + reader_margin, a.y + reader_margin, a.w - 2 * reader_margin,
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
    canvas::fill(c, geometry::make(cover.x + 18, cover.y + 22, cover.w - 36, 38),
                 canvas::gray::black);
    canvas::draw_text_in(c, *t.font_small,
                         geometry::make(cover.x + 12, cover.y + 25, cover.w - 24, 32), "X-READER",
                         text::align::center, canvas::gray::white);
    canvas::draw_text_wrapped(c, *t.font_title,
                              geometry::make(cover.x + 24, cover.y + 100, cover.w - 48, 100),
                              p.title, text::align::center, canvas::gray::black);
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
    canvas::draw_text(c, f, rp.text_rect.x, y, body + off, static_cast<int>(n),
                      canvas::gray::black);
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
  page::invalidate(rp.base, refresh::mode::reader_quality);
}

inline void reader_toggle_chrome(page::context& p) {
  shell::set_chrome(*p.shell, p, p.chrome != 0 ? page::chrome::none : page::chrome::all);
}

inline bool reader_event(page::context& p, const event::value& ev) {
  reader_page& rp = reader_of(p);
  if (ev.kind == event::type::tap) {
    const int third = p.area.w / 3;
    if (ev.x < p.area.x + third)
      reader_turn(rp, -1);
    else if (ev.x >= p.area.x + 2 * third)
      reader_turn(rp, +1);
    else
      reader_toggle_chrome(p);
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
  page::invalidate(rp.base, refresh::mode::reader_quality);
}

inline void reader_action(page::context& p, std::uint16_t id) {
  reader_page& rp = reader_of(p);
  const std::int64_t font_size = state::get(*rp.app->memory, key::font_size, std::int64_t{1});
  if (id == static_cast<std::uint16_t>(reader_action::smaller))
    reader_set_font(rp, font_size - 1);
  else if (id == static_cast<std::uint16_t>(reader_action::larger))
    reader_set_font(rp, font_size + 1);
  else if (id == static_cast<std::uint16_t>(reader_action::close))
    shell::pop(*p.shell);
}

inline constexpr page::vtbl reader_vtbl{
    reader_create, reader_layout, nullptr,       nullptr, nullptr,
    reader_render, reader_event,  reader_action, nullptr,
};

}  // namespace app
