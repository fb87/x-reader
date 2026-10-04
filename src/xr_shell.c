#include "xr/xr_shell.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ setup */

void xr_shell_init(xr_shell_t *s, xr_display_t *display, const xr_platform_t *platform,
                   const xr_theme_t *theme)
{
    memset(s, 0, sizeof(*s));
    s->display = display;
    s->platform = platform;
    s->theme = theme;
    xr_canvas_init(&s->canvas, display->framebuffer, display->width, display->height,
                   display->stride, display->fmt);
    s->screen = xr_rect(0, 0, display->width, display->height);
    s->status_rect = xr_rect(0, 0, display->width, theme->status_h);
    s->dock_rect = xr_rect(0, display->height - theme->dock_h, display->width, theme->dock_h);
    xr_refresh_init(&s->sched, s->screen, display->update_align);
    s->battery = -1;
    s->dock_focus = -1;
}

void xr_shell_set_background(xr_shell_t *s, xr_background_fn fn, void *user)
{
    s->background = fn;
    s->background_user = user;
    xr_shell_invalidate(s, s->screen, XR_REFRESH_QUALITY);
}

void xr_shell_set_full_refresh_every(xr_shell_t *s, uint16_t n)
{
    s->sched.full_every = n;
}

uint32_t xr_shell_now(const xr_shell_t *s)
{
    return s->platform->ops->now_ms(s->platform->ctx);
}

xr_rect_t xr_shell_page_area(const xr_shell_t *s, uint8_t chrome)
{
    int y0 = (chrome & XR_CHROME_STATUS) ? s->status_rect.h : 0;
    int y1 = (chrome & XR_CHROME_DOCK) ? s->dock_rect.y : s->screen.h;
    return xr_rect(0, y0, s->screen.w, y1 - y0);
}

void xr_shell_invalidate(xr_shell_t *s, xr_rect_t r, xr_refresh_t mode)
{
    xr_refresh_invalidate(&s->sched, r, mode);
}

/* ------------------------------------------------------------- navigation */

xr_page_t *xr_shell_top(const xr_shell_t *s)
{
    return s->page_count ? s->pages[s->page_count - 1] : NULL;
}

static void attach_page(xr_shell_t *s, xr_page_t *p)
{
    p->shell = s;
    p->area = xr_shell_page_area(s, p->chrome);
    xr_scope_init(&p->scope, p->area);
    p->scope.root.shell = s;
}

static void drop_dialogs(xr_shell_t *s)
{
    /* Dialogs belong to the screen that opened them. */
    s->dialog_count = 0;
}

static void enter_page(xr_shell_t *s, xr_page_t *p, bool created)
{
    s->dock_focus = -1;
    if (created) {
        attach_page(s, p);
        if (p->vt->on_create) p->vt->on_create(p);
        if (!p->scope.focus) xr_scope_focus_first(&p->scope);
    }
    if (p->vt->on_enter) p->vt->on_enter(p);
    xr_shell_invalidate(s, s->screen, p->enter_refresh);
}

static void leave_page(xr_page_t *p, bool destroy)
{
    if (p->vt->on_exit) p->vt->on_exit(p);
    if (destroy && p->vt->on_destroy) p->vt->on_destroy(p);
}

void xr_shell_push(xr_shell_t *s, xr_page_t *p)
{
    if (s->page_count >= XR_SHELL_MAX_PAGES) return;
    drop_dialogs(s);
    xr_page_t *top = xr_shell_top(s);
    if (top) leave_page(top, false);
    s->pages[s->page_count++] = p;
    enter_page(s, p, true);
}

void xr_shell_pop(xr_shell_t *s)
{
    if (s->page_count <= 1) return;
    drop_dialogs(s);
    leave_page(s->pages[--s->page_count], true);
    enter_page(s, xr_shell_top(s), false);
}

void xr_shell_replace(xr_shell_t *s, xr_page_t *p)
{
    if (s->page_count == 0) { xr_shell_push(s, p); return; }
    drop_dialogs(s);
    leave_page(s->pages[s->page_count - 1], true);
    s->pages[s->page_count - 1] = p;
    enter_page(s, p, true);
}

