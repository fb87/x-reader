/* Splash / boot screen: fullscreen, no chrome, replaced by Home. */
#include "app_internal.h"

#define SPLASH_MS 1500

typedef struct splash_page {
    xr_page_t base;
    uint32_t entered_ms;
    xr_label_t name, tagline, status;
} splash_page_t;

static splash_page_t s_splash;

static void splash_create(xr_page_t *p)
{
    splash_page_t *sp = XR_CONTAINER_OF(p, splash_page_t, base);
    const xr_theme_t *t = p->shell->theme;
    xr_rect_t a = p->area;
    int cy = a.y + a.h / 2;

    xr_label_init(&sp->name, xr_rect(a.x, cy + 20, a.w, 40), "X-Reader", t->font_title, XR_ALIGN_CENTER);
    xr_label_init(&sp->tagline, xr_rect(a.x, cy + 62, a.w, 28), "the ebook reader for e-ink",
                  t->font_normal, XR_ALIGN_CENTER);
    sp->tagline.gray = XR_DARK;
    xr_label_init(&sp->status, xr_rect(a.x, a.y + a.h - 70, a.w, 24), "Loading library...",
                  t->font_small, XR_ALIGN_CENTER);
    sp->status.gray = XR_DARK;
    xr_page_add(p, &sp->name.base);
    xr_page_add(p, &sp->tagline.base);
    xr_page_add(p, &sp->status.base);
}

/* Custom render: an open-book mark above the labels, then the widget tree. */
static void splash_render(xr_page_t *p, xr_canvas_t *c)
{
    xr_rect_t a = p->area;
    int cx = a.x + a.w / 2, top = a.y + a.h / 2 - 110;
    int pw = 64, ph = 96;
    xr_canvas_fill_rect(c, xr_rect(cx - pw - 4, top, pw, ph), XR_BLACK);
    xr_canvas_fill_rect(c, xr_rect(cx + 4, top, pw, ph), XR_BLACK);
    for (int i = 0; i < 5; i++) { /* text lines on the pages */
        int y = top + 18 + i * 14;
        xr_canvas_fill_rect(c, xr_rect(cx - pw + 8, y, pw - 24, 4), XR_WHITE);
        xr_canvas_fill_rect(c, xr_rect(cx + 16, y, pw - 24, 4), XR_WHITE);
    }
    xr_widget_render_tree(&p->scope.root, c);
}

static void splash_enter(xr_page_t *p)
{
    XR_CONTAINER_OF(p, splash_page_t, base)->entered_ms = xr_shell_now(p->shell);
}

static void splash_tick(xr_page_t *p, uint32_t now)
{
    splash_page_t *sp = XR_CONTAINER_OF(p, splash_page_t, base);
    if (now - sp->entered_ms >= SPLASH_MS) xr_shell_replace(p->shell, app_page_home());
}

static const xr_page_vtbl_t k_splash_vtbl = {
    .on_create = splash_create,
    .on_enter = splash_enter,
    .render = splash_render,
    .on_tick = splash_tick,
};

xr_page_t *app_page_splash(void)
{
    xr_page_init(&s_splash.base, &k_splash_vtbl, "X-Reader", XR_CHROME_NONE);
    return &s_splash.base;
}
