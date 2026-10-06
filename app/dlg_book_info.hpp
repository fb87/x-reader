#pragma once

#include "assets.hpp"
#include "pages_fwd.hpp"

/** @brief The book-info modal (Read/Close + progress), ported from app/dlg_book_info.c. */
namespace app {

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
  book::item& metadata = bi.app->library.books[bi.book];
  if (metadata.epub_source) {
    auto* storage_device =
        capability::get<storage::device>(*bi.app->capabilities, capability::id::storage);
    if (storage_device != nullptr) {
      epub::file_view view{storage_device, metadata.path.data(), 0};
      if (storage::file_size(*storage_device, view.path, view.size)) {
        metadata.size_kb = static_cast<std::uint16_t>(
            std::min<std::uint32_t>((view.size + 1023U) / 1024U, 65535U));
        if (reader::open(bi.app->session, view, metadata.title.data())) {
          if (bi.app->session.doc.title[0] != '\0') {
            std::snprintf(metadata.title.data(), metadata.title.size(), "%s",
                          bi.app->session.doc.title);
          }
          // Keep opening the dialog responsive. The EPUB spine count is immediately
          // available and replaces the old zero placeholder; full pagination remains
          // lazy and is performed by the reader as chapters are opened.
          metadata.pages = bi.app->session.doc.spine_count;
        }
      }
    }
  }
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

}  // namespace app