void xr_shell_set_chrome(xr_shell_t *s, xr_page_t *p, uint8_t chrome)
{
    if (p->chrome == chrome) return;
    p->chrome = chrome;
    if (xr_shell_top(s) != p) return;
    if (!(chrome & XR_CHROME_DOCK)) s->dock_focus = -1;
    p->area = xr_shell_page_area(s, chrome);
    p->scope.root.rect = p->area;
    if (p->vt->on_layout) p->vt->on_layout(p);
    xr_shell_invalidate(s, s->screen, XR_REFRESH_QUALITY);
}

/* ---------------------------------------------------------------- dialogs */

void xr_shell_show_dialog(xr_shell_t *s, xr_dialog_t *d)
{
    if (s->dialog_count >= XR_SHELL_MAX_DIALOGS) return;
    d->shell = s;
    xr_rect_t bounds = xr_rect_inset(s->screen, s->theme->pad);
    d->vt->layout(d, bounds);
    if (!d->scope.focus) xr_scope_focus_first(&d->scope);
    s->dialogs[s->dialog_count++] = d;
    xr_shell_invalidate(s, xr_dialog_dirty_rect(d), XR_REFRESH_QUALITY);
}

void xr_shell_close_dialog(xr_shell_t *s, xr_dialog_t *d, int result)
{
    int idx = -1;
    for (int i = 0; i < s->dialog_count; i++)
        if (s->dialogs[i] == d) idx = i;
    if (idx < 0) return;
    for (int i = idx; i < s->dialog_count - 1; i++) s->dialogs[i] = s->dialogs[i + 1];
    s->dialog_count--;
    /* What was underneath is recomposed from the lower layers. */
    xr_shell_invalidate(s, xr_dialog_dirty_rect(d), XR_REFRESH_QUALITY);
    /* Callback last: it may navigate or open another dialog. */
    if (d->on_result) d->on_result(d, result, d->user);
}

/* ------------------------------------------------------------------ input */

static int dock_action_at(const xr_shell_t *s, const xr_page_t *p, int x)
{
    if (!p->action_count) return -1;
    int i = x * p->action_count / s->screen.w;
    return (i >= 0 && i < p->action_count) ? i : -1;
}

static void fire_action(xr_page_t *p, int i)
{
    if (p->vt->on_action) p->vt->on_action(p, p->actions[i].id);
}

static void set_dock_focus(xr_shell_t *s, int focus)
{
    if (s->dock_focus == focus) return;
    s->dock_focus = (int8_t)focus;
    xr_shell_invalidate(s, s->dock_rect, XR_REFRESH_FAST);
}

void xr_shell_dispatch(xr_shell_t *s, const xr_event_t *ev)
{
    /* Layer 2: the top dialog is modal and takes everything. */
    if (s->dialog_count) {
        xr_dialog_t *d = s->dialogs[s->dialog_count - 1];
        if (ev->type == XR_EV_TAP && !xr_rect_contains(d->rect, ev->x, ev->y)) {
            xr_shell_close_dialog(s, d, XR_RESULT_CANCEL);
            return;
        }
        if (d->vt->on_event && d->vt->on_event(d, ev)) return;
        if (xr_scope_handle_event(&d->scope, ev)) return;
        if (ev->type == XR_EV_KEY && ev->key == XR_KEY_BACK)
            xr_shell_close_dialog(s, d, XR_RESULT_CANCEL);
        return;
    }

    xr_page_t *p = xr_shell_top(s);
    if (!p) return;

    if (ev->type == XR_EV_KEY && ev->long_press) {
        /* Rotary holds move directly between long content and the dock. */
        if (ev->key == XR_KEY_DOWN && (p->chrome & XR_CHROME_DOCK) && p->action_count) {
            set_dock_focus(s, 0);
            return;
        }
        if (ev->key == XR_KEY_UP && s->dock_focus >= 0) {
            set_dock_focus(s, -1);
            return;
        }
    }

    if (ev->type == XR_EV_KEY && s->dock_focus >= 0) {
        int n = p->action_count;
        if (!(p->chrome & XR_CHROME_DOCK) || s->dock_focus >= n) {
            set_dock_focus(s, -1);
        } else {
            switch (ev->key) {
            case XR_KEY_DOWN:
                set_dock_focus(s, (s->dock_focus + 1) % n);
                return;
            case XR_KEY_UP:
                if (s->dock_focus == 0) set_dock_focus(s, -1);
                else set_dock_focus(s, s->dock_focus - 1);
                return;
            case XR_KEY_OK:
                fire_action(p, s->dock_focus);
                return;
            default:
                break;
            }
        }
    }

    /* Shell chrome. */
    if (ev->type == XR_EV_TAP) {
        if ((p->chrome & XR_CHROME_DOCK) && xr_rect_contains(s->dock_rect, ev->x, ev->y)) {
            int i = dock_action_at(s, p, ev->x);
            if (i >= 0) fire_action(p, i);
            return;
        }
        if ((p->chrome & XR_CHROME_STATUS) && xr_rect_contains(s->status_rect, ev->x, ev->y))
            return;
    }

    /* Layer 1: the page, then its focused widget / focus navigation. */
    if (p->vt->on_event && p->vt->on_event(p, ev)) return;
    if (xr_scope_handle_event(&p->scope, ev)) return;

    if (ev->type == XR_EV_KEY) {
        if (ev->key == XR_KEY_DOWN && (p->chrome & XR_CHROME_DOCK) && p->action_count) {
            set_dock_focus(s, 0);
            return;
        }
        for (int i = 0; i < p->action_count; i++) {
            if (p->actions[i].key != XR_KEY_NONE && p->actions[i].key == ev->key) {
                fire_action(p, i);
                return;
            }
        }
        if (ev->key == XR_KEY_BACK) xr_shell_pop(s);
    }
}

