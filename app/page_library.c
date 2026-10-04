/* Library: a paginated list of books with dock actions. */
#include "app_internal.h"

#include <stdio.h>

enum { ACT_FAVORITE = 1, ACT_DELETE, ACT_BACK };

#define LIBRARY_INDEX_W 28
#define LIBRARY_COVER_W 42
#define LIBRARY_COVER_H 58

static void favorite_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
static void delete_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
static void back_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);

static const xr_action_t k_library_actions[] = {
    { ACT_FAVORITE, "Favorite", XR_KEY_NONE, favorite_icon },
    { ACT_DELETE, "Delete", XR_KEY_NONE, delete_icon },
    { ACT_BACK, "Back", XR_KEY_NONE, back_icon },
};

static const xr_action_t k_favorites_actions[] = {
    { ACT_FAVORITE, "Remove", XR_KEY_NONE, favorite_icon },
    { ACT_DELETE, "Delete", XR_KEY_NONE, delete_icon },
    { ACT_BACK, "Back", XR_KEY_NONE, back_icon },
};

typedef struct library_page {
    xr_page_t base;
    xr_list_t list;
    char title[32];
    char secondary[64];
    char confirm_msg[96];
    int book_index[APP_MAX_BOOKS];
    int pending_book;
    bool favorites_only;
} library_page_t;

static library_page_t s_library;
static library_page_t s_favorites;

static void library_row(xr_list_t *l, int i, const char **primary, const char **secondary)
{
    library_page_t *lp = (library_page_t *)l->user;
    const app_book_t *b = &g_app.books[lp->book_index[i]];
    *primary = b->title;
    if (b->progress == 0)
        snprintf(lp->secondary, sizeof lp->secondary, "%s  |  New", b->author);
    else if (b->progress >= 100)
        snprintf(lp->secondary, sizeof lp->secondary, "%s  |  Finished", b->author);
    else
        snprintf(lp->secondary, sizeof lp->secondary, "%s  |  %d%%", b->author, b->progress);
    *secondary = lp->secondary; /* rows are drawn one at a time */
}

static void draw_cover(xr_canvas_t *c, xr_rect_t r, const app_book_t *book, int index)
{
    char initial[2] = { book->title[0], '\0' };
    uint8_t band = (index % 3 == 0) ? XR_BLACK : XR_DARK;
    xr_canvas_fill_rect(c, r, XR_LIGHT);
    xr_canvas_draw_rect(c, r, 2, XR_BLACK);
    xr_canvas_fill_rect(c, xr_rect(r.x + 3, r.y + 3, r.w - 6, 11), band);
    xr_canvas_draw_text_in(c, &xr_font_alegreya_bold_26, xr_rect(r.x + 2, r.y + 16, r.w - 4, r.h - 18),
                           initial, XR_ALIGN_CENTER, XR_BLACK);
}

static void favorite_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_BOOK, gray);
}

static void delete_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_DELETE, gray);
}

static void back_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    app_draw_icon(c, r, XR_ICON_ARROW_BACK, gray);
}

static void library_render(xr_page_t *p, xr_canvas_t *c)
{
    library_page_t *lp = XR_CONTAINER_OF(p, library_page_t, base);
    xr_list_t *list = &lp->list;
    int visible = xr_list_visible_rows(list);
    bool focused = xr_widget_has_focus(&list->base);
    int row_w = list->base.rect.w - ((list->count > visible) ? 10 : 0);

    for (int i = list->top; i < list->count && i < list->top + visible; i++) {
        xr_rect_t row = xr_rect(list->base.rect.x, list->base.rect.y + (i - list->top) * list->row_h,
                                row_w, list->row_h);
        if (!xr_rect_intersects(row, c->clip)) continue;

        bool selected = i == list->selected;
        bool inverted = selected && focused;
        uint8_t fg = inverted ? XR_WHITE : XR_BLACK;
        uint8_t secondary_fg = inverted ? XR_WHITE : XR_DARK;
        char number[12];
        const char *title, *author;
        library_row(list, i, &title, &author);
        snprintf(number, sizeof number, "%d", i + 1);

        xr_canvas_fill_rect(c, row, inverted ? XR_BLACK : XR_WHITE);
        if (selected && !focused)
            xr_canvas_fill_rect(c, xr_rect(row.x, row.y + 6, 5, row.h - 12), XR_BLACK);
        if (!inverted) xr_canvas_hline(c, row.x, row.y + row.h - 1, row.w, XR_LIGHT);

        xr_rect_t index_rect = xr_rect(row.x + 2, row.y, LIBRARY_INDEX_W, row.h);
        xr_rect_t cover = xr_rect(index_rect.x + index_rect.w + 6, row.y + (row.h - LIBRARY_COVER_H) / 2,
                                  LIBRARY_COVER_W, LIBRARY_COVER_H);
        int text_x = cover.x + cover.w + 10;
        xr_rect_t text = xr_rect(text_x, row.y, row.x + row.w - text_x - 8, row.h);
        int title_h = p->shell->theme->font_normal->line_height;
        int secondary_h = p->shell->theme->font_small->line_height;
        int text_y = row.y + (row.h - title_h - secondary_h) / 2;

        xr_canvas_draw_text_in(c, p->shell->theme->font_small, index_rect, number, XR_ALIGN_RIGHT, fg);
        draw_cover(c, cover, &g_app.books[lp->book_index[i]], lp->book_index[i]);
        xr_canvas_draw_text_in(c, p->shell->theme->font_normal,
                               xr_rect(text.x, text_y, text.w, title_h), title, XR_ALIGN_LEFT, fg);
        xr_canvas_draw_text_in(c, p->shell->theme->font_small,
                               xr_rect(text.x, text_y + title_h, text.w, secondary_h), author,
                               XR_ALIGN_LEFT, secondary_fg);
    }

    if (list->count > visible) {
        xr_rect_t track = xr_rect(list->base.rect.x + list->base.rect.w - 6, list->base.rect.y,
                                  6, visible * list->row_h);
        xr_canvas_fill_rect(c, track, XR_WHITE);
        xr_canvas_vline(c, track.x + 3, track.y, track.h, XR_LIGHT);
        int thumb_h = XR_MAX(track.h * visible / list->count, 16);
        int thumb_y = track.y + (track.h - thumb_h) * list->top / XR_MAX(list->count - visible, 1);
        xr_canvas_fill_rect(c, xr_rect(track.x, thumb_y, 6, thumb_h), XR_BLACK);
    }
}

