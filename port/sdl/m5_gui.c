/* Interactive SDL2 host port for the M5Paper-style 540x960 GRAY4 panel. */
#include <SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "sim_port.h"

#define ROTARY_HOLD_MS 500

typedef struct gui_display {
    xr_display_t display;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint8_t *rgb;
} gui_display_t;

typedef struct gui_app {
    xr_shell_t shell;
    gui_display_t display;
    sim_platform_t platform;
} gui_app_t;

static void gui_present(gui_display_t *gui)
{
    int window_w, window_h;
    SDL_GetRendererOutputSize(gui->renderer, &window_w, &window_h);
    float scale = (float)window_w / gui->display.width;
    if ((float)window_h / gui->display.height < scale)
        scale = (float)window_h / gui->display.height;
    SDL_Rect dst = {
        (window_w - (int)(gui->display.width * scale)) / 2,
        (window_h - (int)(gui->display.height * scale)) / 2,
        (int)(gui->display.width * scale),
        (int)(gui->display.height * scale),
    };

    SDL_UpdateTexture(gui->texture, NULL, gui->rgb, gui->display.width * 3);
    SDL_SetRenderDrawColor(gui->renderer, 255, 255, 255, 255);
    SDL_RenderClear(gui->renderer);
    SDL_RenderCopy(gui->renderer, gui->texture, NULL, &dst);
    SDL_RenderPresent(gui->renderer);
}

static void gui_update(xr_display_t *display, xr_rect_t area, xr_refresh_t mode)
{
    (void)mode;
    gui_display_t *gui = display->ctx;
    xr_canvas_t canvas;
    xr_canvas_init(&canvas, display->framebuffer, display->width, display->height,
                   display->stride, display->fmt);

    for (int y = area.y; y < area.y + area.h; y++) {
        for (int x = area.x; x < area.x + area.w; x++) {
            uint8_t shade = xr_canvas_get_pixel(&canvas, x, y);
            uint8_t *pixel = gui->rgb + (y * display->width + x) * 3;
            pixel[0] = shade;
            pixel[1] = shade;
            pixel[2] = shade;
        }
    }
    gui_present(gui);
}

static const xr_display_ops_t k_gui_display_ops = { gui_update, NULL };

static bool gui_display_init(gui_display_t *gui, SDL_Renderer *renderer)
{
    const int width = 540;
    const int height = 960;
    memset(gui, 0, sizeof *gui);
    gui->display.ops = &k_gui_display_ops;
    gui->display.width = width;
    gui->display.height = height;
    gui->display.fmt = XR_PIXFMT_GRAY4;
    gui->display.framebuffer = calloc(1, xr_canvas_buffer_size(width, height, XR_PIXFMT_GRAY4));
    gui->display.update_align = 4;
    gui->display.ctx = gui;
    gui->renderer = renderer;
    gui->rgb = calloc((size_t)width * height, 3);
    gui->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
                                    SDL_TEXTUREACCESS_STREAMING, width, height);
    return gui->display.framebuffer && gui->rgb && gui->texture;
}

static xr_key_t map_key(SDL_Keycode key)
{
    switch (key) {
    case SDLK_UP: return XR_KEY_UP;
    case SDLK_DOWN: return XR_KEY_DOWN;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return XR_KEY_OK;
    default: return XR_KEY_NONE;
    }
}

static void dispatch_key(gui_app_t *app, SDL_Keycode key, bool long_press)
{
    xr_key_t mapped = map_key(key);
    if (mapped == XR_KEY_NONE) return;
    xr_event_t event = xr_ev_key(mapped);
    event.long_press = long_press;
    xr_shell_dispatch(&app->shell, &event);
    xr_shell_flush(&app->shell);
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) return 1;
    SDL_Window *window = SDL_CreateWindow("X-Reader M5Paper Simulator",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          540, 960, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    gui_app_t app;
    memset(&app, 0, sizeof app);
    if (!gui_display_init(&app.display, renderer)) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    sim_platform_init(&app.platform);
    xr_shell_init(&app.shell, &app.display.display, &app.platform.platform, app_theme());
    app_start(&app.shell);
    xr_shell_tick(&app.shell);
    xr_shell_flush(&app.shell);

    bool running = true;
    SDL_Keycode held_key = SDLK_UNKNOWN;
    bool long_press_sent = false;
    uint32_t held_at = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_UP || key == SDLK_DOWN) {
                    held_key = key;
                    long_press_sent = false;
                    held_at = SDL_GetTicks();
                } else {
                    dispatch_key(&app, key, false);
                }
            } else if (event.type == SDL_KEYUP && event.key.keysym.sym == held_key) {
                if (!long_press_sent) dispatch_key(&app, held_key, false);
                held_key = SDLK_UNKNOWN;
            }
            else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED)
                gui_present(&app.display);
        }
        uint32_t now = SDL_GetTicks();
        if (held_key != SDLK_UNKNOWN && !long_press_sent && now - held_at >= ROTARY_HOLD_MS) {
            dispatch_key(&app, held_key, true);
            long_press_sent = true;
        }
        app.platform.now_ms = now;
        xr_shell_tick(&app.shell);
        xr_shell_flush(&app.shell);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(app.display.texture);
    free(app.display.rgb);
    free(app.display.display.framebuffer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
