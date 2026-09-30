#include "library_index.hpp"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "epub/book.hpp"
#include "epub/image.hpp"
#include "esp_heap_caps.h"

namespace xreader
{
namespace services
{
namespace library_index
{
namespace
{
static constexpr uint32_t cache_magic = 0x584C4942U; // XLIB
static constexpr uint16_t cache_version = 3;

struct cache_header_t
{
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    uint32_t checksum;
};

struct entry_v2_t
{
    char path[path_length];
    char title[title_length];
    char author[author_length];
    uint32_t file_size;
    int64_t modified_time;
    uint32_t last_read_order;
};

static void copy_text(char* destination, size_t capacity, const char* source)
{
    if (destination == nullptr || capacity == 0)
        return;
    if (source == nullptr)
    {
        destination[0] = '\0';
        return;
    }
    size_t length = strlen(source);
    if (length >= capacity)
        length = capacity - 1U;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static bool ends_with_epub(const char* name)
{
    if (name == nullptr)
        return false;
    const size_t length = strlen(name);
    return length >= 5U && name[length - 5U] == '.' &&
           tolower(static_cast<unsigned char>(name[length - 4U])) == 'e' &&
           tolower(static_cast<unsigned char>(name[length - 3U])) == 'p' &&
           tolower(static_cast<unsigned char>(name[length - 2U])) == 'u' &&
           tolower(static_cast<unsigned char>(name[length - 1U])) == 'b';
}

static const char* basename_of(const char* path)
{
    const char* result = path == nullptr ? "" : path;
    if (path == nullptr)
        return result;
    for (const char* cursor = path; *cursor != '\0'; ++cursor)
        if (*cursor == '/')
            result = cursor + 1;
    return result;
}

static int compare_ci(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0')
    {
        const int a = tolower(static_cast<unsigned char>(*left));
        const int b = tolower(static_cast<unsigned char>(*right));
        if (a != b)
            return a < b ? -1 : 1;
        ++left;
        ++right;
    }
    if (*left == *right)
        return 0;
    return *left == '\0' ? -1 : 1;
}

static bool contains_ci(const char* haystack, const char* needle)
{
    if (needle == nullptr || needle[0] == '\0')
        return true;
    if (haystack == nullptr)
        return false;
    const size_t needle_length = strlen(needle);
    for (const char* start = haystack; *start != '\0'; ++start)
    {
        size_t index = 0;
        while (index < needle_length && start[index] != '\0' &&
               tolower(static_cast<unsigned char>(start[index])) ==
                   tolower(static_cast<unsigned char>(needle[index])))
            ++index;
        if (index == needle_length)
            return true;
    }
    return false;
}

static void cache_path(const char* mount_path, char* output, size_t capacity)
{
    snprintf(output, capacity, "%s/.xreader-library-v1.idx", mount_path);
}

static uint32_t checksum_bytes(const void* data, size_t length)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t hash = 2166136261U;
    for (size_t index = 0; index < length; ++index)
    {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t checksum_entries(const entry_t* entries, uint16_t count)
{
    return checksum_bytes(entries, static_cast<size_t>(count) * sizeof(entry_t));
}

static uint32_t hash_text(const char* text)
{
    return checksum_bytes(text, text == nullptr ? 0U : strlen(text));
}

static void cache_cover(const char* mount_path, const char* book_path, const epub::book_t& metadata,
                        entry_t* entry)
{
    if (mount_path == nullptr || book_path == nullptr || entry == nullptr ||
        metadata.cover_href[0] == '\0')
        return;
    uint8_t* encoded = nullptr;
    size_t encoded_size = 0;
    if (epub::load_resource(book_path, metadata.cover_href, &encoded, &encoded_size) != ESP_OK ||
        encoded == nullptr || encoded_size == 0U || encoded_size > 2U * 1024U * 1024U)
    {
        heap_caps_free(encoded);
        return;
    }

    char directory[path_length] = {};
    const int directory_written =
        snprintf(directory, sizeof(directory), "%s/.xreader-covers", mount_path);
    if (directory_written <= 0 || static_cast<size_t>(directory_written) >= sizeof(directory))
    {
        heap_caps_free(encoded);
        return;
    }
    mkdir(directory, 0775);
    const int written = snprintf(entry->cover_cache, sizeof(entry->cover_cache), "%s/%08lx.img",
                                 directory, static_cast<unsigned long>(hash_text(book_path)));
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(entry->cover_cache))
    {
        entry->cover_cache[0] = '\0';
        heap_caps_free(encoded);
        return;
    }
    FILE* cover = fopen(entry->cover_cache, "wb");
    bool cover_ok = cover != nullptr;
    if (cover_ok)
        cover_ok = fwrite(encoded, 1, encoded_size, cover) == encoded_size;
    if (cover != nullptr && fclose(cover) != 0)
        cover_ok = false;
    if (!cover_ok)
    {
        unlink(entry->cover_cache);
        entry->cover_cache[0] = '\0';
        heap_caps_free(encoded);
        return;
    }
    epub::image::info_t info = {};
    if (epub::image::inspect(encoded, encoded_size, &info) == ESP_OK)
    {
        entry->cover_width = info.width;
        entry->cover_height = info.height;
        entry->cover_supported = info.supported;
    }
    heap_caps_free(encoded);
}

static bool catalog_files_unchanged(const catalog_t* catalog)
{
    for (uint16_t index = 0; index < catalog->count; ++index)
    {
        struct stat info = {};
        if (stat(catalog->entries[index].path, &info) != 0 || !S_ISREG(info.st_mode))
            return false;
        if (info.st_size < 0 || static_cast<uint64_t>(info.st_size) > UINT32_MAX)
            return false;
        if (catalog->entries[index].file_size != static_cast<uint32_t>(info.st_size) ||
            catalog->entries[index].modified_time != static_cast<int64_t>(info.st_mtime))
            return false;
    }
    return true;
}

static void add_file(const char* mount_path, const char* path, const struct stat& info,
                     catalog_t* catalog)
{
    if (catalog->count >= max_books || info.st_size < 0 ||
        static_cast<uint64_t>(info.st_size) > UINT32_MAX)
        return;
    entry_t& entry = catalog->entries[catalog->count];
    memset(&entry, 0, sizeof(entry));
    copy_text(entry.path, sizeof(entry.path), path);
    entry.file_size = static_cast<uint32_t>(info.st_size);
    entry.modified_time = static_cast<int64_t>(info.st_mtime);

    epub::book_t metadata = {};
    if (epub::load_metadata(path, &metadata) == ESP_OK)
    {
        copy_text(entry.title, sizeof(entry.title), metadata.title);
        copy_text(entry.author, sizeof(entry.author), metadata.author);
        cache_cover(mount_path, path, metadata, &entry);
    }
    if (entry.title[0] == '\0')
        copy_text(entry.title, sizeof(entry.title), basename_of(path));
    if (entry.author[0] == '\0')
        copy_text(entry.author, sizeof(entry.author), "UNKNOWN AUTHOR");
    ++catalog->count;
}

static void scan_directory(const char* mount_path, const char* directory, uint8_t depth,
                           catalog_t* catalog)
{
    if (depth > 5U || catalog->count >= max_books)
        return;
    DIR* handle = opendir(directory);
    if (handle == nullptr)
        return;
    struct dirent* item = nullptr;
    while (catalog->count < max_books && (item = readdir(handle)) != nullptr)
    {
        if (item->d_name[0] == '.')
            continue;
        char path[path_length] = {};
        const int written = snprintf(path, sizeof(path), "%s/%s", directory, item->d_name);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(path))
            continue;
        struct stat info = {};
        if (stat(path, &info) != 0)
            continue;
        if (S_ISDIR(info.st_mode))
            scan_directory(mount_path, path, static_cast<uint8_t>(depth + 1U), catalog);
        else if (S_ISREG(info.st_mode) && ends_with_epub(item->d_name))
            add_file(mount_path, path, info, catalog);
    }
    closedir(handle);
}

static bool comes_before(const entry_t& left, const entry_t& right, sort_mode_t mode)
{
    if (mode == sort_recent_read)
    {
        if (left.last_read_order != right.last_read_order)
            return left.last_read_order > right.last_read_order;
        return compare_ci(left.title, right.title) < 0;
    }
    if (mode == sort_recent_added)
    {
        if (left.modified_time != right.modified_time)
            return left.modified_time > right.modified_time;
        return compare_ci(left.title, right.title) < 0;
    }
    if (mode == sort_author)
    {
        const int author = compare_ci(left.author, right.author);
        if (author != 0)
            return author < 0;
    }
    return compare_ci(left.title, right.title) < 0;
}
} // namespace

void sort(catalog_t* catalog, sort_mode_t mode)
{
    if (catalog == nullptr)
        return;
    catalog->sort_mode = mode;
    for (uint16_t index = 1; index < catalog->count; ++index)
    {
        const entry_t current = catalog->entries[index];
        uint16_t position = index;
        while (position > 0U && comes_before(current, catalog->entries[position - 1U], mode))
        {
            catalog->entries[position] = catalog->entries[position - 1U];
            --position;
        }
        catalog->entries[position] = current;
    }
}

esp_err_t save(const char* mount_path, const catalog_t* catalog)
{
    if (mount_path == nullptr || catalog == nullptr || catalog->count > max_books)
        return ESP_ERR_INVALID_ARG;
    char path[path_length] = {};
    cache_path(mount_path, path, sizeof(path));
    char temporary[path_length + 8U] = {};
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    FILE* file = fopen(temporary, "wb");
    if (file == nullptr)
        return ESP_FAIL;
    const cache_header_t header = {
        .magic = cache_magic,
        .version = cache_version,
        .count = catalog->count,
        .checksum = checksum_entries(catalog->entries, catalog->count),
    };
    bool ok = fwrite(&header, 1, sizeof(header), file) == sizeof(header);
    const size_t bytes = static_cast<size_t>(catalog->count) * sizeof(entry_t);
    if (ok && bytes > 0U)
        ok = fwrite(catalog->entries, 1, bytes, file) == bytes;
    if (fclose(file) != 0)
        ok = false;
    if (!ok)
    {
        unlink(temporary);
        return ESP_FAIL;
    }
    unlink(path);
    if (rename(temporary, path) != 0)
    {
        unlink(temporary);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t rebuild(const char* mount_path, catalog_t* catalog)
{
    if (mount_path == nullptr || catalog == nullptr)
        return ESP_ERR_INVALID_ARG;
    catalog_t* previous = static_cast<catalog_t*>(
        heap_caps_calloc(1, sizeof(catalog_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (previous == nullptr)
        previous = static_cast<catalog_t*>(heap_caps_calloc(1, sizeof(catalog_t), MALLOC_CAP_8BIT));
    if (previous == nullptr)
        return ESP_ERR_NO_MEM;
    bool have_previous = false;
    char previous_path[path_length] = {};
    cache_path(mount_path, previous_path, sizeof(previous_path));
    FILE* previous_file = fopen(previous_path, "rb");
    if (previous_file != nullptr)
    {
        cache_header_t header = {};
        if (fread(&header, 1, sizeof(header), previous_file) == sizeof(header) &&
            header.magic == cache_magic && header.count <= max_books)
        {
            previous->count = header.count;
            if (header.version == cache_version)
            {
                const size_t bytes = static_cast<size_t>(header.count) * sizeof(entry_t);
                if (previous != nullptr &&
                    (bytes == 0U || fread(previous->entries, 1, bytes, previous_file) == bytes) &&
                    checksum_entries(previous->entries, previous->count) == header.checksum)
                    have_previous = true;
            }
            else if (header.version == 2U)
            {
                entry_v2_t* old_entries = static_cast<entry_v2_t*>(heap_caps_calloc(
                    max_books, sizeof(entry_v2_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
                if (old_entries == nullptr)
                    old_entries = static_cast<entry_v2_t*>(
                        heap_caps_calloc(max_books, sizeof(entry_v2_t), MALLOC_CAP_8BIT));
                const size_t bytes = static_cast<size_t>(header.count) * sizeof(entry_v2_t);
                if (old_entries != nullptr &&
                    (bytes == 0U || fread(old_entries, 1, bytes, previous_file) == bytes) &&
                    checksum_bytes(old_entries, bytes) == header.checksum)
                {
                    for (uint16_t i = 0; i < header.count; ++i)
                    {
                        copy_text(previous->entries[i].path, sizeof(previous->entries[i].path),
                                  old_entries[i].path);
                        copy_text(previous->entries[i].title, sizeof(previous->entries[i].title),
                                  old_entries[i].title);
                        copy_text(previous->entries[i].author, sizeof(previous->entries[i].author),
                                  old_entries[i].author);
                        previous->entries[i].file_size = old_entries[i].file_size;
                        previous->entries[i].modified_time = old_entries[i].modified_time;
                        previous->entries[i].last_read_order = old_entries[i].last_read_order;
                    }
                    have_previous = true;
                }
                heap_caps_free(old_entries);
            }
        }
        fclose(previous_file);
    }
    memset(catalog, 0, sizeof(*catalog));
    catalog->sort_mode = sort_title;

    // Scan the whole mounted card.  Restricting discovery to /books whenever that
    // directory existed made otherwise valid EPUBs at the card root (or in user
    // folders) disappear after the first import created /books. Hidden X-Reader
    // cache directories are already skipped by scan_directory().
    scan_directory(mount_path, mount_path, 0, catalog);
    if (have_previous)
    {
        for (uint16_t i = 0; i < catalog->count; ++i)
            for (uint16_t j = 0; j < previous->count; ++j)
                if (strcmp(catalog->entries[i].path, previous->entries[j].path) == 0)
                {
                    catalog->entries[i].last_read_order = previous->entries[j].last_read_order;
                    break;
                }
    }
    sort(catalog, sort_title);
    const esp_err_t result = save(mount_path, catalog);
    heap_caps_free(previous);
    return result;
}

esp_err_t load(const char* mount_path, catalog_t* catalog)
{
    if (mount_path == nullptr || catalog == nullptr)
        return ESP_ERR_INVALID_ARG;
    char path[path_length] = {};
    cache_path(mount_path, path, sizeof(path));
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
        return rebuild(mount_path, catalog);
    cache_header_t header = {};
    bool ok = fread(&header, 1, sizeof(header), file) == sizeof(header) &&
              header.magic == cache_magic && header.version == cache_version &&
              header.count <= max_books;
    memset(catalog, 0, sizeof(*catalog));
    if (ok)
    {
        const size_t bytes = static_cast<size_t>(header.count) * sizeof(entry_t);
        if (bytes > 0U)
            ok = fread(catalog->entries, 1, bytes, file) == bytes;
        catalog->count = header.count;
        catalog->sort_mode = sort_title;
        ok = ok && header.checksum == checksum_entries(catalog->entries, catalog->count);
    }
    fclose(file);
    // An empty cache is not authoritative: books may have been copied to the SD card
    // while the device was powered off. Rebuild so a stale zero-book index cannot keep
    // the Library permanently empty.
    if (!ok || catalog->count == 0U || !catalog_files_unchanged(catalog))
        return rebuild(mount_path, catalog);
    sort(catalog, sort_title);
    return ESP_OK;
}

void invalidate(const char* mount_path)
{
    if (mount_path == nullptr)
        return;
    char path[path_length] = {};
    cache_path(mount_path, path, sizeof(path));
    unlink(path);
}

size_t filter(const catalog_t* catalog, const char* query, uint16_t* indices, size_t capacity)
{
    if (catalog == nullptr || indices == nullptr || capacity == 0U)
        return 0;
    size_t count = 0;
    for (uint16_t index = 0; index < catalog->count && count < capacity; ++index)
    {
        const entry_t& entry = catalog->entries[index];
        if (contains_ci(entry.title, query) || contains_ci(entry.author, query) ||
            contains_ci(basename_of(entry.path), query))
            indices[count++] = index;
    }
    return count;
}

esp_err_t mark_read(const char* mount_path, catalog_t* catalog, const char* path)
{
    if (mount_path == nullptr || catalog == nullptr || path == nullptr)
        return ESP_ERR_INVALID_ARG;
    uint32_t next = 1U;
    for (uint16_t i = 0; i < catalog->count; ++i)
        if (catalog->entries[i].last_read_order >= next)
            next = catalog->entries[i].last_read_order + 1U;
    if (next == 0U)
    {
        next = 1U;
        for (uint16_t i = 0; i < catalog->count; ++i)
            catalog->entries[i].last_read_order = 0U;
    }
    for (uint16_t i = 0; i < catalog->count; ++i)
        if (strcmp(catalog->entries[i].path, path) == 0)
        {
            catalog->entries[i].last_read_order = next;
            return save(mount_path, catalog);
        }
    return ESP_ERR_NOT_FOUND;
}

} // namespace library_index
} // namespace services
} // namespace xreader
