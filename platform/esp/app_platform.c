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
#include <freertos/queue.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

enum {
    FRAMEBUFFER_WIDTH = 540,
    FRAMEBUFFER_HEIGHT = 960,
    FRAMEBUFFER_BYTES = FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT / 2,
    /* Covers the status bar (48px) and dock (64px) chrome rows defined in
     * lua/layout.lua, plus the reading scene's own header/footer bands. Any
     * refresh touching these rows is widened to cover the whole band so the
     * band's unchanging background pixels (which never show up in a byte
     * diff) still get redriven along with whatever glyph actually changed. */
    CHROME_EDGE_MARGIN = 64,
    /* Each physical click produces two queued events (press, then release),
     * and each one costs a full redraw cycle (several hundred ms on this
     * panel) to process. A deep queue would let a user who taps rapidly
     * build up a backlog that keeps the display turning pages/highlights
     * for seconds after they've stopped touching the device. Capping it at
     * two clicks' worth keeps a short, deliberate double-tap intact while
     * dropping further spam instead of queuing it indefinitely -- xQueueSend
     * already fails silently once full (see input_task_main), so excess
     * events are simply not observed, not buffered. */
    INPUT_QUEUE_CAPACITY = 4,
    /* Minimum spacing, in microseconds, between recognized button
     * transitions. The e-ink panel can't keep up with a user mashing the
     * dial faster than it can redraw; this throttles recognition at the
     * source rather than just bounding how much backlog can queue up. */
    BUTTON_DEBOUNCE_US = 150000,
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
    int64_t last_button_event_us;
    uint32_t refresh_count;
    bool top_band_primed;
    bool bottom_band_primed;
    QueueHandle_t input_queue;
    TaskHandle_t input_task;
    /* When true, canvas.* draws into a logical FRAMEBUFFER_HEIGHT x
     * FRAMEBUFFER_WIDTH canvas that gets rotated 90 degrees into the
     * physical (native-orientation) framebuffer -- the IT8951 panel itself
     * is wired at a single fixed native resolution and cannot be resized,
     * so landscape support has to happen as a per-pixel rotation here
     * rather than by reallocating anything. */
    bool landscape;
};

static const char* const TAG = "x-reader-app";
static struct app_platform app_context;

struct pixel_ref {
    uint8_t* byte;
    bool high_nibble;
};

static struct pixel_ref pixel_ref(struct app_platform* context, int32_t x, int32_t y) {
    int32_t physical_x = x;
    int32_t physical_y = y;
    if (context->landscape) {
        physical_x = FRAMEBUFFER_WIDTH - 1 - y;
        physical_y = x;
    }
    struct pixel_ref ref = {
        .byte = &context->framebuffer[(size_t)physical_y * (FRAMEBUFFER_WIDTH / 2U) +
                                      (size_t)physical_x / 2U],
        .high_nibble = (physical_x & 1) == 0,
    };
    return ref;
}

static int32_t logical_width(const struct app_platform* context) {
    return context->landscape ? FRAMEBUFFER_HEIGHT : FRAMEBUFFER_WIDTH;
}

static int32_t logical_height(const struct app_platform* context) {
    return context->landscape ? FRAMEBUFFER_WIDTH : FRAMEBUFFER_HEIGHT;
}

