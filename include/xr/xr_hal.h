/*
 * xr_hal.h - the only things a hardware port has to provide.
 *
 *   xr_display_t   framebuffer + "push this rect with this waveform"
 *   xr_platform_t  milliseconds, wall clock, battery
 *   input          the port's main loop builds xr_event_t and calls
 *                  xr_shell_dispatch(); no driver interface is imposed.
 *
 * Everything above this file is hardware independent.
 */
#ifndef XR_HAL_H
#define XR_HAL_H

#include "xr_canvas.h"

/* ---------------------------------------------------------------- display */

typedef struct xr_display xr_display_t;

typedef struct xr_display_ops {
    /* Push `area` of the framebuffer to the panel using `mode`.
     * XR_REFRESH_FULL is always called with the whole screen. */
    void (*update)(xr_display_t *d, xr_rect_t area, xr_refresh_t mode);
    /* Optional: block until the panel finished the last update. */
    void (*wait_idle)(xr_display_t *d);
} xr_display_ops_t;

struct xr_display {
    const xr_display_ops_t *ops;
    int16_t width, height;  /* logical (after any rotation the port does) */
    xr_pixfmt_t fmt;
    void *framebuffer;
    int32_t stride;         /* 0 = derive from width/fmt */
    uint8_t update_align;   /* partial-update x alignment, e.g. 8 for 1bpp */
    void *ctx;              /* port private */
};

/* ---------------------------------------------------------------- platform */

typedef struct xr_platform_ops {
    uint32_t (*now_ms)(void *ctx);
    void (*wall_time)(void *ctx, int *hour, int *minute);
    int (*battery_percent)(void *ctx); /* <0 = unknown */
} xr_platform_ops_t;

typedef struct xr_platform {
    const xr_platform_ops_t *ops;
    void *ctx;
} xr_platform_t;

/* ---------------------------------------------------------------- input */

typedef enum xr_key {
    XR_KEY_NONE = 0,
    XR_KEY_UP, XR_KEY_DOWN, XR_KEY_LEFT, XR_KEY_RIGHT,
    XR_KEY_OK, XR_KEY_BACK, XR_KEY_MENU,
    XR_KEY_NEXT, XR_KEY_PREV, /* page-turn buttons */
    XR_KEY_POWER
} xr_key_t;

typedef enum xr_event_type { XR_EV_NONE = 0, XR_EV_KEY, XR_EV_TAP } xr_event_type_t;

typedef struct xr_event {
    xr_event_type_t type;
    xr_key_t key;
    bool long_press;
    int16_t x, y; /* XR_EV_TAP */
} xr_event_t;

static inline xr_event_t xr_ev_key(xr_key_t k)
{
    xr_event_t e = { XR_EV_KEY, k, false, 0, 0 };
    return e;
}

static inline xr_event_t xr_ev_tap(int x, int y)
{
    xr_event_t e = { XR_EV_TAP, XR_KEY_NONE, false, (int16_t)x, (int16_t)y };
    return e;
}

#endif /* XR_HAL_H */
