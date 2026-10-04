/*
 * Reader: fullscreen paginated text. Draws directly (no widgets).
 * Tap centre / MENU toggles the chrome, which changes the page area and
 * triggers on_layout -> re-pagination that keeps the reading position.
 */
#include "app_internal.h"

#include <stdio.h>
#include <string.h>

#define READER_MAX_PAGES 256
#define MARGIN 28

enum { ACT_SMALLER = 1, ACT_LARGER, ACT_CLOSE };

static void smaller_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
static void larger_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
static void close_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);

static const xr_action_t k_reader_actions[] = {
    { ACT_CLOSE, "Close", XR_KEY_NONE, close_icon },
    { ACT_SMALLER, "A-", XR_KEY_NONE, smaller_icon },
    { ACT_LARGER, "A+", XR_KEY_NONE, larger_icon },
};

typedef struct reader_page {
    xr_page_t base;
    const char *text;
    uint32_t page_start[READER_MAX_PAGES];
    int page_count, page;
    int lines_per_page;
    bool cover_placeholder;
    const char *chapter_title;
    int total_pages;
    xr_rect_t text_rect, footer_rect;
} reader_page_t;

static reader_page_t s_reader;

static const xr_font_t *body_font(void) { return app_body_fonts[g_app.font_idx]; }

static void paginate(reader_page_t *rp)
{
    const xr_font_t *f = body_font();
    uint32_t keep = rp->page_count ? rp->page_start[rp->page] : 0;
    rp->lines_per_page = XR_MAX(rp->text_rect.h / f->line_height, 1);

    size_t len = strlen(rp->text), off = 0, next;
    rp->page_count = 0;
    while (off < len && rp->page_count < READER_MAX_PAGES) {
        rp->page_start[rp->page_count++] = (uint32_t)off;
        for (int l = 0; l < rp->lines_per_page && off < len; l++) {
            xr_text_wrap(f, rp->text + off, rp->text_rect.w, &next);
            off += next;
        }
    }
    if (rp->page_count == 0) rp->page_start[rp->page_count++] = 0;

    /* Stay on the page that contains the previous first character. */
    rp->page = 0;
    for (int i = 0; i < rp->page_count; i++)
        if (rp->page_start[i] <= keep) rp->page = i;
}

static void reader_layout(xr_page_t *p)
{
    reader_page_t *rp = XR_CONTAINER_OF(p, reader_page_t, base);
    const xr_theme_t *t = p->shell->theme;
    int footer_h = t->font_small->line_height + 12;
    xr_rect_t a = p->area;
    rp->text_rect = xr_rect(a.x + MARGIN, a.y + MARGIN, a.w - 2 * MARGIN, a.h - MARGIN - footer_h - 8);
    rp->footer_rect = xr_rect(a.x + MARGIN, a.y + a.h - footer_h - 4, a.w - 2 * MARGIN, footer_h);
    paginate(rp);
    rp->total_pages = rp->page_count;
    app_set_reading_progress(rp->page, rp->total_pages);
}

static void reader_create(xr_page_t *p)
{
    reader_page_t *rp = XR_CONTAINER_OF(p, reader_page_t, base);
    rp->text = app_current_text();
    rp->cover_placeholder = app_current_is_cover_placeholder();
    rp->chapter_title = app_current_chapter_title();
    rp->page_count = 0;
    rp->page = 0;
    p->title = g_app.books[g_app.current].title;
    reader_layout(p);
}

static void reader_render(xr_page_t *p, xr_canvas_t *c)
{
    reader_page_t *rp = XR_CONTAINER_OF(p, reader_page_t, base);
    const xr_theme_t *t = p->shell->theme;
    const xr_font_t *f = body_font();

    if (rp->cover_placeholder) {
        xr_rect_t cover = xr_rect(p->area.x + p->area.w / 2 - 125, p->area.y + 80, 250, 360);
        xr_canvas_fill_rect(c, cover, XR_LIGHT);
        xr_canvas_draw_rect(c, cover, 4, XR_BLACK);
        xr_canvas_fill_rect(c, xr_rect(cover.x + 18, cover.y + 22, cover.w - 36, 38), XR_BLACK);
        xr_canvas_draw_text_in(c, t->font_small, xr_rect(cover.x + 12, cover.y + 25, cover.w - 24, 32),
                               "X-READER", XR_ALIGN_CENTER, XR_WHITE);
        xr_canvas_draw_text_wrapped(c, t->font_title,
                                    xr_rect(cover.x + 24, cover.y + 100, cover.w - 48, 100),
                                    p->title, XR_ALIGN_CENTER, XR_BLACK);
        xr_canvas_hline(c, cover.x + 42, cover.y + 250, cover.w - 84, XR_DARK);
        xr_canvas_hline(c, cover.x + 62, cover.y + 270, cover.w - 124, XR_DARK);
        return;
    }

    size_t off = rp->page_start[rp->page], next;
    int y = rp->text_rect.y;
    for (int l = 0; l < rp->lines_per_page && rp->text[off]; l++) {
        size_t n = xr_text_wrap(f, rp->text + off, rp->text_rect.w, &next);
        xr_canvas_draw_text(c, f, rp->text_rect.x, y, rp->text + off, (int)n, XR_BLACK);
        y += f->line_height;
        off += next;
    }

    /* Footer: title left, page counter right, optional thin progress line. */
    xr_rect_t fr = rp->footer_rect;
    char pages[24];
    snprintf(pages, sizeof pages, "%d / %d", rp->page + 1, rp->page_count);
    int pw = xr_text_width(t->font_small, pages, -1);
    xr_canvas_draw_text_in(c, t->font_small, xr_rect(fr.x, fr.y, fr.w - pw - t->pad, fr.h),
                           rp->chapter_title, XR_ALIGN_LEFT, XR_DARK);
    xr_canvas_draw_text_in(c, t->font_small, fr, pages, XR_ALIGN_RIGHT, XR_DARK);
    if (g_app.show_progress) {
        int done = fr.w * (rp->page + 1) / rp->page_count;
        xr_canvas_hline(c, fr.x, fr.y, fr.w, XR_LIGHT);
        xr_canvas_fill_rect(c, xr_rect(fr.x, fr.y - 1, done, 3), XR_BLACK);
    }
}

