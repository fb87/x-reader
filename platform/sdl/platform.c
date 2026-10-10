#include "platform/sdl/platform.h"
#include "font.h"

#include <SDL.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum {
    DISPLAY_WIDTH = 540,
    DISPLAY_HEIGHT = 960,
    DISPLAY_ROW_BYTES = DISPLAY_WIDTH / 2,
};

struct sdl_context {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint8_t* framebuffer;
    uint32_t* pixels;
    uint64_t started;
};

static uint8_t* pixel_at(struct sdl_context* context, int32_t x, int32_t y) {
    return &context->framebuffer[(size_t)y * DISPLAY_ROW_BYTES + (size_t)x / 2U];
}

static void blend_pixel(struct sdl_context* context, int32_t x, int32_t y, uint8_t gray,
                        uint8_t alpha) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT || alpha == 0U) {
        return;
    }
    uint8_t* pixel = pixel_at(context, x, y);
    const uint8_t current = (x & 1) == 0 ? *pixel >> 4U : *pixel & 0x0fU;
    const uint16_t background = (uint16_t)current * 17U;
    const uint16_t foreground = (uint16_t)gray * 17U;
    const uint8_t blended =
        (uint8_t)((background * (255U - alpha) + foreground * alpha) / 255U / 17U);
    if ((x & 1) == 0) {
        *pixel = (uint8_t)((*pixel & 0x0fU) | (blended << 4U));
    } else {
        *pixel = (uint8_t)((*pixel & 0xf0U) | blended);
    }
}

static enum platform_status clear_display(void* opaque, uint8_t gray) {
    struct sdl_context* context = opaque;
    memset(context->framebuffer, (int)((gray << 4U) | gray), DISPLAY_ROW_BYTES * DISPLAY_HEIGHT);
    return PLATFORM_OK;
}

static enum platform_status draw_rectangle(void* opaque, int32_t x, int32_t y, int32_t width,
                                           int32_t height, uint8_t gray) {
    struct sdl_context* context = opaque;
    if (width < 0 || height < 0) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    const int32_t right = x + width;
    const int32_t bottom = y + height;
    for (int32_t row = y < 0 ? 0 : y; row < bottom && row < DISPLAY_HEIGHT; ++row) {
        for (int32_t column = x < 0 ? 0 : x; column < right && column < DISPLAY_WIDTH; ++column) {
            uint8_t* pixel = pixel_at(context, column, row);
            if ((column & 1) == 0) {
                *pixel = (uint8_t)((*pixel & 0x0fU) | (gray << 4U));
            } else {
                *pixel = (uint8_t)((*pixel & 0xf0U) | gray);
            }
        }
    }
    return PLATFORM_OK;
}

static uint32_t next_codepoint(const char* text, size_t length, size_t* position) {
    const uint8_t first = (uint8_t)text[*position];
    *position += 1U;
    if (first < 0x80U || *position >= length) {
        return first;
    }
    if ((first & 0xe0U) == 0xc0U && *position < length) {
        const uint8_t second = (uint8_t)text[(*position)++];
        return ((uint32_t)(first & 0x1fU) << 6U) | (second & 0x3fU);
    }
    if ((first & 0xf0U) == 0xe0U && *position + 1U < length) {
        const uint8_t second = (uint8_t)text[(*position)++];
        const uint8_t third = (uint8_t)text[(*position)++];
        return ((uint32_t)(first & 0x0fU) << 12U) | ((uint32_t)(second & 0x3fU) << 6U) |
               (third & 0x3fU);
    }
    return 0xfffdU;
}

static enum platform_status draw_text(void* opaque, const struct font* font, int32_t x, int32_t y,
                                      const char* text, size_t length, uint8_t scale,
                                      uint8_t gray) {
    struct sdl_context* context = opaque;
    size_t position = 0U;
    int32_t origin_x = x;
    while (position < length) {
        const struct font_glyph* glyph = font_lookup(font, next_codepoint(text, length, &position));
        if (glyph == NULL) {
            glyph = font_lookup(font, 0xfffdU);
        }
        if (glyph == NULL) {
            continue;
        }
        const uint8_t* bitmap = &font->bitmap[glyph->bitmap_offset];
        const int32_t glyph_x = origin_x + (int32_t)glyph->bearing_x * scale;
        const int32_t glyph_y = y + ((int32_t)glyph->bearing_y + font->baseline) * scale;
        for (uint16_t row = 0U; row < glyph->height; ++row) {
            for (uint16_t column = 0U; column < glyph->width; ++column) {
                const uint8_t alpha = bitmap[(size_t)row * glyph->width + column];
                for (uint8_t y_offset = 0U; y_offset < scale; ++y_offset) {
                    for (uint8_t x_offset = 0U; x_offset < scale; ++x_offset) {
                        blend_pixel(context, glyph_x + (int32_t)column * scale + x_offset,
                                    glyph_y + (int32_t)row * scale + y_offset, gray, alpha);
                    }
                }
            }
        }
        origin_x += (int32_t)glyph->advance * scale;
    }
    return PLATFORM_OK;
}

