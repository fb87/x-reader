/*
 * Host simulator port: a fake panel that writes every update as a PGM
 * frame plus a CSV log (rect + waveform), and a virtual clock.
 * This file is the template for a real port: implement the same ops.
 */
#include "sim_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *mode_name(xr_refresh_t m)
{
    switch (m) {
    case XR_REFRESH_FAST: return "FAST";
    case XR_REFRESH_QUALITY: return "QUALITY";
    case XR_REFRESH_FULL: return "FULL";
    default: return "NONE";
    }
}

static void sim_update(xr_display_t *d, xr_rect_t r, xr_refresh_t mode)
{
    sim_display_t *sd = (sim_display_t *)d->ctx;
    xr_canvas_t view;
    xr_canvas_init(&view, d->framebuffer, d->width, d->height, d->stride, d->fmt);

    char path[512];
    snprintf(path, sizeof path, "%s/%s_%03d.pgm", sd->out_dir, sd->name, sd->frame);
    FILE *f = fopen(path, "wb");
    if (f) {
        fprintf(f, "P5\n%d %d\n255\n", d->width, d->height);
        for (int y = 0; y < d->height; y++)
            for (int x = 0; x < d->width; x++) fputc(xr_canvas_get_pixel(&view, x, y), f);
        fclose(f);
    }
    if (sd->log)
        fprintf(sd->log, "%d,%s,%d,%d,%d,%d,\"%s\"\n", sd->frame, mode_name(mode), r.x, r.y, r.w, r.h,
                sd->label ? sd->label : "");
    sd->counts[mode]++;
    sd->pixels[mode] += (long)r.w * r.h;
    sd->frame++;
}

static const xr_display_ops_t k_sim_display_ops = { sim_update, NULL };

void sim_display_init(sim_display_t *sd, const char *name, int w, int h, xr_pixfmt_t fmt,
                      uint8_t align, const char *out_dir)
{
    memset(sd, 0, sizeof *sd);
    sd->name = name;
    sd->out_dir = out_dir;
    size_t sz = xr_canvas_buffer_size(w, h, fmt);
    sd->display.ops = &k_sim_display_ops;
    sd->display.width = (int16_t)w;
    sd->display.height = (int16_t)h;
    sd->display.fmt = fmt;
    sd->display.framebuffer = calloc(1, sz); /* static buffer on a device */
    sd->display.stride = 0;
    sd->display.update_align = align;
    sd->display.ctx = sd;

    char path[512];
    snprintf(path, sizeof path, "%s/%s_log.csv", out_dir, name);
    sd->log = fopen(path, "w");
    if (sd->log) fprintf(sd->log, "frame,mode,x,y,w,h,step\n");
}

void sim_display_close(sim_display_t *sd)
{
    if (sd->log) fclose(sd->log);
    free(sd->display.framebuffer);
}

/* ---------------------------------------------------------------- platform */

static uint32_t sim_now(void *ctx) { return ((sim_platform_t *)ctx)->now_ms; }

static void sim_time(void *ctx, int *h, int *m)
{
    uint32_t minutes = 9 * 60 + 41 + ((sim_platform_t *)ctx)->now_ms / 60000;
    *h = (int)(minutes / 60) % 24;
    *m = (int)(minutes % 60);
}

static int sim_battery(void *ctx) { return ((sim_platform_t *)ctx)->battery; }

static const xr_platform_ops_t k_sim_platform_ops = { sim_now, sim_time, sim_battery };

void sim_platform_init(sim_platform_t *sp)
{
    sp->now_ms = 0;
    sp->battery = 87;
    sp->platform.ops = &k_sim_platform_ops;
    sp->platform.ctx = sp;
}