/* ------------------------------------------------------------------- time */

void xr_shell_tick(xr_shell_t *s)
{
    xr_page_t *p = xr_shell_top(s);
    if (p && p->vt->on_tick) p->vt->on_tick(p, xr_shell_now(s));

    /* Status bar: redraw only when the minute or battery actually changed. */
    const xr_platform_ops_t *ops = s->platform->ops;
    int hh = 0, mm = 0;
    ops->wall_time(s->platform->ctx, &hh, &mm);
    char clock[8];
    snprintf(clock, sizeof clock, "%02d:%02d", hh % 24, mm % 60);
    int batt = ops->battery_percent ? ops->battery_percent(s->platform->ctx) : -1;
    if (strcmp(clock, s->clock) != 0 || batt != s->battery) {
        memcpy(s->clock, clock, sizeof clock);
        s->battery = batt;
        p = xr_shell_top(s);
        if (p && (p->chrome & XR_CHROME_STATUS))
            xr_shell_invalidate(s, s->status_rect, XR_REFRESH_FAST);
    }
}

/* -------------------------------------------------------------- rendering */

static void render_status(xr_shell_t *s, xr_canvas_t *c, const xr_page_t *p)
{
    const xr_theme_t *t = s->theme;
    xr_rect_t r = s->status_rect;
    xr_canvas_fill_rect(c, r, XR_WHITE);
    xr_canvas_fill_rect(c, xr_rect(r.x, r.y + r.h - 2, r.w, 2), XR_BLACK);

    xr_rect_t inner = xr_rect(r.x + t->pad, r.y, r.w - 2 * t->pad, r.h - 2);
    int x = inner.x + inner.w;

    if (s->battery >= 0) { /* battery icon + percent, right aligned */
        int bw = 26, bh = 13;
        int by = inner.y + (inner.h - bh) / 2;
        x -= 3;
        xr_canvas_fill_rect(c, xr_rect(x, by + 4, 3, bh - 8), XR_BLACK);
        x -= bw;
        xr_canvas_draw_rect(c, xr_rect(x, by, bw, bh), 2, XR_BLACK);
        int fill = (bw - 6) * s->battery / 100;
        xr_canvas_fill_rect(c, xr_rect(x + 3, by + 3, fill, bh - 6), XR_BLACK);
        char pct[8];
        snprintf(pct, sizeof pct, "%d%%", s->battery);
        int pw = xr_text_width(t->font_small, pct, -1);
        x -= pw + 6;
        xr_canvas_draw_text_in(c, t->font_small, xr_rect(x, inner.y, pw, inner.h), pct, XR_ALIGN_LEFT, XR_BLACK);
    }
    int cw = xr_text_width(t->font_bold, s->clock, -1);
    x -= cw + t->pad;
    xr_canvas_draw_text_in(c, t->font_bold, xr_rect(x, inner.y, cw, inner.h), s->clock, XR_ALIGN_LEFT, XR_BLACK);

    if (p->title)
        xr_canvas_draw_text_in(c, t->font_bold, xr_rect(inner.x, inner.y, x - inner.x - t->pad, inner.h),
                               p->title, XR_ALIGN_LEFT, XR_BLACK);
}

