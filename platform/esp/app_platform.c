#include "m5paper_platform.h"

#include "board_config.h"
#include "buses.h"
#include "display_it8951.h"
#include "font.h"
#include "input_gt911.h"
#include "storage.h"

#include "driver/gpio.h"
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

enum {
    FRAMEBUFFER_WIDTH = 540,
    FRAMEBUFFER_HEIGHT = 960,
    FRAMEBUFFER_BYTES = FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT / 2,
    /* Covers the status bar (48px) and dock (64px) chrome rows defined in
     * lua/layout.lua, plus the reading scene's own header/footer bands.
     * Updates touching these rows always use GL16 so the frequent small
     * redraws there (clock, focus highlight, page footer) cannot build up
     * DU-mode ghosting at the panel edges. */
    CHROME_EDGE_MARGIN = 64,
};

struct app_platform {
    struct m5paper_buses buses;
    struct m5paper_it8951 display;
    struct m5paper_it8951_info display_info;
    struct m5paper_gt911 touch;
    struct m5paper_storage storage;
    uint8_t* framebuffer;
    uint8_t* previous_framebuffer;
    bool presented;
    int64_t started_us;
    int dial_levels[3];
};

static const char* const TAG = "x-reader-app";
static struct app_platform app_context;

static uint8_t* pixel(struct app_platform* context, int32_t x, int32_t y) {
    return &context->framebuffer[(size_t)y * (FRAMEBUFFER_WIDTH / 2U) + (size_t)x / 2U];
}

static void blend_pixel(struct app_platform* context, int32_t x, int32_t y, uint8_t gray,
                        uint8_t alpha) {
    if (x < 0 || x >= FRAMEBUFFER_WIDTH || y < 0 || y >= FRAMEBUFFER_HEIGHT || alpha == 0U) {
        return;
    }
    uint8_t* value = pixel(context, x, y);
    const uint8_t current = (x & 1) == 0 ? *value >> 4U : *value & 0x0fU;
    const uint16_t blended = ((uint16_t)current * (255U - alpha) + (uint16_t)gray * alpha) / 255U;
    if ((x & 1) == 0) {
        *value = (uint8_t)((*value & 0x0fU) | ((uint8_t)blended << 4U));
    } else {
        *value = (uint8_t)((*value & 0xf0U) | (uint8_t)blended);
    }
}

static uint32_t next_codepoint(const char* text, size_t length, size_t* position) {
    const uint8_t first = (uint8_t)text[*position];
    if (first < 0x80U) {
        *position += 1U;
        return first;
    }
    if ((first & 0xe0U) == 0xc0U && *position + 1U < length) {
        const uint8_t second = (uint8_t)text[*position + 1U];
        *position += 2U;
        return ((uint32_t)(first & 0x1fU) << 6U) | (second & 0x3fU);
    }
    if ((first & 0xf0U) == 0xe0U && *position + 2U < length) {
        const uint8_t second = (uint8_t)text[*position + 1U];
        const uint8_t third = (uint8_t)text[*position + 2U];
        *position += 3U;
        return ((uint32_t)(first & 0x0fU) << 12U) | ((uint32_t)(second & 0x3fU) << 6U) |
               (third & 0x3fU);
    }
    *position += 1U;
    return 0xfffdU;
}

static enum platform_status clear_display(void* opaque, uint8_t gray) {
    struct app_platform* context = opaque;
    memset(context->framebuffer, (int)((gray << 4U) | gray), FRAMEBUFFER_BYTES);
    return PLATFORM_OK;
}

static enum platform_status rectangle_display(void* opaque, int32_t x, int32_t y, int32_t width,
                                              int32_t height, uint8_t gray) {
    struct app_platform* context = opaque;
    if (width < 0 || height < 0) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    for (int32_t row = y < 0 ? 0 : y; row < y + height && row < FRAMEBUFFER_HEIGHT; ++row) {
        for (int32_t column = x < 0 ? 0 : x; column < x + width && column < FRAMEBUFFER_WIDTH;
             ++column) {
            uint8_t* value = pixel(context, column, row);
            if ((column & 1) == 0) {
                *value = (uint8_t)((*value & 0x0fU) | (gray << 4U));
            } else {
                *value = (uint8_t)((*value & 0xf0U) | gray);
            }
        }
    }
    return PLATFORM_OK;
}

