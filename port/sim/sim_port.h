#ifndef SIM_PORT_H
#define SIM_PORT_H

#include <stdio.h>

#include "xr/xr.h"

typedef struct sim_display {
    xr_display_t display;
    const char *name, *out_dir, *label;
    FILE *log;
    int frame;
    int counts[4];
    long pixels[4];
} sim_display_t;

typedef struct sim_platform {
    xr_platform_t platform;
    uint32_t now_ms;
    int battery;
} sim_platform_t;

void sim_display_init(sim_display_t *sd, const char *name, int w, int h, xr_pixfmt_t fmt,
                      uint8_t align, const char *out_dir);
void sim_display_close(sim_display_t *sd);
void sim_platform_init(sim_platform_t *sp);

#endif
