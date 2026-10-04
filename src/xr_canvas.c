#include "xr/xr_canvas.h"

#include <string.h>

/* 4x4 Bayer thresholds scaled to 0..255 (used for gray fills on 1 bpp). */
static const uint8_t k_bayer4[4][4] = {
    {   8, 136,  40, 168 },
    { 200,  72, 232, 104 },
    {  56, 184,  24, 152 },
    { 248, 120, 216,  88 },
};

int32_t xr_canvas_stride_for(int width, xr_pixfmt_t fmt)
{
    switch (fmt) {
    case XR_PIXFMT_MONO1: return (width + 7) / 8;
    case XR_PIXFMT_GRAY4: return (width + 1) / 2;
    default:              return width;
    }
}

size_t xr_canvas_buffer_size(int width, int height, xr_pixfmt_t fmt)
{
    return (size_t)xr_canvas_stride_for(width, fmt) * (size_t)height;
}

void xr_canvas_init(xr_canvas_t *c, void *buf, int width, int height,
                    int32_t stride, xr_pixfmt_t fmt)
{
    c->buf = (uint8_t *)buf;
    c->width = (int16_t)width;
    c->height = (int16_t)height;
    c->stride = stride ? stride : xr_canvas_stride_for(width, fmt);
    c->fmt = fmt;
    c->clip = xr_rect(0, 0, width, height);
}

xr_rect_t xr_canvas_set_clip(xr_canvas_t *c, xr_rect_t clip)
{
    xr_rect_t old = c->clip;
    c->clip = xr_rect_intersect(clip, xr_rect(0, 0, c->width, c->height));
    return old;
}

static inline void px_put(xr_canvas_t *c, int x, int y, uint8_t g, bool dither)
{
    uint8_t *row = c->buf + (int32_t)y * c->stride;
    switch (c->fmt) {
    case XR_PIXFMT_GRAY8:
        row[x] = g;
        break;
    case XR_PIXFMT_GRAY4: {
        uint8_t v = g >> 4;
        uint8_t *p = &row[x >> 1];
        *p = (x & 1) ? (uint8_t)((*p & 0xF0) | v) : (uint8_t)((*p & 0x0F) | (v << 4));
        break;
    }
    case XR_PIXFMT_MONO1: {
        uint8_t thr = dither ? k_bayer4[y & 3][x & 3] : 128;
        uint8_t m = (uint8_t)(0x80 >> (x & 7));
        if (g >= thr) row[x >> 3] |= m;
        else          row[x >> 3] &= (uint8_t)~m;
        break;
    }
    }
}

uint8_t xr_canvas_get_pixel(const xr_canvas_t *c, int x, int y)
{
    const uint8_t *row = c->buf + (int32_t)y * c->stride;
    switch (c->fmt) {
    case XR_PIXFMT_GRAY8: return row[x];
    case XR_PIXFMT_GRAY4: {
        uint8_t p = row[x >> 1];
        uint8_t v = (x & 1) ? (p & 0x0F) : (p >> 4);
        return (uint8_t)(v * 17);
    }
    case XR_PIXFMT_MONO1:
        return (row[x >> 3] & (0x80 >> (x & 7))) ? 255 : 0;
    }
    return 255;
}

void xr_canvas_fill_rect(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    r = xr_rect_intersect(r, c->clip);
    if (xr_rect_empty(r)) return;
    if (c->fmt == XR_PIXFMT_GRAY8) {
        for (int y = r.y; y < r.y + r.h; y++)
            memset(c->buf + (int32_t)y * c->stride + r.x, gray, (size_t)r.w);
        return;
    }
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            px_put(c, x, y, gray, true);
}

void xr_canvas_hline(xr_canvas_t *c, int x, int y, int w, uint8_t gray)
{
    xr_canvas_fill_rect(c, xr_rect(x, y, w, 1), gray);
}

void xr_canvas_vline(xr_canvas_t *c, int x, int y, int h, uint8_t gray)
{
    xr_canvas_fill_rect(c, xr_rect(x, y, 1, h), gray);
}

void xr_canvas_draw_rect(xr_canvas_t *c, xr_rect_t r, int t, uint8_t gray)
{
    if (t <= 0) return;
    xr_canvas_fill_rect(c, xr_rect(r.x, r.y, r.w, t), gray);
    xr_canvas_fill_rect(c, xr_rect(r.x, r.y + r.h - t, r.w, t), gray);
    xr_canvas_fill_rect(c, xr_rect(r.x, r.y + t, t, r.h - 2 * t), gray);
    xr_canvas_fill_rect(c, xr_rect(r.x + r.w - t, r.y + t, t, r.h - 2 * t), gray);
}

void xr_canvas_stipple_rect(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    r = xr_rect_intersect(r, c->clip);
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            if (((x + y) & 1) == 0) px_put(c, x, y, gray, false);
}

void xr_canvas_draw_mask(xr_canvas_t *c, int x, int y, int w, int h,
                         const uint8_t *alpha, int alpha_stride, uint8_t gray)
{
    xr_rect_t r = xr_rect_intersect(xr_rect(x, y, w, h), c->clip);
    for (int py = r.y; py < r.y + r.h; py++) {
        const uint8_t *arow = alpha + (py - y) * alpha_stride;
        for (int px = r.x; px < r.x + r.w; px++) {
            uint8_t a = arow[px - x];
            if (a == 0) continue;
            if (c->fmt == XR_PIXFMT_MONO1) {
                /* No gray to blend into: keep thin strokes by inking at
                 * ~30% coverage instead of thresholding the blend at 50%. */
                if (a >= 80) px_put(c, px, py, gray, false);
                continue;
            }
            if (a == 255) {
                px_put(c, px, py, gray, false);
            } else {
                uint8_t bg = xr_canvas_get_pixel(c, px, py);
                uint8_t out = (uint8_t)((bg * (255 - a) + gray * a) / 255);
                px_put(c, px, py, out, false);
            }
        }
    }
}
