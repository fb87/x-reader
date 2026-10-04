/*
 * Port template - copy to port/<board>/ and fill in the TODOs.
 * A port is: a framebuffer, one update() function, a clock, and a main
 * loop that turns buttons/touch into xr_event_t. Nothing else.
 *
 * Framebuffer sizing (static, no malloc):
 *   M5Paper   540x960 @ 4bpp = 259,200 B  -> put it in PSRAM
 *   Xteink    480x800 @ 1bpp =  48,000 B  -> fits in internal SRAM
 *
 * EPUB storage: bind xr_storage_t to FatFS FIL with f_lseek() + f_read().
 * The EPUB core has no allocator; provide a fixed extraction buffer and an
 * inflate_raw callback backed by IDF's miniz/zlib-compatible inflater.
 */
#include "app.h"
#include "xr/xr.h"

#define W 480
#define H 800
#define FMT XR_PIXFMT_MONO1

static uint8_t s_fb[(W + 7) / 8 * H]; /* use xr_canvas_buffer_size() for other formats */

static void board_update(xr_display_t *d, xr_rect_t r, xr_refresh_t mode)
{
    (void)d;
    switch (mode) {
    case XR_REFRESH_FAST:
        /* TODO: partial update of r with the fast waveform / LUT
         *       (IT8951: DU or A2; SPI controllers: partial LUT). */
        break;
    case XR_REFRESH_QUALITY:
        /* TODO: partial update of r with full grayscale, no flash
         *       (IT8951: GL16 / GC16 partial). */
        break;
    case XR_REFRESH_FULL:
        /* TODO: full-screen flashing refresh (IT8951: GC16 full;
         *       SPI: full LUT). r is always the whole screen. */
        break;
    default:
        break;
    }
    (void)r;
}

static void board_wait_idle(xr_display_t *d) { (void)d; /* TODO: poll BUSY pin */ }

static uint32_t board_now_ms(void *ctx) { (void)ctx; return 0; /* TODO: millis() */ }
static void board_time(void *ctx, int *h, int *m) { (void)ctx; *h = 0; *m = 0; /* TODO: RTC */ }
static int board_battery(void *ctx) { (void)ctx; return -1; /* TODO: ADC */ }

static const xr_display_ops_t k_display_ops = { board_update, board_wait_idle };
static const xr_platform_ops_t k_platform_ops = { board_now_ms, board_time, board_battery };

static xr_display_t s_display = { &k_display_ops, W, H, FMT, s_fb, 0, 8, NULL };
static xr_platform_t s_platform = { &k_platform_ops, NULL };
static xr_shell_t s_shell;

/* TODO: map GPIO buttons / touch controller to events. */
static bool board_poll_input(xr_event_t *ev) { (void)ev; return false; }

void board_main(void)
{
    xr_shell_init(&s_shell, &s_display, &s_platform, app_theme());
    app_start(&s_shell);
    /* TODO: open an SD-card FIL, wrap it in xr_storage_t, then call
     * app_load_epub(&storage, "Book title") after mounting FatFS. */
    for (;;) {
        xr_event_t ev;
        while (board_poll_input(&ev)) xr_shell_dispatch(&s_shell, &ev);
        xr_shell_tick(&s_shell);
        xr_shell_flush(&s_shell);
        /* TODO: light sleep until a button IRQ or the next minute. */
    }
}
