#include "platform/sdl/platform.h"
#include "epub.h"
#include "font.h"
#include "zip.h"

#include <SDL.h>

#include <zlib.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

struct host_archive {
    FILE* file;
    struct zip_archive* archive;
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
        event->pressed = true;
        break;
    case SDL_KEYUP:
        event->type = PLATFORM_EVENT_BUTTON;
        event->button = (int32_t)sdl_event.key.keysym.sym;
        event->pressed = false;
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

static int entry_compare(const void* left, const void* right) {
    const struct platform_entry* a = left;
    const struct platform_entry* b = right;
    if (a->directory != b->directory)
        return a->directory ? -1 : 1;
    return strcmp(a->name, b->name);
}

static enum platform_status list_storage(void* opaque, const char* root, const char* relative,
                                         struct platform_entry* entries, uint32_t capacity,
                                         uint32_t* count) {
    (void)opaque;
    if (root == NULL || relative == NULL || entries == NULL || count == NULL) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    char directory[PLATFORM_PATH_MAX];
    const int written = snprintf(directory, sizeof(directory), "%s/%s", root, relative);
    if (written < 0 || (size_t)written >= sizeof(directory))
        return PLATFORM_INVALID_ARGUMENT;
    DIR* stream = opendir(directory);
    if (stream == NULL)
        return PLATFORM_ERROR;
    uint32_t found = 0U;
    struct dirent* item = NULL;
    while ((item = readdir(stream)) != NULL && found < capacity) {
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0)
            continue;
        char full_path[PLATFORM_PATH_MAX];
        const int full_written =
            snprintf(full_path, sizeof(full_path), "%s/%s", directory, item->d_name);
        if (full_written < 0 || (size_t)full_written >= sizeof(full_path))
            continue;
        struct stat file = {0};
        if (stat(full_path, &file) != 0)
            continue;
        const bool is_directory = S_ISDIR(file.st_mode);
        if (!is_directory && strstr(item->d_name, ".epub") == NULL)
            continue;
        const size_t name_length = strlen(item->d_name);
        if (name_length >= sizeof(entries[found].name))
            continue;
        struct platform_entry* entry = &entries[found++];
        memcpy(entry->name, item->d_name, name_length + 1U);
        epub_repair_mojibake(entry->name, sizeof(entry->name));
        if (relative[0] == '\0')
            snprintf(entry->path, sizeof(entry->path), "%s", item->d_name);
        else
            snprintf(entry->path, sizeof(entry->path), "%s/%s", relative, item->d_name);
        entry->size = is_directory ? 0U : (uint64_t)file.st_size;
        entry->directory = is_directory;
    }
    closedir(stream);
    qsort(entries, found, sizeof(*entries), entry_compare);
    *count = found;
    return PLATFORM_OK;
}

static enum zip_status archive_read_at(void* context, uint64_t offset, void* buffer,
                                       size_t length) {
    FILE* file = context;
    return fseek(file, (long)offset, SEEK_SET) == 0 && fread(buffer, 1U, length, file) == length
               ? ZIP_OK
               : ZIP_IO_ERROR;
}

static enum zip_status archive_inflate(void* context, uint64_t offset, size_t compressed_size,
                                       void* output, size_t output_size) {
    FILE* file = context;
    uint8_t* compressed = malloc(compressed_size == 0U ? 1U : compressed_size);
    if (compressed == NULL)
        return ZIP_OUT_OF_MEMORY;
    if (archive_read_at(file, offset, compressed, compressed_size) != ZIP_OK) {
        free(compressed);
        return ZIP_IO_ERROR;
    }
    z_stream stream = {0};
    stream.next_in = compressed;
    stream.avail_in = (uInt)compressed_size;
    stream.next_out = output;
    stream.avail_out = (uInt)output_size;
    const int result =
        inflateInit2(&stream, -MAX_WBITS) == Z_OK ? inflate(&stream, Z_FINISH) : Z_STREAM_ERROR;
    const bool valid = result == Z_STREAM_END && stream.total_out == output_size;
    inflateEnd(&stream);
    free(compressed);
    return valid ? ZIP_OK : ZIP_MALFORMED;
}

