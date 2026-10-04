#include "app_internal.h"

static void sleep_render(xr_page_t *p, xr_canvas_t *c)
{
    const xr_theme_t *t = p->shell->theme;
    xr_rect_t a = p->area;
    int cx = a.x + a.w / 2, cy = a.y + a.h / 2;
    xr_canvas_fill_rect(c, a, XR_BLACK);
    xr_canvas_draw_rect(c, xr_rect(cx - 38, cy - 52, 76, 104), 3, XR_WHITE);
    xr_canvas_hline(c, cx - 20, cy - 20, 40, XR_WHITE);
    xr_canvas_hline(c, cx - 20, cy - 4, 40, XR_WHITE);
    xr_canvas_hline(c, cx - 20, cy + 12, 28, XR_WHITE);
    xr_canvas_draw_text_in(c, t->font_small, xr_rect(a.x, cy + 130, a.w, 30),
                           "Sleeping", XR_ALIGN_CENTER, XR_WHITE);
}

static bool sleep_event(xr_page_t *p, const xr_event_t *ev)
{
    if (ev->type == XR_EV_KEY || ev->type == XR_EV_TAP) {
        xr_shell_replace(p->shell, app_page_home());
        return true;
    }
    return false;
}

static const xr_page_vtbl_t k_sleep_vtbl = { .render = sleep_render, .on_event = sleep_event };

xr_page_t *app_page_sleep(void)
{
    static xr_page_t page;
    xr_page_init(&page, &k_sleep_vtbl, "Sleep", XR_CHROME_NONE);
    return &page;
}
