#include "library.hpp"

#include <dirent.h>
#include <string.h>

#include "gfx/font.hpp"

namespace xreader
{
namespace ui
{

namespace
{

static constexpr uint8_t max_books = 8;
static constexpr size_t max_name_length = 48;

struct book_list_t
{
    uint8_t count;
    char names[max_books][max_name_length];
    char titles[max_books][max_name_length];
};

static bool is_epub(const char* name)
{
    const char* extension = nullptr;
    for (const char* cursor = name; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '.')
        {
            extension = cursor + 1;
        }
    }
    if (extension == nullptr)
    {
        return false;
    }
    const size_t extension_length = strlen(extension);
    if (extension_length != 3 && extension_length != 4)
    {
        return false;
    }
    return (extension[0] == 'e' || extension[0] == 'E') &&
           (extension[1] == 'p' || extension[1] == 'P') &&
           (extension[2] == 'u' || extension[2] == 'U') &&
           (extension_length == 3 ||
            ((extension[3] == 'b' || extension[3] == 'B') && extension[4] == '\0'));
}

static void copy_name(char* destination, const char* source)
{
    size_t index = 0;
    while (index + 1 < max_name_length && source[index] != '\0')
    {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static void scan_directory(const char* path, book_list_t* books, uint8_t depth)
{
    if (depth > 3 || books->count >= max_books)
        return;
    DIR* directory = opendir(path);
    if (directory == nullptr)
    {
        return;
    }

    struct dirent* entry = nullptr;
    while (books->count < max_books && (entry = readdir(directory)) != nullptr)
    {
        if (entry->d_name[0] == '.')
            continue;
        char entry_path[256] = {};
        const size_t path_length = strlen(path);
        if (path_length + 1 + strlen(entry->d_name) >= sizeof(entry_path))
            continue;
        memcpy(entry_path, path, path_length);
        entry_path[path_length] = '/';
        strcpy(entry_path + path_length + 1, entry->d_name);
        if (entry->d_type == DT_DIR)
        {
            scan_directory(entry_path, books, static_cast<uint8_t>(depth + 1));
            continue;
        }
        if (!is_epub(entry->d_name))
            continue;
        copy_name(books->names[books->count], entry->d_name);
        copy_name(books->titles[books->count], entry->d_name);
        ++books->count;
    }
    closedir(directory);
}

static bool find_book(const char* directory, char* path, size_t capacity, uint8_t depth)
{
    if (depth > 3)
        return false;
    DIR* handle = opendir(directory);
    if (handle == nullptr)
        return false;
    struct dirent* entry = nullptr;
    while ((entry = readdir(handle)) != nullptr)
    {
        if (entry->d_name[0] == '.')
            continue;
        char candidate[256] = {};
        const size_t length = strlen(directory);
        if (length + 1 + strlen(entry->d_name) >= sizeof(candidate))
            continue;
        memcpy(candidate, directory, length);
        candidate[length] = '/';
        strcpy(candidate + length + 1, entry->d_name);
        if (entry->d_type == DT_DIR)
        {
            if (find_book(candidate, path, capacity, static_cast<uint8_t>(depth + 1)))
            {
                closedir(handle);
                return true;
            }
        }
        else if (is_epub(entry->d_name) && strlen(candidate) + 1 <= capacity)
        {
            strcpy(path, candidate);
            closedir(handle);
            return true;
        }
    }
    closedir(handle);
    return false;
}

} // namespace

void draw_library(gfx::framebuffer_t* framebuffer, bool storage_mounted, const char* mount_path)
{
    if (framebuffer == nullptr)
    {
        return;
    }

    gfx::clear(framebuffer, 0x00);
    gfx::draw_rect(framebuffer, 0, 0, framebuffer->width, framebuffer->height, 0x0f);
    gfx::draw_text(framebuffer, 24, 20, "XREADER", 3, 0x0f);
    gfx::fill_rect(framebuffer, 24, 76, framebuffer->width - 48, 2, 0x0f);

    if (!storage_mounted)
    {
        gfx::draw_text(framebuffer, 40, 130, "NO SD CARD", 3, 0x0f);
        return;
    }

    book_list_t books = {};
    if (mount_path == nullptr)
    {
        gfx::draw_text(framebuffer, 40, 130, "SD ERROR", 3, 0x0f);
        return;
    }
    scan_directory(mount_path, &books, 0);
    if (books.count == 0)
    {
        gfx::draw_text(framebuffer, 40, 130, "NO EPUB BOOKS", 3, 0x0f);
        return;
    }

    gfx::draw_text(framebuffer, 40, 106, "BOOKS", 2, 0x0f);
    for (uint8_t index = 0; index < books.count; ++index)
    {
        const uint16_t y = static_cast<uint16_t>(150 + index * 42);
        gfx::draw_text(framebuffer, 40, y, books.titles[index], 2, 0x0f);
        gfx::fill_rect(framebuffer, 40, y + 24, framebuffer->width - 80, 1, 0x04);
    }
}

bool find_first_book(const char* mount_path, char* path, size_t capacity)
{
    if (mount_path == nullptr || path == nullptr || capacity == 0)
        return false;
    path[0] = '\0';
    return find_book(mount_path, path, capacity, 0);
}

} // namespace ui
} // namespace xreader
