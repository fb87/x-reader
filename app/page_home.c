/* Home: "continue reading" card + navigation. Shows a custom widget. */
#include "app_internal.h"

#include <stdio.h>

enum { ACT_LIBRARY = 1, ACT_FAVORITES, ACT_SETTINGS, ACT_SLEEP };

static void library_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
static void favorite_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
static void settings_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
static void sleep_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);

static const xr_action_t k_home_actions[] = {
    { ACT_LIBRARY, "Library", XR_KEY_NONE, library_icon },
    { ACT_FAVORITES, "Favorites", XR_KEY_NONE, favorite_icon },
    { ACT_SETTINGS, "Settings", XR_KEY_NONE, settings_icon },
    { ACT_SLEEP, "Sleep", XR_KEY_POWER, sleep_icon },
};

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
    xr_canvas_draw_text_wrapped(c, t->font_title, xr_rect(details.x, details.y, details.w, 58),
                                b->title, XR_ALIGN_LEFT, XR_BLACK);
    xr_canvas_draw_text_in(c, t->font_normal, xr_rect(details.x, details.y + 62, details.w, t->font_normal->line_height),
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
    xr_button_t library, favorites, settings;
    xr_label_t stats;
    char stats_buf[48];
} home_page_t;

static home_page_t s_home;

static void go_library(xr_widget_t *w, void *u) { (void)w; (void)u; xr_shell_push(g_app.shell, app_page_library()); }
static void go_favorites(xr_widget_t *w, void *u) { (void)w; (void)u; xr_shell_push(g_app.shell, app_page_favorites()); }
static void go_settings(xr_widget_t *w, void *u) { (void)w; (void)u; xr_shell_push(g_app.shell, app_page_settings()); }

static void library_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    xr_canvas_draw_rect(c, xr_rect(r.x + 2, r.y + 3, 6, r.h - 6), 2, gray);
    xr_canvas_draw_rect(c, xr_rect(r.x + 9, r.y + 1, 6, r.h - 4), 2, gray);
    xr_canvas_draw_rect(c, xr_rect(r.x + 16, r.y + 4, 6, r.h - 7), 2, gray);
}

static void favorite_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    xr_canvas_draw_rect(c, xr_rect(r.x + 6, r.y + 2, r.w - 12, r.h - 4), 2, gray);
    xr_canvas_vline(c, r.x + r.w / 2, r.y + 2, r.h - 4, gray);
}

static void settings_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    xr_canvas_hline(c, r.x + 2, r.y + 5, r.w - 4, gray);
    xr_canvas_hline(c, r.x + 2, r.y + 12, r.w - 4, gray);
    xr_canvas_hline(c, r.x + 2, r.y + 19, r.w - 4, gray);
    xr_canvas_fill_rect(c, xr_rect(r.x + 6, r.y + 2, 5, 7), gray);
    xr_canvas_fill_rect(c, xr_rect(r.x + 14, r.y + 9, 5, 7), gray);
    xr_canvas_fill_rect(c, xr_rect(r.x + 9, r.y + 16, 5, 7), gray);
}

static void sleep_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    xr_canvas_draw_rect(c, xr_rect(r.x + 3, r.y + 4, r.w - 6, r.h - 4), 2, gray);
    xr_canvas_vline(c, r.x + r.w / 2, r.y, 8, gray);
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
    xr_button_init(&hp->settings, xr_rect(a.x, y, a.w, t->row_h - 8), "Settings", t->font_bold, go_settings, NULL);
    xr_button_set_icon(&hp->settings, settings_icon);

    update_stats(hp);
    xr_label_init(&hp->stats, xr_rect(a.x, a.y + a.h - 30, a.w, 30), hp->stats_buf, t->font_small, XR_ALIGN_CENTER);
    hp->stats.gray = XR_DARK;

    xr_page_add(p, &hp->card.base);
    xr_page_add(p, &hp->library.base);
    xr_page_add(p, &hp->favorites.base);
    xr_page_add(p, &hp->settings.base);
    xr_page_add(p, &hp->stats.base);
}

static void home_enter(xr_page_t *p)
{
    /* Library may have changed while we were covered. */
    update_stats(XR_CONTAINER_OF(p, home_page_t, base));
}

static void sleep_result(xr_dialog_t *d, int result, void *user)
{
    (void)d; (void)user;
    if (result == XR_RESULT_YES) { /* port hook: show sleep image, deep sleep */ }
}

static void home_action(xr_page_t *p, uint16_t id)
{
    (void)p;
    switch (id) {
    case ACT_LIBRARY: go_library(NULL, NULL); break;
    case ACT_FAVORITES: go_favorites(NULL, NULL); break;
    case ACT_SETTINGS: go_settings(NULL, NULL); break;
    case ACT_SLEEP:
        app_confirm("Sleep", "Put the device to sleep now?", "Sleep", sleep_icon, sleep_result, NULL);
        break;
    }
}

static const xr_page_vtbl_t k_home_vtbl = {
    .on_create = home_create,
    .on_enter = home_enter,
    .on_action = home_action,
};

xr_page_t *app_page_home(void)
{
    xr_page_init(&s_home.base, &k_home_vtbl, "Home", XR_CHROME_ALL);
    xr_page_set_actions(&s_home.base, k_home_actions, XR_ARRAY_LEN(k_home_actions));
    return &s_home.base;
}