static enum platform_status text_display(void* opaque, const struct font* font, int32_t x,
                                         int32_t y, const char* text, size_t length, uint8_t scale,
                                         uint8_t gray) {
    struct app_platform* context = opaque;
    int32_t origin = x;
    size_t position = 0U;
    while (position < length) {
        const struct font_glyph* glyph = font_lookup(font, next_codepoint(text, length, &position));
        if (glyph == NULL) {
            glyph = font_lookup(font, '?');
        }
        if (glyph == NULL) {
            continue;
        }
        const uint8_t* bitmap = &font->bitmap[glyph->bitmap_offset];
        const int32_t glyph_x = origin + (int32_t)glyph->bearing_x * scale;
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
        origin += (int32_t)glyph->advance * scale;
    }
    return PLATFORM_OK;
}

static enum platform_status present_display(void* opaque, bool partial) {
    struct app_platform* context = opaque;
    uint16_t min_x = FRAMEBUFFER_WIDTH;
    uint16_t min_y = FRAMEBUFFER_HEIGHT;
    uint16_t max_x = 0U;
    uint16_t max_y = 0U;
    if (!context->presented) {
        min_x = 0U;
        min_y = 0U;
        max_x = FRAMEBUFFER_WIDTH;
        max_y = FRAMEBUFFER_HEIGHT;
    } else {
        for (uint16_t row = 0U; row < FRAMEBUFFER_HEIGHT; ++row) {
            const size_t row_offset = (size_t)row * (FRAMEBUFFER_WIDTH / 2U);
            for (uint16_t column = 0U; column < FRAMEBUFFER_WIDTH; column += 2U) {
                const size_t offset = row_offset + column / 2U;
                if (context->framebuffer[offset] == context->previous_framebuffer[offset]) {
                    continue;
                }
                if (column < min_x) {
                    min_x = column;
                }
                if (row < min_y) {
                    min_y = row;
                }
                if (column + 2U > max_x) {
                    max_x = column + 2U;
                }
                if (row + 1U > max_y) {
                    max_y = row + 1U;
                }
            }
        }
    }
    if (min_x >= max_x || min_y >= max_y) {
        return PLATFORM_OK;
    }
    /* A dirty rect that merely touches the status bar or dock band only
     * covers the bytes that actually differ (e.g. clock glyph pixels), not
     * the band's unchanging background rows. Those rows would then never be
     * flagged dirty again and would never receive a genuine refresh. Widen
     * the rect to the whole band, full width, whenever it overlaps either
     * edge so the entire chrome element is redriven together. */
    if (min_y < CHROME_EDGE_MARGIN) {
        min_y = 0U;
        if (max_y < CHROME_EDGE_MARGIN) {
            max_y = CHROME_EDGE_MARGIN;
        }
        min_x = 0U;
        max_x = FRAMEBUFFER_WIDTH;
    }
    if (max_y > FRAMEBUFFER_HEIGHT - CHROME_EDGE_MARGIN) {
        max_y = FRAMEBUFFER_HEIGHT;
        if (min_y > FRAMEBUFFER_HEIGHT - CHROME_EDGE_MARGIN) {
            min_y = FRAMEBUFFER_HEIGHT - CHROME_EDGE_MARGIN;
        }
        min_x = 0U;
        max_x = FRAMEBUFFER_WIDTH;
    }
    min_x &= 0xfffcU;
    max_x = (uint16_t)((max_x + 3U) & 0xfffcU);
    if (max_x > FRAMEBUFFER_WIDTH) {
        max_x = FRAMEBUFFER_WIDTH;
    }
    const uint16_t width = max_x - min_x;
    const uint16_t height = max_y - min_y;
    const uint32_t area = (uint32_t)width * height;
    const bool touches_chrome_edge =
        min_y < CHROME_EDGE_MARGIN || max_y > FRAMEBUFFER_HEIGHT - CHROME_EDGE_MARGIN;
    const enum m5paper_it8951_update_mode mode =
        context->presented && (partial || area <= FRAMEBUFFER_WIDTH * 120U) && !touches_chrome_edge
            ? M5PAPER_IT8951_UPDATE_DU
            : M5PAPER_IT8951_UPDATE_GL16;
    const int64_t refresh_started_us = esp_timer_get_time();
    if (m5paper_it8951_present(&context->display, &context->display_info, context->framebuffer,
                               min_x, min_y, width, height, mode) != ESP_OK) {
        return PLATFORM_ERROR;
    }
    const int64_t refresh_us = esp_timer_get_time() - refresh_started_us;
    memcpy(context->previous_framebuffer, context->framebuffer, FRAMEBUFFER_BYTES);
    context->presented = true;
    ESP_LOGI(TAG, "refresh %ux%u at %u,%u using %s in %lldms", width, height, min_x, min_y,
             mode == M5PAPER_IT8951_UPDATE_DU ? "DU" : "GL16", (long long)(refresh_us / 1000));
    return PLATFORM_OK;
}

