/* Home: "continue reading" card + navigation. Shows a custom widget. */
#include "app_internal.h"

#include <stdio.h>

static void library_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
static void favorite_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
static void file_manager_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
static void settings_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);

/* ---- custom widget: the current-book card --------------------------- */

typedef struct book_card {
    xr_widget_t base;
} book_card_t;

static void card_cover(xr_canvas_t *c, xr_rect_t r, const app_book_t *book)
{
    char initial[2] = { book->title[0], '\0' };
    xr_canvas_fill_rect(c, r, XR_LIGHT);
    xr_canvas_draw_rect(c, r, 2, XR_BLACK);
    xr_canvas_fill_rect(c, xr_rect(r.x + 5, r.y + 5, r.w - 10, 16), XR_BLACK);
    xr_canvas_draw_text_in(c, &xr_font_alegreya_bold_26, xr_rect(r.x + 4, r.y + 24, r.w - 8, r.h - 28),
                           initial, XR_ALIGN_CENTER, XR_BLACK);
}

static void card_render(xr_widget_t *w, xr_canvas_t *c)
{
    const xr_theme_t *t = g_app.shell->theme;
    xr_rect_t r = w->rect;
    xr_canvas_fill_rect(c, r, XR_WHITE);
    xr_canvas_draw_rect(c, r, xr_widget_has_focus(w) ? 5 : 2, XR_BLACK);

    xr_rect_t in = xr_rect_inset(r, t->pad + 4);
    int y = in.y;
    xr_canvas_draw_text_in(c, t->font_small, xr_rect(in.x, y, in.w, t->font_small->line_height),
                           "CONTINUE READING", XR_ALIGN_LEFT, XR_DARK);

    if (g_app.current < 0) {
        xr_canvas_draw_text_in(c, t->font_normal, xr_rect(in.x, y + t->font_small->line_height + 6, in.w, 40), "No book open",
                               XR_ALIGN_LEFT, XR_BLACK);
        return;
    }
    const app_book_t *b = &g_app.books[g_app.current];
    xr_rect_t cover = xr_rect(in.x, y + t->font_small->line_height + 6, 82, 112);
    xr_rect_t details = xr_rect(cover.x + cover.w + 14, cover.y, in.x + in.w - (cover.x + cover.w + 14), cover.h);
    card_cover(c, cover, b);
    xr_canvas_draw_text_in(c, t->font_title, xr_rect(details.x, details.y, details.w, 42),
                           b->title, XR_ALIGN_LEFT, XR_BLACK);
    xr_canvas_draw_text_in(c, t->font_normal, xr_rect(details.x, details.y + 46, details.w, t->font_normal->line_height),
                            b->author, XR_ALIGN_LEFT, XR_DARK);

    char pct[8];
    snprintf(pct, sizeof pct, "%d%%", b->progress);
    int pw = xr_text_width(t->font_bold, pct, -1);
    int by = details.y + details.h - 18;
    app_draw_progress(c, xr_rect(details.x, by, details.w - pw - t->pad, 18), b->progress);
    xr_canvas_draw_text_in(c, t->font_bold, xr_rect(details.x + details.w - pw, by - 6, pw, 30), pct,
                            XR_ALIGN_RIGHT, XR_BLACK);
}

static bool card_event(xr_widget_t *w, const xr_event_t *ev)
{
    (void)w;
    if (ev->type == XR_EV_TAP || (ev->type == XR_EV_KEY && ev->key == XR_KEY_OK)) {
        app_open_book(g_app.current);
        return true;
    }
    return false;
}

static const xr_widget_vtbl_t k_card_vtbl = { "book_card", card_render, card_event, NULL };

/* ---- page --------------------------------------------------------------- */

typedef struct home_page {
    xr_page_t base;
    book_card_t card;
    xr_button_t library, favorites, file_manager, settings;
    xr_button_t sleep;
    xr_label_t stats;
    char stats_buf[48];
} home_page_t;

static home_page_t s_home;

static void go_library(xr_widget_t *w, void *u) { (void)w; (void)u; xr_shell_push(g_app.shell, app_page_library()); }
static void go_favorites(xr_widget_t *w, void *u) { (void)w; (void)u; xr_shell_push(g_app.shell, app_page_favorites()); }
static void go_file_manager(xr_widget_t *w, void *u) { (void)w; (void)u; xr_shell_push(g_app.shell, app_page_file_manager()); }
static void go_settings(xr_widget_t *w, void *u) { (void)w; (void)u; xr_shell_push(g_app.shell, app_page_settings()); }
static void go_sleep(xr_widget_t *w, void *u) { (void)w; (void)u; xr_shell_replace(g_app.shell, app_page_sleep()); }