static enum platform_status present_display(void* opaque, bool partial) {
    (void)partial;
    struct sdl_context* context = opaque;
    for (int32_t y = 0; y < DISPLAY_HEIGHT; ++y) {
        for (int32_t x = 0; x < DISPLAY_WIDTH; ++x) {
            const uint8_t packed = *pixel_at(context, x, y);
            const uint8_t gray = (x & 1) == 0 ? packed >> 4U : packed & 0x0fU;
            const uint8_t value = (uint8_t)(gray * 17U);
            context->pixels[(size_t)y * DISPLAY_WIDTH + (size_t)x] =
                0xff000000U | ((uint32_t)value << 16U) | ((uint32_t)value << 8U) | value;
        }
    }
    if (SDL_UpdateTexture(context->texture, NULL, context->pixels,
                          DISPLAY_WIDTH * (int)sizeof(*context->pixels)) != 0) {
        return PLATFORM_ERROR;
    }
    if (SDL_RenderClear(context->renderer) != 0 ||
        SDL_RenderCopy(context->renderer, context->texture, NULL, NULL) != 0) {
        return PLATFORM_ERROR;
    }
    SDL_RenderPresent(context->renderer);
    return PLATFORM_OK;
}

static enum platform_status next_event(void* opaque, struct platform_event* event) {
    (void)opaque;
    SDL_Event sdl_event;
    if (SDL_WaitEventTimeout(&sdl_event, 16) == 0) {
        event->type = PLATFORM_EVENT_NONE;
        return PLATFORM_OK;
    }
    memset(event, 0, sizeof(*event));
    switch (sdl_event.type) {
    case SDL_QUIT:
        event->type = PLATFORM_EVENT_QUIT;
        break;
    case SDL_KEYDOWN:
        event->type = PLATFORM_EVENT_BUTTON;
        event->button = (int32_t)sdl_event.key.keysym.sym;
        break;
    case SDL_MOUSEBUTTONDOWN:
        event->type = PLATFORM_EVENT_TOUCH_DOWN;
        event->x = sdl_event.button.x;
        event->y = sdl_event.button.y;
        break;
    case SDL_MOUSEMOTION:
        event->type = PLATFORM_EVENT_TOUCH_MOVE;
        event->x = sdl_event.motion.x;
        event->y = sdl_event.motion.y;
        break;
    case SDL_MOUSEBUTTONUP:
        event->type = PLATFORM_EVENT_TOUCH_UP;
        event->x = sdl_event.button.x;
        event->y = sdl_event.button.y;
        break;
    default:
        event->type = PLATFORM_EVENT_NONE;
        break;
    }
    return PLATFORM_OK;
}

static uint64_t monotonic_ms(void* opaque) {
    struct sdl_context* context = opaque;
    return SDL_GetTicks64() - context->started;
}

static void sleep_ms(void* opaque, uint32_t milliseconds) {
    (void)opaque;
    SDL_Delay(milliseconds);
}

enum platform_status sdl_platform_create(struct platform* platform, const char* title) {
    if (platform == NULL || title == NULL) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        return PLATFORM_ERROR;
    }
    struct sdl_context* context = calloc(1U, sizeof(*context));
    if (context == NULL) {
        SDL_Quit();
        return PLATFORM_ERROR;
    }
    context->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       DISPLAY_WIDTH, DISPLAY_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
    context->renderer = context->window == NULL
                            ? NULL
                            : SDL_CreateRenderer(context->window, -1, SDL_RENDERER_ACCELERATED);
    if (context->renderer == NULL && context->window != NULL) {
        context->renderer = SDL_CreateRenderer(context->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (context->renderer != NULL) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        (void)SDL_RenderSetLogicalSize(context->renderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }
    context->texture =
        context->renderer == NULL
            ? NULL
            : SDL_CreateTexture(context->renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    context->framebuffer =
        calloc(DISPLAY_ROW_BYTES * DISPLAY_HEIGHT, sizeof(*context->framebuffer));
    context->pixels = calloc(DISPLAY_WIDTH * DISPLAY_HEIGHT, sizeof(*context->pixels));
    if (context->window == NULL || context->renderer == NULL || context->texture == NULL ||
        context->framebuffer == NULL || context->pixels == NULL) {
        sdl_platform_destroy(&(struct platform){.context = context});
        return PLATFORM_ERROR;
    }
    context->started = SDL_GetTicks64();
    platform->context = context;
    platform->display = (struct platform_display){
        .context = context,
        .clear = clear_display,
        .rectangle = draw_rectangle,
        .text = draw_text,
        .present = present_display,
    };
    platform->next_event = next_event;
    platform->monotonic_ms = monotonic_ms;
    platform->sleep_ms = sleep_ms;
    return PLATFORM_OK;
}

void sdl_platform_destroy(struct platform* platform) {
    if (platform == NULL || platform->context == NULL) {
        return;
    }
    struct sdl_context* context = platform->context;
    free(context->pixels);
    free(context->framebuffer);
    SDL_DestroyTexture(context->texture);
    SDL_DestroyRenderer(context->renderer);
    SDL_DestroyWindow(context->window);
    free(context);
    SDL_Quit();
    memset(platform, 0, sizeof(*platform));
}
