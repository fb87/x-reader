/*
 * xr_shell.h - owns the screen.
 *
 * Regions (layout):   status bar | page area | dock
 * Layers (z-order):   0 background, 1 content (chrome + top page), 2 dialogs
 *
 * Everything that changes calls xr_shell_invalidate(rect, mode). The
 * port's main loop then calls xr_shell_flush(), which recomposes each
 * dirty rect bottom-to-top into the single framebuffer and pushes it to
 * the panel with the requested waveform.
 *
 * Typical main loop:
 *     while (1) {
 *         if (input_poll(&ev)) xr_shell_dispatch(&shell, &ev);
 *         xr_shell_tick(&shell);
 *         xr_shell_flush(&shell);
 *         sleep_until_input_or_timer();
 *     }
 */
#ifndef XR_SHELL_H
#define XR_SHELL_H

#include "xr_dialog.h"
#include "xr_page.h"
#include "xr_refresh.h"

#ifndef XR_SHELL_MAX_PAGES
#define XR_SHELL_MAX_PAGES 8
#endif
#ifndef XR_SHELL_MAX_DIALOGS
#define XR_SHELL_MAX_DIALOGS 4
#endif

typedef struct xr_theme {
    const xr_font_t *font_small, *font_normal, *font_bold, *font_title, *font_body;
    int16_t status_h, dock_h, pad, row_h;
} xr_theme_t;

typedef struct xr_shell xr_shell_t;
typedef void (*xr_background_fn)(xr_shell_t *s, xr_canvas_t *c, void *user);

struct xr_shell {
    xr_display_t *display;
    const xr_platform_t *platform;
    const xr_theme_t *theme;
    xr_canvas_t canvas;
    xr_refresh_sched_t sched;
    xr_rect_t screen, status_rect, dock_rect;

    xr_page_t *pages[XR_SHELL_MAX_PAGES];
    uint8_t page_count;
    int8_t dock_focus; /* selected dock action for keyboard navigation, -1 = page content */
    xr_dialog_t *dialogs[XR_SHELL_MAX_DIALOGS];
    uint8_t dialog_count;

    xr_background_fn background;
    void *background_user;

    char clock[8];
    int battery;
};

void xr_shell_init(xr_shell_t *s, xr_display_t *display, const xr_platform_t *platform,
                   const xr_theme_t *theme);
void xr_shell_set_background(xr_shell_t *s, xr_background_fn fn, void *user);
/* Promote to a full flash after N QUALITY updates (0 = never). */
void xr_shell_set_full_refresh_every(xr_shell_t *s, uint16_t n);

/* Navigation */
void xr_shell_push(xr_shell_t *s, xr_page_t *p);
void xr_shell_pop(xr_shell_t *s);
void xr_shell_replace(xr_shell_t *s, xr_page_t *p);
xr_page_t *xr_shell_top(const xr_shell_t *s);
void xr_shell_set_chrome(xr_shell_t *s, xr_page_t *p, uint8_t chrome);
xr_rect_t xr_shell_page_area(const xr_shell_t *s, uint8_t chrome);

/* Dialogs */
void xr_shell_show_dialog(xr_shell_t *s, xr_dialog_t *d);
void xr_shell_close_dialog(xr_shell_t *s, xr_dialog_t *d, int result);

/* Rendering / input / time */
void xr_shell_invalidate(xr_shell_t *s, xr_rect_t r, xr_refresh_t mode);
void xr_shell_dispatch(xr_shell_t *s, const xr_event_t *ev);
void xr_shell_tick(xr_shell_t *s);
void xr_shell_flush(xr_shell_t *s);
uint32_t xr_shell_now(const xr_shell_t *s);

#endif /* XR_SHELL_H */
