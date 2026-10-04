#include "xr/xr_widget.h"
#include "xr/xr_shell.h"

#include <string.h>

/* --------------------------------------------------------------- base */

static void container_render(xr_widget_t *w, xr_canvas_t *c) { (void)w; (void)c; }

const xr_widget_vtbl_t xr_container_vtbl = { "container", container_render, NULL, NULL };

void xr_widget_init(xr_widget_t *w, const xr_widget_vtbl_t *vt, xr_rect_t rect)
{
    memset(w, 0, sizeof(*w));
    w->vt = vt;
    w->rect = rect;
    w->flags = XR_WF_VISIBLE;
    w->refresh_hint = XR_REFRESH_FAST;
}

void xr_widget_add(xr_widget_t *parent, xr_widget_t *child)
{
    child->parent = parent;
    child->next_sibling = NULL;
    xr_widget_t **pp = &parent->first_child;
    while (*pp) pp = &(*pp)->next_sibling;
    *pp = child;
}

void xr_widget_set_visible(xr_widget_t *w, bool visible)
{
    bool was = (w->flags & XR_WF_VISIBLE) != 0;
    if (was == visible) return;
    if (visible) w->flags |= XR_WF_VISIBLE;
    else w->flags &= (uint8_t)~XR_WF_VISIBLE;
    xr_widget_invalidate_rect(w, w->rect, XR_REFRESH_QUALITY);
}

void xr_widget_render_tree(xr_widget_t *w, xr_canvas_t *c)
{
    if (!(w->flags & XR_WF_VISIBLE)) return;
    xr_rect_t clip = xr_rect_intersect(c->clip, w->rect);
    if (xr_rect_empty(clip)) return;
    xr_rect_t old = xr_canvas_set_clip(c, clip);
    if (w->vt && w->vt->render) w->vt->render(w, c);
    for (xr_widget_t *ch = w->first_child; ch; ch = ch->next_sibling)
        xr_widget_render_tree(ch, c);
    xr_canvas_set_clip(c, old);
}

struct xr_shell *xr_widget_shell(xr_widget_t *w)
{
    while (w->parent) w = w->parent;
    return w->shell;
}

void xr_widget_invalidate_rect(xr_widget_t *w, xr_rect_t r, xr_refresh_t mode)
{
    struct xr_shell *s = xr_widget_shell(w);
    if (s) xr_shell_invalidate(s, r, mode);
}

void xr_widget_invalidate(xr_widget_t *w)
{
    xr_widget_invalidate_rect(w, w->rect, w->refresh_hint);
}

static bool can_focus(const xr_widget_t *w)
{
    return (w->flags & (XR_WF_VISIBLE | XR_WF_FOCUSABLE | XR_WF_DISABLED)) ==
           (XR_WF_VISIBLE | XR_WF_FOCUSABLE);
}

xr_widget_t *xr_widget_hit_test(xr_widget_t *root, int x, int y)
{
    if (!(root->flags & XR_WF_VISIBLE) || !xr_rect_contains(root->rect, x, y)) return NULL;
    xr_widget_t *found = NULL;
    for (xr_widget_t *ch = root->first_child; ch; ch = ch->next_sibling) {
        xr_widget_t *h = xr_widget_hit_test(ch, x, y);
        if (h) found = h; /* later siblings are on top */
    }
    if (found) return found;
    return can_focus(root) ? root : NULL;
}

/* -------------------------------------------------------------- scope */

#define XR_MAX_FOCUSABLE 32

static int collect_focusable(xr_widget_t *w, xr_widget_t **out, int n)
{
    if (!(w->flags & XR_WF_VISIBLE)) return n;
    if (can_focus(w) && n < XR_MAX_FOCUSABLE) out[n++] = w;
    for (xr_widget_t *ch = w->first_child; ch; ch = ch->next_sibling)
        n = collect_focusable(ch, out, n);
    return n;
}

void xr_scope_init(xr_scope_t *s, xr_rect_t rect)
{
    xr_widget_init(&s->root, &xr_container_vtbl, rect);
    s->focus = NULL;
}

