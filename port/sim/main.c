/*
 * Simulator entry point: runs the same scripted session on two device
 * profiles to show the app is hardware independent.
 *
 *   m5paper   540x960, 4 bpp (IT8951-style 16 gray levels)
 *   xteink    480x800, 1 bpp (small SPI panel, 8-px aligned updates)
 *
 * Usage: xr_sim [out_dir]
 */
#include <stdio.h>
#include <sys/stat.h>

#include "app.h"
#include "sim_port.h"

typedef struct ctx {
    xr_shell_t shell;
    sim_display_t disp;
    sim_platform_t plat;
} ctx_t;

static void run(ctx_t *c, const char *label)
{
    c->disp.label = label;
    xr_shell_tick(&c->shell);
    xr_shell_flush(&c->shell);
}

static void key(ctx_t *c, xr_key_t k, const char *label)
{
    xr_event_t ev = xr_ev_key(k);
    xr_shell_dispatch(&c->shell, &ev);
    run(c, label);
}

static void tap(ctx_t *c, int x, int y, const char *label)
{
    xr_event_t ev = xr_ev_tap(x, y);
    xr_shell_dispatch(&c->shell, &ev);
    run(c, label);
}

static void wait_ms(ctx_t *c, uint32_t ms, const char *label)
{
    c->plat.now_ms += ms;
    run(c, label);
}

/* Tap the i-th of n dock buttons. */
static void dock(ctx_t *c, int i, int n, const char *label)
{
    xr_rect_t r = c->shell.dock_rect;
    tap(c, r.w * (2 * i + 1) / (2 * n), r.y + r.h / 2, label);
}

static void scenario(const char *name, int w, int h, xr_pixfmt_t fmt, uint8_t align, const char *out)
{
    static ctx_t c;
    sim_platform_init(&c.plat);
    sim_display_init(&c.disp, name, w, h, fmt, align, out);
    xr_shell_init(&c.shell, &c.disp.display, &c.plat.platform, app_theme());

    app_start(&c.shell);
    run(&c, "boot: splash");
    wait_ms(&c, 1600, "splash timeout -> Home");
    key(&c, XR_KEY_DOWN, "Home: focus Library button");
    key(&c, XR_KEY_OK, "open Library");
    key(&c, XR_KEY_DOWN, "Library: select row 2");
    key(&c, XR_KEY_DOWN, "Library: select row 3");
    key(&c, XR_KEY_NEXT, "Library: next page of rows");
    key(&c, XR_KEY_PREV, "Library: previous page");
    key(&c, XR_KEY_OK, "Book info dialog");
    key(&c, XR_KEY_OK, "Read -> Reader (fullscreen)");
    key(&c, XR_KEY_NEXT, "page turn 1");
    key(&c, XR_KEY_NEXT, "page turn 2");
    tap(&c, w / 2, h / 2, "tap centre -> show chrome");
    dock(&c, 1, 3, "dock A+ -> larger font, repaginate");
    key(&c, XR_KEY_BACK, "BACK -> hide chrome");
    key(&c, XR_KEY_NEXT, "page turn 3 (6th QUALITY -> FULL)");
    key(&c, XR_KEY_NEXT, "page turn 4");
    key(&c, XR_KEY_BACK, "BACK -> Library");
    dock(&c, 1, 3, "dock Delete -> confirm dialog");
    key(&c, XR_KEY_LEFT, "confirm: focus Delete");
    key(&c, XR_KEY_OK, "Delete -> list updates");
    key(&c, XR_KEY_BACK, "BACK -> Home");
    key(&c, XR_KEY_DOWN, "Home: focus Settings (focus kept)");
    key(&c, XR_KEY_OK, "open Settings");
    key(&c, XR_KEY_DOWN, "Settings: select Full refresh");
    key(&c, XR_KEY_OK, "cycle Full refresh value");
    wait_ms(&c, 60000, "one minute later: clock");
    key(&c, XR_KEY_BACK, "BACK -> Home");

    sim_display_t *d = &c.disp;
    printf("%-8s %dx%d  updates: FAST=%d QUALITY=%d FULL=%d  (%d frames)\n", name, w, h,
           d->counts[XR_REFRESH_FAST], d->counts[XR_REFRESH_QUALITY], d->counts[XR_REFRESH_FULL], d->frame);
    sim_display_close(d);
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "out";
    mkdir(out, 0755);
    scenario("m5paper", 540, 960, XR_PIXFMT_GRAY4, 4, out);
    scenario("xteink", 480, 800, XR_PIXFMT_MONO1, 8, out);
    return 0;
}