static enum platform_status next_event(void* opaque, struct platform_event* event) {
    struct app_platform* context = opaque;
    if (event == NULL) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    memset(event, 0, sizeof(*event));
    const gpio_num_t pins[] = {M5PAPER_DIAL_RIGHT, M5PAPER_DIAL_LEFT, M5PAPER_DIAL_PUSH};
    const int32_t buttons[] = {1073741906, 1073741905, 13};
    for (size_t index = 0U; index < sizeof(pins) / sizeof(pins[0]); ++index) {
        const int level = gpio_get_level(pins[index]);
        if (level != context->dial_levels[index]) {
            context->dial_levels[index] = level;
            event->type = PLATFORM_EVENT_BUTTON;
            event->button = buttons[index];
            event->pressed = level == 0;
            return PLATFORM_OK;
        }
    }
    struct m5paper_touch_point points[1] = {0};
    uint8_t count = 0U;
    if (m5paper_gt911_read(&context->touch, points, &count) == ESP_OK && count > 0U) {
        event->type = PLATFORM_EVENT_TOUCH_DOWN;
        event->x = points[0].x;
        event->y = points[0].y;
        return PLATFORM_OK;
    }
    return PLATFORM_OK;
}

static uint64_t monotonic_ms(void* opaque) {
    const struct app_platform* context = opaque;
    return (uint64_t)((esp_timer_get_time() - context->started_us) / 1000);
}