void xr_scope_set_focus(xr_scope_t *s, xr_widget_t *w)
{
    if (s->focus == w) return;
    xr_widget_t *old = s->focus;
    s->focus = w;
    if (old) {
        old->flags &= (uint8_t)~XR_WF_FOCUSED;
        if (old->vt->on_focus) old->vt->on_focus(old, false);
        xr_widget_invalidate(old);
    }
    if (w) {
        w->flags |= XR_WF_FOCUSED;
        if (w->vt->on_focus) w->vt->on_focus(w, true);
        xr_widget_invalidate(w);
    }
}

void xr_scope_focus_first(xr_scope_t *s)
{
    xr_widget_t *list[XR_MAX_FOCUSABLE];
    int n = collect_focusable(&s->root, list, 0);
    xr_scope_set_focus(s, n ? list[0] : NULL);
}

bool xr_scope_move_focus(xr_scope_t *s, int dir)
{
    xr_widget_t *list[XR_MAX_FOCUSABLE];
    int n = collect_focusable(&s->root, list, 0);
    if (n == 0) return false;
    int cur = -1;
    for (int i = 0; i < n; i++)
        if (list[i] == s->focus) cur = i;
    int next = (cur < 0) ? (dir < 0 ? n - 1 : 0) : cur + dir;
    if (next < 0 || next >= n) return false;
    if (list[next] == s->focus) return false;
    xr_scope_set_focus(s, list[next]);
    return true;
}

bool xr_scope_handle_event(xr_scope_t *s, const xr_event_t *ev)
{
    if (ev->type == XR_EV_KEY) {
        xr_widget_t *f = s->focus;
        if (f && f->vt->on_event && f->vt->on_event(f, ev)) return true;
        switch (ev->key) {
        case XR_KEY_UP:
        case XR_KEY_LEFT:  return xr_scope_move_focus(s, -1);
        case XR_KEY_DOWN:
        case XR_KEY_RIGHT: return xr_scope_move_focus(s, +1);
        default:           return false;
        }
    }
    if (ev->type == XR_EV_TAP) {
        xr_widget_t *w = xr_widget_hit_test(&s->root, ev->x, ev->y);
        if (!w) return false;
        xr_scope_set_focus(s, w);
        if (w->vt->on_event) w->vt->on_event(w, ev);
        return true;
    }
    return false;
}

/* -------------------------------------------------------------- label */

static void label_render(xr_widget_t *w, xr_canvas_t *c)
{
    xr_label_t *l = XR_CONTAINER_OF(w, xr_label_t, base);
    if (!l->text || !l->font) return;
    if (l->wrap) xr_canvas_draw_text_wrapped(c, l->font, w->rect, l->text, l->align, l->gray);
    else         xr_canvas_draw_text_in(c, l->font, w->rect, l->text, l->align, l->gray);
}

static const xr_widget_vtbl_t k_label_vtbl = { "label", label_render, NULL, NULL };

void xr_label_init(xr_label_t *l, xr_rect_t r, const char *text,
                   const xr_font_t *font, xr_align_t align)
{
    xr_widget_init(&l->base, &k_label_vtbl, r);
    l->base.refresh_hint = XR_REFRESH_QUALITY;
    l->text = text;
    l->font = font;
    l->align = align;
    l->gray = XR_BLACK;
    l->wrap = false;
}

void xr_label_set_text(xr_label_t *l, const char *text)
{
    l->text = text;
    xr_widget_invalidate(&l->base);
}

/* ------------------------------------------------------------- button */

static void button_render(xr_widget_t *w, xr_canvas_t *c)
{
    xr_button_t *b = XR_CONTAINER_OF(w, xr_button_t, base);
    bool focused = xr_widget_has_focus(w);
    bool disabled = (w->flags & XR_WF_DISABLED) != 0;
    if (focused) {
        /* Inverted = the clearest focus cue on a 2-level panel. */
        xr_canvas_fill_rect(c, w->rect, XR_BLACK);
    } else {
        xr_canvas_fill_rect(c, w->rect, XR_WHITE);
        xr_canvas_draw_rect(c, w->rect, 2, disabled ? XR_LIGHT : XR_BLACK);
    }
    uint8_t fg = focused ? XR_WHITE : (disabled ? XR_LIGHT : XR_BLACK);
    xr_rect_t text = xr_rect_inset(w->rect, 8);
    xr_align_t align = XR_ALIGN_CENTER;
    if (b->icon) {
        xr_rect_t icon = xr_rect(text.x, text.y + (text.h - 24) / 2, 24, 24);
        b->icon(c, icon, fg);
        text.x = icon.x + icon.w + 8;
        text.w = w->rect.x + w->rect.w - text.x - 8;
        align = XR_ALIGN_LEFT;
    }
    xr_canvas_draw_text_in(c, b->font, text, b->text, align, fg);
}