static enum platform_status open_archive(void* opaque, const char* root, const char* path,
                                         struct platform_archive* result) {
    (void)opaque;
    if (root == NULL || path == NULL || result == NULL)
        return PLATFORM_INVALID_ARGUMENT;
    char full_path[PLATFORM_PATH_MAX];
    const int written = snprintf(full_path, sizeof(full_path), "%s/%s", root, path);
    if (written < 0 || (size_t)written >= sizeof(full_path))
        return PLATFORM_INVALID_ARGUMENT;
    FILE* file = fopen(full_path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != NULL)
            fclose(file);
        return PLATFORM_ERROR;
    }
    const long end = ftell(file);
    if (end < 0) {
        fclose(file);
        return PLATFORM_ERROR;
    }
    struct host_archive* host = calloc(1U, sizeof(*host));
    if (host == NULL) {
        fclose(file);
        return PLATFORM_ERROR;
    }
    host->file = file;
    const struct zip_source source = {
        .context = file,
        .size = (uint64_t)end,
        .read_at = archive_read_at,
        .inflate_raw = archive_inflate,
    };
    const struct zip_limits limits = {
        .max_entries = 4096U,
        .max_name_bytes = 1024U,
        .max_entry_uncompressed_bytes = 16U * 1024U * 1024U,
        .max_total_uncompressed_bytes = 128U * 1024U * 1024U,
        .max_read_all_bytes = 4U * 1024U * 1024U,
    };
    if (zip_open(&source, &limits, &host->archive) != ZIP_OK) {
        fclose(file);
        free(host);
        return PLATFORM_ERROR;
    }
    result->archive = host->archive;
    result->context = host;
    return PLATFORM_OK;
}

static void close_archive(void* opaque, struct platform_archive* archive) {
    (void)opaque;
    if (archive == NULL || archive->context == NULL)
        return;
    struct host_archive* host = archive->context;
    zip_close(host->archive);
    fclose(host->file);
    free(host);
    archive->archive = NULL;
    archive->context = NULL;
}

static enum platform_status state_read(void* opaque, const char* root, const char* name,
                                       uint8_t* buffer, size_t capacity, size_t* length) {
    (void)opaque;
    if (root == NULL || name == NULL || buffer == NULL || length == NULL)
        return PLATFORM_INVALID_ARGUMENT;
    char path[PLATFORM_PATH_MAX];
    const int written = snprintf(path, sizeof(path), "%s/%s", root, name);
    if (written < 0 || (size_t)written >= sizeof(path))
        return PLATFORM_INVALID_ARGUMENT;
    FILE* file = fopen(path, "rb");
    if (file == NULL)
        return PLATFORM_ERROR;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return PLATFORM_ERROR;
    }
    const long end = ftell(file);
    if (end < 0 || (unsigned long)end > capacity) {
        fclose(file);
        return PLATFORM_ERROR;
    }
    rewind(file);
    const size_t expected = (size_t)end;
    const bool valid = fread(buffer, 1U, expected, file) == expected;
    fclose(file);
    if (!valid)
        return PLATFORM_ERROR;
    *length = expected;
    return PLATFORM_OK;
}

static enum platform_status state_write(void* opaque, const char* root, const char* name,
                                        const uint8_t* data, size_t length) {
    (void)opaque;
    if (root == NULL || name == NULL || data == NULL)
        return PLATFORM_INVALID_ARGUMENT;
    if (mkdir(root, 0755) != 0 && errno != EEXIST)
        return PLATFORM_ERROR;
    char temporary[PLATFORM_PATH_MAX];
    char destination[PLATFORM_PATH_MAX];
    const int temporary_written = snprintf(temporary, sizeof(temporary), "%s/%s.tmp", root, name);
    const int destination_written = snprintf(destination, sizeof(destination), "%s/%s", root, name);
    if (temporary_written < 0 || destination_written < 0 ||
        (size_t)temporary_written >= sizeof(temporary) ||
        (size_t)destination_written >= sizeof(destination))
        return PLATFORM_INVALID_ARGUMENT;
    FILE* file = fopen(temporary, "wb");
    if (file == NULL)
        return PLATFORM_ERROR;
    const bool written = fwrite(data, 1U, length, file) == length && fflush(file) == 0;
    const int descriptor = written ? open(temporary, O_RDONLY) : -1;
    const bool synced = !written || (descriptor >= 0 && fsync(descriptor) == 0);
    if (descriptor >= 0)
        close(descriptor);
    if (fclose(file) != 0 || !written || !synced) {
        remove(temporary);
        return PLATFORM_ERROR;
    }
    if (rename(temporary, destination) != 0) {
        remove(temporary);
        return PLATFORM_ERROR;
    }
    return PLATFORM_OK;
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
        .width = DISPLAY_WIDTH,
        .height = DISPLAY_HEIGHT,
        .clear = clear_display,
        .rectangle = draw_rectangle,
        .text = draw_text,
        .present = present_display,
    };
    platform->next_event = next_event;
    platform->monotonic_ms = monotonic_ms;
    platform->sleep_ms = sleep_ms;
    platform->list_storage = list_storage;
    platform->open_archive = open_archive;
    platform->close_archive = close_archive;
    platform->state_read = state_read;
    platform->state_write = state_write;
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
