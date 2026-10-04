#include "xr/xr_dialog.h"
#include "xr/xr_shell.h"

#include <string.h>

#define DIALOG_BORDER 3

void xr_dialog_init(xr_dialog_t *d, const xr_dialog_vtbl_t *vt, const char *title)
{
    memset(d, 0, sizeof(*d));
    d->vt = vt;
    d->title = title;
    xr_scope_init(&d->scope, xr_rect(0, 0, 0, 0));
}

void xr_dialog_set_icon(xr_dialog_t *d, xr_button_icon_fn icon)
{
    d->icon = icon;
}

void xr_dialog_place(xr_dialog_t *d, xr_rect_t bounds, int w, int h)
{
    w = XR_MIN(w, bounds.w);
    h = XR_MIN(h, bounds.h);
    d->rect = xr_rect(bounds.x + (bounds.w - w) / 2, bounds.y + (bounds.h - h) / 2, w, h);
    xr_scope_init(&d->scope, d->rect);
    d->scope.root.shell = d->shell;
}

int xr_dialog_header_height(const xr_dialog_t *d)
{
    if (!d->title) return 0;
    const xr_theme_t *t = d->shell->theme;
    return t->font_bold->line_height + t->pad;
}

xr_rect_t xr_dialog_dirty_rect(const xr_dialog_t *d)
{
    return xr_rect(d->rect.x, d->rect.y, d->rect.w + XR_DIALOG_SHADOW, d->rect.h + XR_DIALOG_SHADOW);
}

void xr_dialog_render_frame(xr_dialog_t *d, xr_canvas_t *c)
{
    xr_rect_t r = d->rect;
    xr_canvas_stipple_rect(c, xr_rect(r.x + XR_DIALOG_SHADOW, r.y + XR_DIALOG_SHADOW, r.w, r.h), XR_BLACK);
    xr_canvas_fill_rect(c, r, XR_WHITE);
    xr_canvas_draw_rect(c, r, DIALOG_BORDER, XR_BLACK);
    if (d->title) {
        const xr_theme_t *t = d->shell->theme;
        int hh = xr_dialog_header_height(d);
        xr_rect_t tr = xr_rect(r.x + t->pad, r.y + DIALOG_BORDER, r.w - 2 * t->pad, hh);
        if (d->icon) {
            xr_rect_t icon = xr_rect(tr.x, tr.y + (tr.h - 18) / 2, 18, 18);
            d->icon(c, icon, XR_BLACK);
            tr.x += icon.w + 8;
            tr.w -= icon.w + 8;
        }
        xr_canvas_draw_text_in(c, t->font_bold, tr, d->title, XR_ALIGN_LEFT, XR_BLACK);
        xr_canvas_hline(c, r.x + DIALOG_BORDER, r.y + DIALOG_BORDER + hh, r.w - 2 * DIALOG_BORDER, XR_BLACK);
    }
}

void xr_dialog_default_render(xr_dialog_t *d, xr_canvas_t *c)
{
    xr_dialog_render_frame(d, c);
    xr_widget_render_tree(&d->scope.root, c);
}

void xr_dialog_close(xr_dialog_t *d, int result)
{
    if (d->shell) xr_shell_close_dialog(d->shell, d, result);
}

/* ---------------------------------------------------------------- confirm */

static void confirm_clicked(xr_widget_t *w, void *user)
{
    xr_confirm_t *cd = (xr_confirm_t *)user;
    xr_dialog_close(&cd->base, (w == &cd->yes.base) ? XR_RESULT_YES : XR_RESULT_NO);
}

static void confirm_layout(xr_dialog_t *d, xr_rect_t bounds)
{
    xr_confirm_t *cd = XR_CONTAINER_OF(d, xr_confirm_t, base);
    const xr_theme_t *t = d->shell->theme;
    int pad = t->pad;
    int w = XR_MIN(bounds.w * 88 / 100, 460);
    int inner_w = w - 2 * pad;
    int msg_h = xr_text_measure_height(t->font_normal, cd->message, inner_w);
    int head = xr_dialog_header_height(d);
    int btn_h = t->row_h - 8;
    int h = head + pad + msg_h + pad + btn_h + pad;

    xr_dialog_place(d, bounds, w, h);
    xr_rect_t r = d->rect;
    int y = r.y + head + pad;

    xr_label_init(&cd->label, xr_rect(r.x + pad, y, inner_w, msg_h), cd->message,
                  t->font_normal, XR_ALIGN_LEFT);
    cd->label.wrap = true;
    y += msg_h + pad;

    int bw = (inner_w - pad) / 2;
    xr_button_init(&cd->yes, xr_rect(r.x + pad, y, bw, btn_h), cd->yes_text, t->font_bold,
                   confirm_clicked, cd);
    xr_button_init(&cd->no, xr_rect(r.x + pad + bw + pad, y, bw, btn_h), cd->no_text, t->font_bold,
                   confirm_clicked, cd);

    xr_widget_add(&d->scope.root, &cd->label.base);
    xr_widget_add(&d->scope.root, &cd->yes.base);
    xr_widget_add(&d->scope.root, &cd->no.base);
    xr_scope_set_focus(&d->scope, &cd->no.base);
}

static const xr_dialog_vtbl_t k_confirm_vtbl = { confirm_layout, NULL, NULL };

void xr_confirm_show(xr_shell_t *s, xr_confirm_t *cd, const char *title,
                     const char *message, const char *yes_text, const char *no_text,
                     xr_dialog_result_fn cb, void *user)
{
    xr_dialog_init(&cd->base, &k_confirm_vtbl, title);
    cd->message = message;
    cd->yes_text = yes_text ? yes_text : "Yes";
    cd->no_text = no_text ? no_text : "No";
    cd->base.on_result = cb;
    cd->base.user = user;
    xr_shell_show_dialog(s, &cd->base);
}