static void library_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_BOOK, gray);
}

static void favorite_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_BOOK, gray);
}

static void settings_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_SETTINGS, gray);
}

static void file_manager_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_FOLDER, gray);
}

static void sleep_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_CLOSE, gray);
}

static void update_stats(home_page_t *hp)
{
    int reading = 0;
    for (int i = 0; i < g_app.book_count; i++)
        if (g_app.books[i].progress > 0 && g_app.books[i].progress < 100) reading++;
    snprintf(hp->stats_buf, sizeof hp->stats_buf, "%d books  |  %d in progress", g_app.book_count, reading);
}

static void home_create(xr_page_t *p)
{
    home_page_t *hp = XR_CONTAINER_OF(p, home_page_t, base);
    const xr_theme_t *t = p->shell->theme;
    xr_rect_t a = xr_rect_inset(p->area, t->pad);
    int y = a.y + 8;

    xr_widget_init(&hp->card.base, &k_card_vtbl, xr_rect(a.x, y, a.w, 190));
    hp->card.base.flags |= XR_WF_FOCUSABLE;
    y += 190 + t->pad * 2;

    xr_button_init(&hp->library, xr_rect(a.x, y, a.w, t->row_h - 8), "Library", t->font_bold, go_library, NULL);
    xr_button_set_icon(&hp->library, library_icon);
    y += t->row_h;
    xr_button_init(&hp->favorites, xr_rect(a.x, y, a.w, t->row_h - 8), "Favorites", t->font_bold,
                   go_favorites, NULL);
    xr_button_set_icon(&hp->favorites, favorite_icon);
    y += t->row_h;
    xr_button_init(&hp->file_manager, xr_rect(a.x, y, a.w, t->row_h - 8), "File Manager",
                   t->font_bold, go_file_manager, NULL);
    xr_button_set_icon(&hp->file_manager, file_manager_icon);
    y += t->row_h;
    xr_button_init(&hp->settings, xr_rect(a.x, y, a.w, t->row_h - 8), "Settings", t->font_bold, go_settings, NULL);
    xr_button_set_icon(&hp->settings, settings_icon);
    xr_button_init(&hp->sleep, xr_rect(a.x + a.w - 112, a.y + a.h - 42, 112, 34),
                   "Sleep", t->font_small, go_sleep, NULL);
    xr_button_set_icon(&hp->sleep, sleep_icon);

    update_stats(hp);
    xr_label_init(&hp->stats, xr_rect(a.x, a.y + a.h - 30, a.w, 30), hp->stats_buf, t->font_small, XR_ALIGN_CENTER);
    hp->stats.gray = XR_DARK;

    xr_page_add(p, &hp->card.base);
    xr_page_add(p, &hp->library.base);
    xr_page_add(p, &hp->favorites.base);
    xr_page_add(p, &hp->file_manager.base);
    xr_page_add(p, &hp->settings.base);
    xr_page_add(p, &hp->sleep.base);
    xr_page_add(p, &hp->stats.base);
}

static void home_enter(xr_page_t *p)
{
    home_page_t *hp = XR_CONTAINER_OF(p, home_page_t, base);
    /* Library or Reader may have changed while Home was covered. */
    update_stats(hp);
    xr_widget_invalidate(&hp->card.base);
    xr_widget_invalidate(&hp->stats.base);
}

static void home_tick(xr_page_t *p, uint32_t now)
{
    if (g_app.sleep_timeout_minutes > 0 &&
        now - p->shell->last_input_ms >= (uint32_t)g_app.sleep_timeout_minutes * 60000u)
        xr_shell_replace(p->shell, app_page_sleep());
}

static const xr_page_vtbl_t k_home_vtbl = {
    .on_create = home_create,
    .on_enter = home_enter,
    .on_tick = home_tick,
};

xr_page_t *app_page_home(void)
{
    xr_page_init(&s_home.base, &k_home_vtbl, "Home", XR_CHROME_STATUS);
    return &s_home.base;
}
