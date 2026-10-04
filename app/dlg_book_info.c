/* Book info dialog: content-sized popup with a custom-drawn progress bar. */
#include "app_internal.h"

#include <stdio.h>

typedef struct book_info_dialog {
    xr_dialog_t base;
    int book;
    xr_label_t title, author, meta;
    xr_button_t read, close;
    xr_rect_t progress_rect;
    char meta_buf[64];
} book_info_dialog_t;

static book_info_dialog_t s_info;

static void book_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    xr_canvas_draw_rect(c, xr_rect(r.x + 3, r.y + 1, r.w - 6, r.h - 2), 2, gray);
    xr_canvas_vline(c, r.x + r.w / 2, r.y + 3, r.h - 6, gray);
}

static void info_clicked(xr_widget_t *w, void *user)
{
    book_info_dialog_t *bi = (book_info_dialog_t *)user;
    xr_dialog_close(&bi->base, (w == &bi->read.base) ? XR_RESULT_YES : XR_RESULT_NO);
}

static void info_layout(xr_dialog_t *d, xr_rect_t bounds)
{
    book_info_dialog_t *bi = XR_CONTAINER_OF(d, book_info_dialog_t, base);
    const xr_theme_t *t = d->shell->theme;
    const app_book_t *b = &g_app.books[bi->book];
    int pad = t->pad;
    int w = XR_MIN(bounds.w * 90 / 100, 480);
    int iw = w - 2 * pad;

    snprintf(bi->meta_buf, sizeof bi->meta_buf, "%s  |  %u pages  |  %u KB", b->format,
             (unsigned)b->pages, (unsigned)b->size_kb);

    /* Measure first: the dialog is as tall as its content. */
    int title_h = xr_text_measure_height(t->font_title, b->title, iw);
    int line_h = t->font_normal->line_height;
    int small_h = t->font_small->line_height;
    int btn_h = t->row_h - 8;
    int head = xr_dialog_header_height(d);
    int h = head + pad + title_h + line_h + small_h + pad + 18 + pad + btn_h + pad;

    xr_dialog_place(d, bounds, w, h);
    xr_rect_t r = d->rect;
    int x = r.x + pad, y = r.y + head + pad;

    xr_label_init(&bi->title, xr_rect(x, y, iw, title_h), b->title, t->font_title, XR_ALIGN_LEFT);
    bi->title.wrap = true;
    y += title_h;
    xr_label_init(&bi->author, xr_rect(x, y, iw, line_h), b->author, t->font_normal, XR_ALIGN_LEFT);
    y += line_h;
    xr_label_init(&bi->meta, xr_rect(x, y, iw, small_h), bi->meta_buf, t->font_small, XR_ALIGN_LEFT);
    bi->meta.gray = XR_DARK;
    y += small_h + pad;
    bi->progress_rect = xr_rect(x, y, iw, 18);
    y += 18 + pad;

    int bw = (iw - pad) / 2;
    xr_button_init(&bi->read, xr_rect(x, y, bw, btn_h), b->progress ? "Continue" : "Read",
                   t->font_bold, info_clicked, bi);
    xr_button_init(&bi->close, xr_rect(x + bw + pad, y, bw, btn_h), "Close", t->font_bold,
                   info_clicked, bi);

    xr_widget_add(&d->scope.root, &bi->title.base);
    xr_widget_add(&d->scope.root, &bi->author.base);
    xr_widget_add(&d->scope.root, &bi->meta.base);
    xr_widget_add(&d->scope.root, &bi->read.base);
    xr_widget_add(&d->scope.root, &bi->close.base);
    xr_scope_set_focus(&d->scope, &bi->read.base);
}

static void info_render(xr_dialog_t *d, xr_canvas_t *c)
{
    book_info_dialog_t *bi = XR_CONTAINER_OF(d, book_info_dialog_t, base);
    xr_dialog_default_render(d, c);
    app_draw_progress(c, bi->progress_rect, g_app.books[bi->book].progress);
}

static void info_result(xr_dialog_t *d, int result, void *user)
{
    (void)user;
    book_info_dialog_t *bi = XR_CONTAINER_OF(d, book_info_dialog_t, base);
    if (result == XR_RESULT_YES) app_open_book(bi->book);
}

static const xr_dialog_vtbl_t k_info_vtbl = { info_layout, info_render, NULL };

void app_show_book_info(int index)
{
    if (index < 0 || index >= g_app.book_count) return;
    xr_dialog_init(&s_info.base, &k_info_vtbl, "Book info");
    xr_dialog_set_icon(&s_info.base, book_icon);
    s_info.book = index;
    s_info.base.on_result = info_result;
    xr_shell_show_dialog(g_app.shell, &s_info.base);
}