static void update_title(library_page_t *lp)
{
    snprintf(lp->title, sizeof lp->title, "%s (%d)", lp->favorites_only ? "Favorites" : "Library",
             lp->list.count);
    xr_page_set_title(&lp->base, lp->title);
}

static void library_sync(library_page_t *lp)
{
    int n = 0;
    for (int i = 0; i < g_app.book_count; i++)
        if (!g_app.books[i].transient && (!lp->favorites_only || g_app.books[i].favorite))
            lp->book_index[n++] = i;
    xr_list_set_count(&lp->list, n);
    update_title(lp);
}

static int selected_book(const library_page_t *lp)
{
    return (lp->list.selected >= 0 && lp->list.selected < lp->list.count)
        ? lp->book_index[lp->list.selected] : -1;
}

static void library_selected(xr_list_t *l, int index)
{
    library_page_t *lp = (library_page_t *)l->user;
    if (index >= 0 && index < l->count) app_show_book_info(lp->book_index[index]);
}

static void library_create(xr_page_t *p)
{
    library_page_t *lp = XR_CONTAINER_OF(p, library_page_t, base);
    const xr_theme_t *t = p->shell->theme;
    xr_rect_t a = p->area;
    xr_rect_t lr = xr_rect(a.x + 4, a.y + 4, a.w - 12, a.h - 8);
    xr_list_init(&lp->list, lr, XR_LIST_TWO_LINE, t->row_h, t->font_normal, t->font_small,
                 library_row, lp);
    lp->list.on_select = library_selected;
    library_sync(lp);
    xr_page_add(p, &lp->list.base);
    update_title(lp);
}

static void delete_result(xr_dialog_t *d, int result, void *user)
{
    (void)d;
    library_page_t *lp = (library_page_t *)user;
    if (result != XR_RESULT_YES) return;
    app_delete_book(lp->pending_book);
    library_sync(lp);
}

static void library_action(xr_page_t *p, uint16_t id)
{
    library_page_t *lp = XR_CONTAINER_OF(p, library_page_t, base);
    int book = selected_book(lp);
    if (book < 0 && id != ACT_BACK) return;
    switch (id) {
    case ACT_FAVORITE:
        g_app.books[book].favorite = !lp->favorites_only;
        if (lp->favorites_only) library_sync(lp);
        else xr_list_invalidate_row(&lp->list, lp->list.selected, XR_REFRESH_QUALITY);
        break;
    case ACT_DELETE:
        snprintf(lp->confirm_msg, sizeof lp->confirm_msg, "Delete \"%s\" from the device?",
                 g_app.books[book].title);
        lp->pending_book = book;
        app_confirm("Delete book", lp->confirm_msg, "Delete", delete_icon, delete_result, lp);
        break;
    case ACT_BACK:
        xr_shell_pop(p->shell);
        break;
    }
}

static const xr_page_vtbl_t k_library_vtbl = {
    .on_create = library_create,
    .render = library_render,
    .on_action = library_action,
};

xr_page_t *app_page_library(void)
{
    xr_page_init(&s_library.base, &k_library_vtbl, "Library", XR_CHROME_ALL);
    s_library.favorites_only = false;
    xr_page_set_actions(&s_library.base, k_library_actions, XR_ARRAY_LEN(k_library_actions));
    return &s_library.base;
}

xr_page_t *app_page_favorites(void)
{
    xr_page_init(&s_favorites.base, &k_library_vtbl, "Favorites", XR_CHROME_ALL);
    s_favorites.favorites_only = true;
    xr_page_set_actions(&s_favorites.base, k_favorites_actions, XR_ARRAY_LEN(k_favorites_actions));
    return &s_favorites.base;
}