static bool button_event(xr_widget_t *w, const xr_event_t *ev)
{
    bool activate = (ev->type == XR_EV_TAP) || (ev->type == XR_EV_KEY && ev->key == XR_KEY_OK);
    if (!activate) return false;
    if (w->on_activate) w->on_activate(w, w->user);
    return true;
}

static const xr_widget_vtbl_t k_button_vtbl = { "button", button_render, button_event, NULL };

void xr_button_init(xr_button_t *b, xr_rect_t r, const char *text,
                    const xr_font_t *font, xr_activate_fn fn, void *user)
{
    xr_widget_init(&b->base, &k_button_vtbl, r);
    b->base.flags |= XR_WF_FOCUSABLE;
    b->base.on_activate = fn;
    b->base.user = user;
    b->text = text;
    b->font = font;
    b->icon = NULL;
}

void xr_button_set_icon(xr_button_t *b, xr_button_icon_fn icon)
{
    b->icon = icon;
    xr_widget_invalidate(&b->base);
}

/* --------------------------------------------------------------- list */

#define LIST_PAD 14
#define SCROLLBAR_W 6

int xr_list_visible_rows(const xr_list_t *l)
{
    int n = l->base.rect.h / l->row_h;
    return n < 1 ? 1 : n;
}

static xr_rect_t row_rect(const xr_list_t *l, int i)
{
    const xr_rect_t r = l->base.rect;
    int w = r.w - ((l->count > xr_list_visible_rows(l)) ? SCROLLBAR_W + 4 : 0);
    return xr_rect(r.x, r.y + (i - l->top) * l->row_h, w, l->row_h);
}

static void list_render(xr_widget_t *w, xr_canvas_t *c)
{
    xr_list_t *l = XR_CONTAINER_OF(w, xr_list_t, base);
    int vis = xr_list_visible_rows(l);
    bool focused = xr_widget_has_focus(w);

    for (int i = l->top; i < l->count && i < l->top + vis; i++) {
        xr_rect_t rr = row_rect(l, i);
        if (!xr_rect_intersects(rr, c->clip)) continue;

        const char *p = "", *s = NULL;
        l->get_row(l, i, &p, &s);
        bool sel = (i == l->selected);
        bool inverted = sel && focused;
        uint8_t fg = inverted ? XR_WHITE : XR_BLACK;
        uint8_t fg2 = inverted ? XR_WHITE : XR_DARK;

        xr_canvas_fill_rect(c, rr, inverted ? XR_BLACK : XR_WHITE);
        if (sel && !focused) xr_canvas_fill_rect(c, xr_rect(rr.x, rr.y + 6, 5, rr.h - 12), XR_BLACK);
        if (!inverted) xr_canvas_hline(c, rr.x, rr.y + rr.h - 1, rr.w, XR_LIGHT);

        xr_rect_t inner = xr_rect(rr.x + LIST_PAD, rr.y, rr.w - 2 * LIST_PAD, rr.h);
        if (l->style == XR_LIST_TWO_LINE && s) {
            int h1 = l->font->line_height, h2 = l->font_secondary->line_height;
            int y = rr.y + (rr.h - h1 - h2) / 2;
            xr_canvas_draw_text_in(c, l->font, xr_rect(inner.x, y, inner.w, h1), p, XR_ALIGN_LEFT, fg);
            xr_canvas_draw_text_in(c, l->font_secondary, xr_rect(inner.x, y + h1, inner.w, h2), s,
                                   XR_ALIGN_LEFT, fg2);
        } else {
            int sw = s ? xr_text_width(l->font_secondary, s, -1) + LIST_PAD : 0;
            xr_canvas_draw_text_in(c, l->font, xr_rect(inner.x, inner.y, inner.w - sw, inner.h), p,
                                   XR_ALIGN_LEFT, fg);
            if (s) xr_canvas_draw_text_in(c, l->font_secondary, inner, s, XR_ALIGN_RIGHT, fg2);
        }
    }

    if (l->count > vis) { /* scrollbar: track + thumb */
        xr_rect_t r = w->rect;
        xr_rect_t track = xr_rect(r.x + r.w - SCROLLBAR_W, r.y, SCROLLBAR_W, vis * l->row_h);
        xr_canvas_fill_rect(c, track, XR_WHITE);
        xr_canvas_vline(c, track.x + SCROLLBAR_W / 2, track.y, track.h, XR_LIGHT);
        int th = XR_MAX(track.h * vis / l->count, 16);
        int ty = track.y + (track.h - th) * l->top / XR_MAX(l->count - vis, 1);
        xr_canvas_fill_rect(c, xr_rect(track.x, ty, SCROLLBAR_W, th), XR_BLACK);
    }
}

