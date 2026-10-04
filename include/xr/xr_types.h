/*
 * xr_types.h - basic types shared by every X-Reader UI module.
 *
 * Coordinates are absolute screen pixels. Gray levels are 0 (black)
 * .. 255 (white); the canvas quantizes to the panel's pixel format.
 */
#ifndef XR_TYPES_H
#define XR_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define XR_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#define XR_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define XR_MIN(a, b) ((a) < (b) ? (a) : (b))
#define XR_MAX(a, b) ((a) > (b) ? (a) : (b))

/* Gray palette (4 levels map cleanly onto 1/2/4/8 bpp panels). */
#define XR_BLACK 0x00
#define XR_DARK  0x55
#define XR_LIGHT 0xAA
#define XR_WHITE 0xFF

typedef struct xr_rect {
    int16_t x, y, w, h;
} xr_rect_t;

/*
 * E-ink refresh intent. Widgets and pages state *what* they need;
 * the display port maps it onto real waveforms (A2/DU/GC16/...).
 */
typedef enum xr_refresh {
    XR_REFRESH_NONE = 0,
    XR_REFRESH_FAST,    /* partial, 1-bit-ish, no flash: focus moves, menus  */
    XR_REFRESH_QUALITY, /* partial, full grayscale: text pages, dialogs      */
    XR_REFRESH_FULL     /* full screen with flash: clears ghosting           */
} xr_refresh_t;

static inline xr_rect_t xr_rect(int x, int y, int w, int h)
{
    xr_rect_t r = { (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h };
    return r;
}

static inline bool xr_rect_empty(xr_rect_t r) { return r.w <= 0 || r.h <= 0; }

static inline bool xr_rect_contains(xr_rect_t r, int x, int y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static inline xr_rect_t xr_rect_intersect(xr_rect_t a, xr_rect_t b)
{
    int x0 = XR_MAX(a.x, b.x), y0 = XR_MAX(a.y, b.y);
    int x1 = XR_MIN(a.x + a.w, b.x + b.w), y1 = XR_MIN(a.y + a.h, b.y + b.h);
    if (x1 <= x0 || y1 <= y0) return xr_rect(0, 0, 0, 0);
    return xr_rect(x0, y0, x1 - x0, y1 - y0);
}

static inline bool xr_rect_intersects(xr_rect_t a, xr_rect_t b)
{
    return !xr_rect_empty(xr_rect_intersect(a, b));
}

static inline xr_rect_t xr_rect_union(xr_rect_t a, xr_rect_t b)
{
    if (xr_rect_empty(a)) return b;
    if (xr_rect_empty(b)) return a;
    int x0 = XR_MIN(a.x, b.x), y0 = XR_MIN(a.y, b.y);
    int x1 = XR_MAX(a.x + a.w, b.x + b.w), y1 = XR_MAX(a.y + a.h, b.y + b.h);
    return xr_rect(x0, y0, x1 - x0, y1 - y0);
}

static inline xr_rect_t xr_rect_inset(xr_rect_t r, int d)
{
    return xr_rect(r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d);
}

static inline int32_t xr_rect_area(xr_rect_t r)
{
    return xr_rect_empty(r) ? 0 : (int32_t)r.w * r.h;
}

#endif /* XR_TYPES_H */