static void turn(reader_page_t *rp, int delta)
{
    int np = rp->page + delta;
    if (np < 0 || np >= rp->page_count) {
        if (!app_turn_epub_chapter(delta)) return;
        rp->text = app_current_text();
        rp->cover_placeholder = app_current_is_cover_placeholder();
        rp->chapter_title = app_current_chapter_title();
        rp->page_count = 0;
        paginate(rp);
        rp->page = delta > 0 ? 0 : rp->page_count - 1;
        app_set_reading_progress(rp->page, rp->total_pages);
        xr_page_invalidate(&rp->base, XR_REFRESH_QUALITY);
        return;
    }
    rp->page = np;
    app_set_reading_progress(rp->page, rp->total_pages);
    /* QUALITY: counts toward the "full refresh every N pages" budget. */
    xr_page_invalidate(&rp->base, XR_REFRESH_QUALITY);
}

static void smaller_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_REMOVE, gray);
}

static void larger_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_ADD, gray);
}

static void close_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_CLOSE, gray);
}

static void toggle_chrome(xr_page_t *p)
{
    xr_shell_set_chrome(p->shell, p, p->chrome ? XR_CHROME_NONE : XR_CHROME_ALL);
}

static bool reader_event(xr_page_t *p, const xr_event_t *ev)
{
    reader_page_t *rp = XR_CONTAINER_OF(p, reader_page_t, base);
    if (ev->type == XR_EV_TAP) {
        int third = p->area.w / 3;
        if (ev->x < p->area.x + third) turn(rp, -1);
        else if (ev->x >= p->area.x + 2 * third) turn(rp, +1);
        else toggle_chrome(p);
        return true;
    }
    switch (ev->key) {
    case XR_KEY_NEXT: case XR_KEY_RIGHT: turn(rp, +1); return true;
    case XR_KEY_DOWN:
        /* With chrome visible, Down moves into the dock action bar. */
        if (p->chrome) return false;
        turn(rp, +1);
        return true;
    case XR_KEY_PREV: case XR_KEY_LEFT:  case XR_KEY_UP:   turn(rp, -1); return true;
    case XR_KEY_MENU: case XR_KEY_OK: toggle_chrome(p); return true;
    case XR_KEY_BACK:
        if (p->chrome) { toggle_chrome(p); return true; }
        return false; /* shell pops the page */
    default: return false;
    }
}

static void set_font(reader_page_t *rp, int idx)
{
    if (idx < 0 || idx > 2 || idx == g_app.font_idx) return;
    g_app.font_idx = idx;
    /* Font changes alter every chapter's page count, so rebuild the whole-book
     * page map before updating the progress bar. */
    reader_layout(&rp->base);
    xr_page_invalidate(&rp->base, XR_REFRESH_QUALITY);
}

static void reader_action(xr_page_t *p, uint16_t id)
{
    reader_page_t *rp = XR_CONTAINER_OF(p, reader_page_t, base);
    switch (id) {
    case ACT_SMALLER: set_font(rp, g_app.font_idx - 1); break;
    case ACT_LARGER: set_font(rp, g_app.font_idx + 1); break;
    case ACT_CLOSE: xr_shell_pop(p->shell); break;
    }
}

static const xr_page_vtbl_t k_reader_vtbl = {
    .on_create = reader_create,
    .on_layout = reader_layout,
    .render = reader_render,
    .on_event = reader_event,
    .on_action = reader_action,
};

xr_page_t *app_page_reader(void)
{
    xr_page_init(&s_reader.base, &k_reader_vtbl, "Reading", XR_CHROME_NONE);
    xr_page_set_actions(&s_reader.base, k_reader_actions, XR_ARRAY_LEN(k_reader_actions));
    return &s_reader.base;
}