static void sleep_ms(void* opaque, uint32_t milliseconds) {
    (void)opaque;
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

static enum platform_status list_storage(void* opaque, const char* root, const char* relative,
                                         struct platform_entry* entries, uint32_t capacity,
                                         uint32_t* count) {
    struct app_platform* context = opaque;
    return m5paper_storage_list(&context->storage, root, relative, entries, capacity, count);
}

static enum platform_status open_archive(void* opaque, const char* root, const char* path,
                                         struct platform_archive* archive) {
    struct app_platform* context = opaque;
    return m5paper_storage_open_archive(&context->storage, root, path, archive);
}

static void close_archive(void* opaque, struct platform_archive* archive) {
    struct app_platform* context = opaque;
    m5paper_storage_close_archive(&context->storage, archive);
}

static enum platform_status unavailable_state(void* opaque, const char* root, const char* name,
                                              uint8_t* buffer, size_t capacity, size_t* length) {
    (void)opaque;
    (void)root;
    (void)name;
    (void)buffer;
    (void)capacity;
    (void)length;
    return PLATFORM_ERROR;
}

static enum platform_status unavailable_write(void* opaque, const char* root, const char* name,
                                              const uint8_t* data, size_t length) {
    (void)opaque;
    (void)root;
    (void)name;
    (void)data;
    (void)length;
    return PLATFORM_NOT_SUPPORTED;
}

esp_err_t esp_platform_create(struct platform* platform) {
    if (platform == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    struct app_platform* context = &app_context;
    memset(context, 0, sizeof(*context));
    context->framebuffer = heap_caps_malloc(FRAMEBUFFER_BYTES, MALLOC_CAP_SPIRAM);
    context->previous_framebuffer = heap_caps_malloc(FRAMEBUFFER_BYTES, MALLOC_CAP_SPIRAM);
    if (context->framebuffer == NULL || context->previous_framebuffer == NULL) {
        heap_caps_free(context->framebuffer);
        heap_caps_free(context->previous_framebuffer);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t status = m5paper_buses_init(&context->buses);
    if (status == ESP_OK) {
        status = m5paper_it8951_init(&context->buses, &context->display);
    }
    if (status == ESP_OK) {
        status = m5paper_it8951_system_info(&context->display, &context->display_info);
    }
    if (status == ESP_OK) {
        status = m5paper_gt911_init(&context->buses, &context->touch);
    }
    const gpio_config_t dial_config = {
        .pin_bit_mask = (1ULL << M5PAPER_DIAL_RIGHT) | (1ULL << M5PAPER_DIAL_LEFT) |
                        (1ULL << M5PAPER_DIAL_PUSH),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (status == ESP_OK) {
        status = gpio_config(&dial_config);
    }
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "application platform initialization failed: %s", esp_err_to_name(status));
        esp_platform_destroy(&(struct platform){.context = context});
        return status;
    }
    const esp_err_t storage_status = m5paper_storage_mount(&context->storage);
    if (storage_status == ESP_OK) {
        ESP_LOGI(TAG, "microSD mounted at /sdcard");
    } else {
        ESP_LOGW(TAG, "microSD unavailable: %s", esp_err_to_name(storage_status));
    }
    context->started_us = esp_timer_get_time();
    context->dial_levels[0] = gpio_get_level(M5PAPER_DIAL_RIGHT);
    context->dial_levels[1] = gpio_get_level(M5PAPER_DIAL_LEFT);
    context->dial_levels[2] = gpio_get_level(M5PAPER_DIAL_PUSH);
    memset(context->framebuffer, 0xff, FRAMEBUFFER_BYTES);
    memset(context->previous_framebuffer, 0xff, FRAMEBUFFER_BYTES);
    /* Flash black then white through GL16 once at boot so any ghosting left
     * over from the panel's prior analog state (e.g. a reset mid-refresh
     * during a reflash) is cleared before the first real frame is drawn. */
    (void)clear_display(context, 0x0U);
    (void)present_display(context, false);
    (void)clear_display(context, 0xfU);
    (void)present_display(context, false);
    /* Make the first real Lua-drawn frame a guaranteed full refresh rather
     * than a dirty-rect diff against this priming flash: rows that render
     * identically to the primed background (e.g. the status bar's empty
     * margin above the clock glyphs) would otherwise never be flagged as
     * changed and would never receive a genuine GL16 push with real
     * content. */
    context->presented = false;
    *platform = (struct platform){
        .context = context,
        .display = {.context = context,
                    .width = FRAMEBUFFER_WIDTH,
                    .height = FRAMEBUFFER_HEIGHT,
                    .clear = clear_display,
                    .rectangle = rectangle_display,
                    .text = text_display,
                    .present = present_display},
        .next_event = next_event,
        .monotonic_ms = monotonic_ms,
        .sleep_ms = sleep_ms,
        .list_storage = list_storage,
        .open_archive = open_archive,
        .close_archive = close_archive,
        .state_read = unavailable_state,
        .state_write = unavailable_write,
    };
    return ESP_OK;
}

void esp_platform_destroy(struct platform* platform) {
    if (platform == NULL || platform->context == NULL) {
        return;
    }
    struct app_platform* context = platform->context;
    m5paper_storage_unmount(&context->storage);
    m5paper_gt911_deinit(&context->touch);
    m5paper_it8951_deinit(&context->display);
    m5paper_buses_deinit(&context->buses);
    heap_caps_free(context->framebuffer);
    heap_caps_free(context->previous_framebuffer);
    memset(platform, 0, sizeof(*platform));
}