void xr_list_invalidate_row(xr_list_t *l, int index, xr_refresh_t mode)
{
    if (index < l->top || index >= l->top + xr_list_visible_rows(l)) return;
    xr_widget_invalidate_rect(&l->base, row_rect(l, index), mode);
}

void xr_list_select(xr_list_t *l, int index)
{
    if (l->count == 0) { l->selected = 0; return; }
    if (index < 0) index = 0;
    if (index >= l->count) index = l->count - 1;
    if (index == l->selected) return;

    int vis = xr_list_visible_rows(l);
    int old = l->selected;
    l->selected = index;
    if (index < l->top || index >= l->top + vis) {
        /* Scrolled: show a whole new page of rows, paginated not smooth. */
        l->top = (index / vis) * vis;
        xr_widget_invalidate_rect(&l->base, l->base.rect, XR_REFRESH_QUALITY);
    } else {
        /* Only two rows changed: fast partial update, no flash. */
        xr_list_invalidate_row(l, old, XR_REFRESH_FAST);
        xr_list_invalidate_row(l, index, XR_REFRESH_FAST);
    }
}

void xr_list_set_count(xr_list_t *l, int count)
{
    l->count = count;
    if (l->selected >= count) l->selected = count > 0 ? count - 1 : 0;
    int vis = xr_list_visible_rows(l);
    l->top = (l->selected / vis) * vis;
    xr_widget_invalidate_rect(&l->base, l->base.rect, XR_REFRESH_QUALITY);
}

static bool list_event(xr_widget_t *w, const xr_event_t *ev)
{
    xr_list_t *l = XR_CONTAINER_OF(w, xr_list_t, base);
    int vis = xr_list_visible_rows(l);
    if (ev->type == XR_EV_TAP) {
        int i = l->top + (ev->y - w->rect.y) / l->row_h;
        if (i >= l->count) return true;
        xr_list_select(l, i);
        if (l->on_select) l->on_select(l, i);
        return true;
    }
    switch (ev->key) {
    case XR_KEY_UP:
        if (l->selected == 0) return false; /* let focus leave the list */
        xr_list_select(l, l->selected - 1);
        return true;
    case XR_KEY_DOWN:
        if (l->selected >= l->count - 1) return false;
        xr_list_select(l, l->selected + 1);
        return true;
    case XR_KEY_NEXT: xr_list_select(l, XR_MIN(l->top + vis, l->count - 1)); return true;
    case XR_KEY_PREV: xr_list_select(l, XR_MAX(l->top - vis, 0)); return true;
    case XR_KEY_OK:
        if (l->count && l->on_select) l->on_select(l, l->selected);
        return true;
    default:
        return false;
    }
}

static const xr_widget_vtbl_t k_list_vtbl = { "list", list_render, list_event, NULL };

void xr_list_init(xr_list_t *l, xr_rect_t r, xr_list_style_t style, int16_t row_h,
                  const xr_font_t *font, const xr_font_t *font_secondary,
                  xr_list_row_fn get_row, void *user)
{
    xr_widget_init(&l->base, &k_list_vtbl, r);
    l->base.flags |= XR_WF_FOCUSABLE;
    l->base.refresh_hint = XR_REFRESH_FAST;
    l->style = style;
    l->count = l->selected = l->top = 0;
    l->row_h = row_h;
    l->font = font;
    l->font_secondary = font_secondary;
    l->get_row = get_row;
    l->on_select = NULL;
    l->user = user;
}