static void blend_pixel(struct app_platform* context, int32_t x, int32_t y, uint8_t gray,
                        uint8_t alpha) {
    if (x < 0 || x >= logical_width(context) || y < 0 || y >= logical_height(context) ||
        alpha == 0U) {
        return;
    }
    struct pixel_ref ref = pixel_ref(context, x, y);
    const uint8_t current = ref.high_nibble ? *ref.byte >> 4U : *ref.byte & 0x0fU;
    const uint16_t blended = ((uint16_t)current * (255U - alpha) + (uint16_t)gray * alpha) / 255U;
    if (ref.high_nibble) {
        *ref.byte = (uint8_t)((*ref.byte & 0x0fU) | ((uint8_t)blended << 4U));
    } else {
        *ref.byte = (uint8_t)((*ref.byte & 0xf0U) | (uint8_t)blended);
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

static enum platform_status set_orientation_display(void* opaque, bool landscape,
                                                    int32_t* out_width, int32_t* out_height) {
    struct app_platform* context = opaque;
    if (context->landscape != landscape) {
        context->landscape = landscape;
        /* The physical panel keeps whatever was last presented in the old
         * orientation; nothing in the new logical framebuffer has actually
         * changed relative to it, so the dirty-rect diff in present_display
         * would think most of the screen is still correct. Force the next
         * present to redraw everything. */
        context->presented = false;
        context->top_band_primed = false;
        context->bottom_band_primed = false;
    }
    *out_width = logical_width(context);
    *out_height = logical_height(context);
    return PLATFORM_OK;
}

static enum platform_status rectangle_display(void* opaque, int32_t x, int32_t y, int32_t width,
                                              int32_t height, uint8_t gray) {
    struct app_platform* context = opaque;
    if (width < 0 || height < 0) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    const int32_t limit_width = logical_width(context);
    const int32_t limit_height = logical_height(context);
    for (int32_t row = y < 0 ? 0 : y; row < y + height && row < limit_height; ++row) {
        for (int32_t column = x < 0 ? 0 : x; column < x + width && column < limit_width; ++column) {
            struct pixel_ref ref = pixel_ref(context, column, row);
            if (ref.high_nibble) {
                *ref.byte = (uint8_t)((*ref.byte & 0x0fU) | (gray << 4U));
            } else {
                *ref.byte = (uint8_t)((*ref.byte & 0xf0U) | gray);
            }
        }
    }
    return PLATFORM_OK;
}

static enum platform_status text_display(void* opaque, const struct font* font, int32_t x,
                                         int32_t y, const char* text, size_t length, uint8_t scale,
                                         uint8_t gray, bool rotated) {
    struct app_platform* context = opaque;
    size_t position = 0U;
    if (!rotated) {
        int32_t origin = x;
        while (position < length) {
            const struct font_glyph* glyph =
                font_lookup(font, next_codepoint(text, length, &position));
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
    /* Vertical layout: characters stack downward from y, each glyph rotated
     * 90 degrees clockwise as a rigid block and centered horizontally on
     * x. Per-glyph bearings are ignored here -- they only matter for tight
     * horizontal typesetting, not short rail labels. */
    int32_t origin = y;
    while (position < length) {
        const struct font_glyph* glyph = font_lookup(font, next_codepoint(text, length, &position));
        if (glyph == NULL) {
            glyph = font_lookup(font, '?');
        }
        if (glyph == NULL) {
            continue;
        }
        const uint8_t* bitmap = &font->bitmap[glyph->bitmap_offset];
        const int32_t block_left = x - ((int32_t)glyph->height * scale) / 2;
        for (uint16_t row = 0U; row < glyph->height; ++row) {
            for (uint16_t column = 0U; column < glyph->width; ++column) {
                const uint8_t alpha = bitmap[(size_t)row * glyph->width + column];
                const int32_t rotated_x = block_left + (int32_t)(glyph->height - 1U - row) * scale;
                const int32_t rotated_y = origin + (int32_t)column * scale;
                for (uint8_t y_offset = 0U; y_offset < scale; ++y_offset) {
                    for (uint8_t x_offset = 0U; x_offset < scale; ++x_offset) {
                        blend_pixel(context, rotated_x + x_offset, rotated_y + y_offset, gray,
                                    alpha);
                    }
                }
            }
        }
        origin += (int32_t)glyph->advance * scale;
    }
    return PLATFORM_OK;
}

static enum platform_status present_display(void* opaque, bool partial, bool high_quality) {
    const int64_t present_started_us = esp_timer_get_time();
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
        /* Both buffers live in PSRAM, where scattered single-byte access is
         * markedly slower than sequential bulk access. memcmp() over the
         * whole row first lets the toolchain's optimized implementation
         * rule out the common case (an unchanged row) in one bulk compare
         * instead of up to 270 individual byte reads; the byte-wise scan
         * below only runs for rows that actually differ. */
        for (uint16_t row = 0U; row < FRAMEBUFFER_HEIGHT; ++row) {
            const size_t row_offset = (size_t)row * (FRAMEBUFFER_WIDTH / 2U);
            if (memcmp(&context->framebuffer[row_offset],
                       &context->previous_framebuffer[row_offset], FRAMEBUFFER_WIDTH / 2U) == 0) {
                continue;
            }
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
    const bool touches_top = min_y < CHROME_EDGE_MARGIN;
    const bool touches_bottom = max_y > FRAMEBUFFER_HEIGHT - CHROME_EDGE_MARGIN;
    /* The first time a chrome band is touched after boot, widen the rect to
     * its full extent, full width: that one-time pass ensures rows which
     * are always background (and so never show up in a byte diff) still get
     * painted. Once primed, later touches -- e.g. a dock highlight moving
     * from one button to another -- keep the diff's natural, much smaller
     * extent so the refresh only covers the button rects that changed. */
    if (touches_top) {
        if (!context->top_band_primed) {
            min_y = 0U;
            if (max_y < CHROME_EDGE_MARGIN) {
                max_y = CHROME_EDGE_MARGIN;
            }
            min_x = 0U;
            max_x = FRAMEBUFFER_WIDTH;
        }
        context->top_band_primed = true;
    }
    if (touches_bottom) {
        if (!context->bottom_band_primed) {
            max_y = FRAMEBUFFER_HEIGHT;
            if (min_y > FRAMEBUFFER_HEIGHT - CHROME_EDGE_MARGIN) {
                min_y = FRAMEBUFFER_HEIGHT - CHROME_EDGE_MARGIN;
            }
            min_x = 0U;
            max_x = FRAMEBUFFER_WIDTH;
        }
        context->bottom_band_primed = true;
    }
    min_x &= 0xfffcU;
    max_x = (uint16_t)((max_x + 3U) & 0xfffcU);
    if (max_x > FRAMEBUFFER_WIDTH) {
        max_x = FRAMEBUFFER_WIDTH;
    }
    uint16_t width = max_x - min_x;
    uint16_t height = max_y - min_y;
    uint32_t area = (uint32_t)width * height;
    /* DU doesn't reliably redraw the dock/status chrome on this panel (even
     * for a single highlight move), so any refresh touching those bands
     * always uses GL16 -- still only over the (now possibly much smaller,
     * primed) rect, not a full-screen refresh. Dialogs request the same
     * via high_quality. */
    enum m5paper_it8951_update_mode mode;
    if (high_quality || touches_top || touches_bottom) {
        mode = M5PAPER_IT8951_UPDATE_GL16;
    } else if (context->presented && (partial || area <= FRAMEBUFFER_WIDTH * 120U)) {
        mode = M5PAPER_IT8951_UPDATE_DU;
    } else {
        mode = M5PAPER_IT8951_UPDATE_GL16;
    }
    /* DU refreshes never fully reset a pixel's analog state, so a run of
     * frequent small updates gradually builds up ghosting. Count only
     * consecutive DU refreshes; any GL16/GC16 pass already redrives its
     * pixels properly and resets the count. After 15 DU refreshes in a row,
     * force a full-panel pass regardless of what the current update would
     * otherwise have covered. GC16 is the deepest clean but, measured on
     * hardware, can take several *seconds* over a content-heavy full
     * screen (vs. ~400-700ms for GL16) -- long enough to feel like a
     * freeze if it lands mid-read. GL16 already fully redrives every
     * pixel's waveform, so it resets DU ghosting just as reliably; it's
     * just not quite as deep a clean as GC16. Use it here to keep the
     * periodic refresh bounded in time. */
    if (mode == M5PAPER_IT8951_UPDATE_DU) {
        context->refresh_count++;
    } else {
        context->refresh_count = 0U;
    }
    if (context->refresh_count >= 15U) {
        context->refresh_count = 0U;
        mode = M5PAPER_IT8951_UPDATE_GL16;
        min_x = 0U;
        min_y = 0U;
        max_x = FRAMEBUFFER_WIDTH;
        max_y = FRAMEBUFFER_HEIGHT;
        width = max_x - min_x;
        height = max_y - min_y;
        area = (uint32_t)width * height;
    }
    const int64_t refresh_started_us = esp_timer_get_time();
    if (m5paper_it8951_present(&context->display, &context->display_info, context->framebuffer,
                               min_x, min_y, width, height, mode) != ESP_OK) {
        return PLATFORM_ERROR;
    }
    const int64_t refresh_us = esp_timer_get_time() - refresh_started_us;
    memcpy(context->previous_framebuffer, context->framebuffer, FRAMEBUFFER_BYTES);
    context->presented = true;
    const char* mode_name = mode == M5PAPER_IT8951_UPDATE_DU     ? "DU"
                            : mode == M5PAPER_IT8951_UPDATE_GC16 ? "GC16"
                                                                 : "GL16";
    const int64_t total_us = esp_timer_get_time() - present_started_us;
    ESP_LOGI(TAG, "refresh %ux%u at %u,%u using %s: spi %lldms, total %lldms (diff+copy %lldms)",
             width, height, min_x, min_y, mode_name, (long long)(refresh_us / 1000),
             (long long)(total_us / 1000), (long long)((total_us - refresh_us) / 1000));
    return PLATFORM_OK;
}

/* Polls the dial GPIOs and touch controller directly; only ever called from
 * the dedicated input task (see input_task_main below), never from the main
 * task, so dial_levels and the touch I2C transaction have a single owner. */
static enum platform_status poll_input(struct app_platform* context, struct platform_event* event) {
    memset(event, 0, sizeof(*event));
    const gpio_num_t pins[] = {M5PAPER_DIAL_RIGHT, M5PAPER_DIAL_LEFT, M5PAPER_DIAL_PUSH};
    const int32_t buttons[] = {1073741906, 1073741905, 13};
    const int64_t now_us = esp_timer_get_time();
    for (size_t index = 0U; index < sizeof(pins) / sizeof(pins[0]); ++index) {
        const int level = gpio_get_level(pins[index]);
        /* Within the debounce window, leave dial_levels untouched rather
         * than dropping the transition outright: the next poll keeps
         * re-checking against the same recorded level, so once the window
         * clears we recognize whatever the pin's current state is. Any
         * bounces in between are coalesced into that one transition
         * instead of each being queued as its own event. */
        if (level != context->dial_levels[index] &&
            now_us - context->last_button_event_us >= BUTTON_DEBOUNCE_US) {
            context->dial_levels[index] = level;
            context->last_button_event_us = now_us;
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
        /* The GT911 reports raw physical panel coordinates; invert the
         * drawing rotation (see pixel_ref) so Lua always sees touches in
         * the same logical space it draws into. */
        if (context->landscape) {
            event->x = points[0].y;
            event->y = FRAMEBUFFER_WIDTH - 1 - points[0].x;
        } else {
            event->x = points[0].x;
            event->y = points[0].y;
        }
        return PLATFORM_OK;
    }
    return PLATFORM_OK;
}

/* Polls continuously, independent of whatever the main task is doing (Lua
 * execution, a blocking SPI present, EPUB parsing). Without this, a button
 * press during a multi-hundred-millisecond screen refresh would simply not
 * be observed until the refresh finished. */
static void input_task_main(void* parameter) {
    struct app_platform* context = parameter;
    while (true) {
        struct platform_event event;
        if (poll_input(context, &event) == PLATFORM_OK && event.type != PLATFORM_EVENT_NONE) {
            (void)xQueueSend(context->input_queue, &event, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static enum platform_status next_event(void* opaque, struct platform_event* event) {
    struct app_platform* context = opaque;
    if (event == NULL) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    if (xQueueReceive(context->input_queue, event, 0) == pdTRUE) {
        return PLATFORM_OK;
    }
    memset(event, 0, sizeof(*event));
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

static enum platform_status state_read(void* opaque, const char* root, const char* name,
                                       uint8_t* buffer, size_t capacity, size_t* length) {
    struct app_platform* context = opaque;
    return m5paper_storage_state_read(&context->storage, root, name, buffer, capacity, length);
}

static enum platform_status state_write(void* opaque, const char* root, const char* name,
                                        const uint8_t* data, size_t length) {
    struct app_platform* context = opaque;
    return m5paper_storage_state_write(&context->storage, root, name, data, length);
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
    (void)present_display(context, false, false);
    (void)clear_display(context, 0xfU);
    (void)present_display(context, false, false);
    /* Make the first real Lua-drawn frame a guaranteed full refresh rather
     * than a dirty-rect diff against this priming flash: rows that render
     * identically to the primed background (e.g. the status bar's empty
     * margin above the clock glyphs) would otherwise never be flagged as
     * changed and would never receive a genuine GL16 push with real
     * content. */
    context->presented = false;
    context->input_queue = xQueueCreate(INPUT_QUEUE_CAPACITY, sizeof(struct platform_event));
    if (context->input_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    /* Runs independently of the main task so a button press is still
     * observed while the main task is blocked in a multi-hundred-ms SPI
     * present or parsing an EPUB chapter -- that only requires the input
     * task to be *ready* to run while main is blocked/yielded, which
     * FreeRTOS already guarantees for any task regardless of priority.
     * Equal priority to the main task (ESP-IDF's default of 1) means it's
     * scheduled via normal time-slicing instead of hard-preempting active
     * rendering computation every 10ms, which measurably slowed draws when
     * this ran at a higher priority. */
    if (xTaskCreate(input_task_main, "x-reader-input", 4096, context, 1, &context->input_task) !=
        pdPASS) {
        vQueueDelete(context->input_queue);
        context->input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    *platform = (struct platform){
        .context = context,
        .display = {.context = context,
                    .width = FRAMEBUFFER_WIDTH,
                    .height = FRAMEBUFFER_HEIGHT,
                    .clear = clear_display,
                    .rectangle = rectangle_display,
                    .text = text_display,
                    .present = present_display,
                    .set_orientation = set_orientation_display},
        .next_event = next_event,
        .monotonic_ms = monotonic_ms,
        .sleep_ms = sleep_ms,
        .list_storage = list_storage,
        .open_archive = open_archive,
        .close_archive = close_archive,
        .state_read = state_read,
        .state_write = state_write,
    };
    return ESP_OK;
}

void esp_platform_destroy(struct platform* platform) {
    if (platform == NULL || platform->context == NULL) {
        return;
    }
    struct app_platform* context = platform->context;
    if (context->input_task != NULL) {
        vTaskDelete(context->input_task);
        context->input_task = NULL;
    }
    if (context->input_queue != NULL) {
        vQueueDelete(context->input_queue);
        context->input_queue = NULL;
    }
    m5paper_storage_unmount(&context->storage);
    m5paper_gt911_deinit(&context->touch);
    m5paper_it8951_deinit(&context->display);
    m5paper_buses_deinit(&context->buses);
    heap_caps_free(context->framebuffer);
    heap_caps_free(context->previous_framebuffer);
    memset(platform, 0, sizeof(*platform));
}
