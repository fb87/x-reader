#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace services
{
namespace library_index
{

// XTeink X4 has no PSRAM, so its catalog_t (entries[max_books]) has to fit
// inside the ~139KB internal RAM region alongside the display buffers and
// WiFi stack; a full 32-entry catalog (~23KB) doesn't leave enough headroom.
#if defined(XREADER_BOARD_XTEINK)
static constexpr size_t max_books = 8;
#else
static constexpr size_t max_books = 32;
#endif
static constexpr size_t path_length = 256;
static constexpr size_t title_length = 96;
static constexpr size_t author_length = 80;

enum sort_mode_t : uint8_t
{
    sort_title,
    sort_author,
    sort_recent_added,
    sort_recent_read,
};

struct entry_t
{
    char path[path_length];
    char title[title_length];
    char author[author_length];
    uint32_t file_size;
    int64_t modified_time;
    uint32_t last_read_order;
    char cover_cache[path_length];
    uint16_t cover_width;
    uint16_t cover_height;
    bool cover_supported;
};

struct catalog_t
{
    uint16_t count;
    sort_mode_t sort_mode;
    entry_t entries[max_books];
};

esp_err_t load(const char* mount_path, catalog_t* catalog);
esp_err_t rebuild(const char* mount_path, catalog_t* catalog);
esp_err_t save(const char* mount_path, const catalog_t* catalog);
void invalidate(const char* mount_path);
void sort(catalog_t* catalog, sort_mode_t mode);
size_t filter(const catalog_t* catalog, const char* query, uint16_t* indices, size_t capacity);
esp_err_t mark_read(const char* mount_path, catalog_t* catalog, const char* path);

} // namespace library_index
} // namespace services
} // namespace xreader
