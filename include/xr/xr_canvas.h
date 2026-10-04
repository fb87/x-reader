/*
 * xr_canvas.h - drawing into a framebuffer of any supported pixel format.
 *
 * The canvas does not own memory: the display port hands it the
 * framebuffer. All drawing is clipped to canvas->clip, which the shell
 * sets to the dirty rectangle being recomposed.
 */
#ifndef XR_CANVAS_H
#define XR_CANVAS_H

#include "xr_types.h"

typedef enum xr_pixfmt {
    XR_PIXFMT_MONO1, /* 1 bpp, MSB first, 1 = white (e.g. small SPI panels) */
    XR_PIXFMT_GRAY4, /* 4 bpp, high nibble first (e.g. IT8951 / M5Paper)    */
    XR_PIXFMT_GRAY8  /* 8 bpp                                               */
} xr_pixfmt_t;

typedef struct xr_canvas {
    uint8_t *buf;
    int16_t width, height;
    int32_t stride; /* bytes per row */
    xr_pixfmt_t fmt;
    xr_rect_t clip;
} xr_canvas_t;

int32_t xr_canvas_stride_for(int width, xr_pixfmt_t fmt);
size_t xr_canvas_buffer_size(int width, int height, xr_pixfmt_t fmt);

void xr_canvas_init(xr_canvas_t *c, void *buf, int width, int height,
                    int32_t stride, xr_pixfmt_t fmt);

/* Sets the clip (intersected with the canvas bounds); returns the old one. */
xr_rect_t xr_canvas_set_clip(xr_canvas_t *c, xr_rect_t clip);

uint8_t xr_canvas_get_pixel(const xr_canvas_t *c, int x, int y);

/* Solid fill. On 1 bpp, mid grays are ordered-dithered. */
void xr_canvas_fill_rect(xr_canvas_t *c, xr_rect_t r, uint8_t gray);
void xr_canvas_draw_rect(xr_canvas_t *c, xr_rect_t r, int thickness, uint8_t gray);
void xr_canvas_hline(xr_canvas_t *c, int x, int y, int w, uint8_t gray);
void xr_canvas_vline(xr_canvas_t *c, int x, int y, int h, uint8_t gray);

/* 50% checkerboard: cheap "shadow"/"dim" that looks right on every panel. */
void xr_canvas_stipple_rect(xr_canvas_t *c, xr_rect_t r, uint8_t gray);

/* Blend an 8-bit alpha mask (glyphs, icons) in the given gray. */
void xr_canvas_draw_mask(xr_canvas_t *c, int x, int y, int w, int h,
                         const uint8_t *alpha, int alpha_stride, uint8_t gray);

#endif /* XR_CANVAS_H */