static void render_dock(xr_shell_t *s, xr_canvas_t *c, const xr_page_t *p)
{
    const xr_theme_t *t = s->theme;
    xr_rect_t r = s->dock_rect;
    xr_canvas_fill_rect(c, r, XR_WHITE);
    xr_canvas_fill_rect(c, xr_rect(r.x, r.y, r.w, 2), XR_BLACK);
    int n = p->action_count;
    for (int i = 0; i < n; i++) {
        int x0 = r.w * i / n, x1 = r.w * (i + 1) / n;
        bool focused = s->dock_focus == i;
        if (focused) xr_canvas_fill_rect(c, xr_rect(x0, r.y + 2, x1 - x0, r.h - 2), XR_BLACK);
        if (i > 0) xr_canvas_vline(c, x0, r.y + 14, r.h - 28, XR_LIGHT);
        uint8_t fg = focused ? XR_WHITE : XR_BLACK;
        xr_rect_t label = xr_rect(x0 + 4, r.y + 2, x1 - x0 - 8, r.h - 2);
        if (p->actions[i].icon) {
            xr_rect_t icon = xr_rect(x0 + (x1 - x0 - 18) / 2, r.y + 5, 18, 18);
            p->actions[i].icon(c, icon, fg);
            label = xr_rect(x0 + 4, r.y + 27, x1 - x0 - 8, r.h - 29);
        }
        xr_canvas_draw_text_in(c, t->font_normal, label, p->actions[i].label, XR_ALIGN_CENTER, fg);
    }
}

static void compose(xr_shell_t *s, xr_rect_t clip)
{
    xr_canvas_t *c = &s->canvas;
    xr_rect_t saved = xr_canvas_set_clip(c, clip);

    /* Layer 0: background */
    if (s->background) s->background(s, c, s->background_user);
    else xr_canvas_fill_rect(c, clip, XR_WHITE);

    /* Layer 1: content = chrome + top page (pages below are fully covered) */
    xr_page_t *p = xr_shell_top(s);
    if (p) {
        if ((p->chrome & XR_CHROME_STATUS) && xr_rect_intersects(clip, s->status_rect)) {
            xr_canvas_set_clip(c, xr_rect_intersect(clip, s->status_rect));
            render_status(s, c, p);
        }
        if (xr_rect_intersects(clip, p->area)) {
            xr_canvas_set_clip(c, xr_rect_intersect(clip, p->area));
            if (p->vt->render) p->vt->render(p, c);
            else xr_widget_render_tree(&p->scope.root, c);
        }
        if ((p->chrome & XR_CHROME_DOCK) && xr_rect_intersects(clip, s->dock_rect)) {
            xr_canvas_set_clip(c, xr_rect_intersect(clip, s->dock_rect));
            render_dock(s, c, p);
        }
    }

    /* Layer 2: dialogs, bottom to top */
    for (int i = 0; i < s->dialog_count; i++) {
        xr_dialog_t *d = s->dialogs[i];
        xr_rect_t dr = xr_dialog_dirty_rect(d);
        if (!xr_rect_intersects(clip, dr)) continue;
        xr_canvas_set_clip(c, xr_rect_intersect(clip, dr));
        if (d->vt->render) d->vt->render(d, c);
        else xr_dialog_default_render(d, c);
    }

    xr_canvas_set_clip(c, saved);
}

void xr_shell_flush(xr_shell_t *s)
{
    xr_dirty_t d;
    while (xr_refresh_take(&s->sched, &d)) {
        compose(s, d.rect);
        s->display->ops->update(s->display, d.rect, d.mode);
    }
    if (s->display->ops->wait_idle) s->display->ops->wait_idle(s->display);
}
