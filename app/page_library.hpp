#pragma once

#include "assets.hpp"
#include "pages_fwd.hpp"

/**
 * @brief The library/favorites page (one shared implementation, `favorites_only` toggles
 * the filter), ported from app/page_library.c.
 */
namespace app {

enum class library_action : std::uint16_t { favorite = 1, del, back };

inline void favorite_icon_lib(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::book, g);
}
inline void delete_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::del, g);
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
  return *reinterpret_cast<library_page*>(reinterpret_cast<char*>(&p) -
                                          offsetof(library_page, base));
}

inline void library_row(widget::list& l, int i, const char** primary, const char** secondary) {
  library_page& lp = *static_cast<library_page*>(l.user);
  const book::item& b = lp.app->library.books[lp.book_index[i]];
  *primary = b.title.data();
  if (b.progress == 0)
    std::snprintf(lp.secondary, sizeof(lp.secondary), "%s  |  New", b.author);
  else if (b.progress >= 100)
    std::snprintf(lp.secondary, sizeof(lp.secondary), "%s  |  Finished", b.author);
  else
    std::snprintf(lp.secondary, sizeof(lp.secondary), "%s  |  %d%%", b.author, b.progress);
  *secondary = lp.secondary;
}

inline void draw_cover(canvas::surface& c, geometry::rect r, const book::item& book, int index) {
  const char initial[2] = {book.title[0], '\0'};
  const std::uint8_t band = (index % 3 == 0) ? canvas::gray::black : canvas::gray::dark;
  canvas::fill(c, r, canvas::gray::light);
  canvas::border(c, r, 2, canvas::gray::black);
  canvas::fill(c, geometry::make(r.x + 3, r.y + 3, r.w - 6, 11), band);
  canvas::draw_text_in(c, xr_font_alegreya_bold_26,
                       geometry::make(r.x + 2, r.y + 16, r.w - 4, r.h - 18), initial,
                       text::align::center, canvas::gray::black);
}

inline void library_render(page::context& p, canvas::surface& c) {
  library_page& lp = library_of(p);
  widget::list& list = lp.list;
  const int visible = widget::visible_rows(list);
  const bool focused = widget::has_focus(list.base_widget);
  const int row_w = list.base_widget.rect.w - ((list.count > visible) ? 10 : 0);

  for (int i = list.top; i < list.count && i < list.top + visible; ++i) {
    const geometry::rect row =
        geometry::make(list.base_widget.rect.x,
                       list.base_widget.rect.y + (i - list.top) * list.row_h, row_w, list.row_h);
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
    const geometry::rect track =
        geometry::make(list.base_widget.rect.x + list.base_widget.rect.w - 6,
                       list.base_widget.rect.y, 6, visible * list.row_h);
    canvas::fill(c, track, canvas::gray::white);
    canvas::vline(c, track.x + 3, track.y, track.h, canvas::gray::light);
    const int thumb_h = std::max(track.h * visible / list.count, 16);
    const int thumb_y =
        track.y + (track.h - thumb_h) * list.top / std::max(list.count - visible, 1);
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
  return (lp.list.selected >= 0 && lp.list.selected < lp.list.count)
             ? lp.book_index[lp.list.selected]
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
    if (lp.favorites_only)
      library_sync(lp);
    else
      widget::invalidate_row(lp.list, lp.list.selected, refresh::mode::quality);
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
    library_create, nullptr, nullptr,        nullptr, nullptr,
    library_render, nullptr, library_action, nullptr,
};

/**
 * @brief Result callback for the delete-confirmation dialog shown by `library_action`.
 * Doesn't touch `pages` fields directly (only `library_page`/`context`), so it lives here
 * rather than in app.hpp alongside `show_delete_confirm`, which does.
 */
inline void delete_confirm_result(dialog::context& d, dialog::result result, void* user) {
  (void)d;
  auto& lp = *static_cast<library_page*>(user);
  if (result != dialog::result::yes) return;
  context& app = *lp.app;
  const int removed = lp.pending_book;

  // Mirrors the old app_delete_book's two-step book_current fixup, in the same order: first,
  // if the transient overflow slot is about to be dropped (deleting a *different*, regular
  // book while the library is full) and book_current was pointing at it, that index is gone
  // -- reset before the shift below renumbers everything else.
  const bool drops_transient = removed >= 0 && removed < app.library.count &&
                               !app.library.books[removed].transient &&
                               library::has_transient_overflow(app.library);
  std::int64_t current = state::get(*app.memory, key::book_current, std::int64_t{-1});
  if (drops_transient && current == library::max_books) current = -1;

  library::remove(app.library, removed);

  if (current == removed)
    current = app.library.count > 0 ? 0 : -1;
  else if (current > removed)
    --current;
  state::set(*app.memory, key::book_current, current);

  library_sync(lp);
}

}  // namespace app
