#include "library.hpp"

#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"

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

static int compare_names(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0')
    {
        const int left_value = tolower(static_cast<unsigned char>(*left));
        const int right_value = tolower(static_cast<unsigned char>(*right));
        if (left_value != right_value)
            return left_value < right_value ? -1 : 1;
        ++left;
        ++right;
    }
    return *left == *right ? 0 : (*left == '\0' ? -1 : 1);
}

static void sort_books(book_list_t* books)
{
    for (uint8_t index = 1; index < books->count; ++index)
    {
        char name[max_name_length] = {};
        char title[max_name_length] = {};
        copy_name(name, books->names[index]);
        copy_name(title, books->titles[index]);
        uint8_t position = index;
        while (position > 0 && compare_names(name, books->names[position - 1]) < 0)
        {
            copy_name(books->names[position], books->names[position - 1]);
            copy_name(books->titles[position], books->titles[position - 1]);
            --position;
        }
        copy_name(books->names[position], name);
        copy_name(books->titles[position], title);
    }
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
        char entry_path[512] = {};
        const size_t path_length = strlen(path);
        if (path_length + 1 + strlen(entry->d_name) >= sizeof(entry_path))
            continue;
        memcpy(entry_path, path, path_length);
        entry_path[path_length] = '/';
        strcpy(entry_path + path_length + 1, entry->d_name);
        struct stat entry_stat = {};
        if (stat(entry_path, &entry_stat) != 0)
            continue;
        if (S_ISDIR(entry_stat.st_mode))
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

static void collect_books(const char* directory, char paths[][book_path_length], size_t capacity,
                          size_t* count, uint8_t depth)
{
    if (depth > 3 || *count >= capacity)
        return;
    DIR* handle = opendir(directory);
    if (handle == nullptr)
        return;
    struct dirent* entry = nullptr;
    while (*count < capacity && (entry = readdir(handle)) != nullptr)
    {
        if (entry->d_name[0] == '.')
            continue;
        char candidate[book_path_length] = {};
        const size_t directory_length = strlen(directory);
        const size_t entry_length = strlen(entry->d_name);
        if (directory_length + 1 + entry_length >= sizeof(candidate))
            continue;
        memcpy(candidate, directory, directory_length);
        candidate[directory_length] = '/';
        strcpy(candidate + directory_length + 1, entry->d_name);
        struct stat entry_stat = {};
        if (stat(candidate, &entry_stat) != 0)
            continue;
        if (S_ISDIR(entry_stat.st_mode))
        {
            collect_books(candidate, paths, capacity, count, static_cast<uint8_t>(depth + 1));
        }
        else if (is_epub(entry->d_name))
        {
            strcpy(paths[*count], candidate);
            ++*count;
        }
    }
    closedir(handle);
}

static void sort_paths(char paths[][book_path_length], size_t count)
{
    for (size_t index = 1; index < count; ++index)
    {
        char path[book_path_length] = {};
        strcpy(path, paths[index]);
        size_t position = index;
        while (position > 0 && compare_names(path, paths[position - 1]) < 0)
        {
            strcpy(paths[position], paths[position - 1]);
            --position;
        }
        strcpy(paths[position], path);
    }
}

} // namespace

void draw_library(gfx::framebuffer_t* framebuffer, bool storage_mounted, const char* mount_path)
{
    if (framebuffer == nullptr)
    {
        return;
    }

    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "LIBRARY", "SD");
    chrome::draw_indication_bar(framebuffer, "SELECT", "OPEN", "ROTARY");

    if (!storage_mounted)
    {
        gfx::draw_text(framebuffer, 40, 130, "NO SD CARD", 3, 0x00);
        return;
    }

    book_list_t books = {};
    if (mount_path == nullptr)
    {
        gfx::draw_text(framebuffer, 40, 130, "SD ERROR", 3, 0x00);
        return;
    }
    scan_directory(mount_path, &books, 0);
    sort_books(&books);
    if (books.count == 0)
    {
        gfx::draw_text(framebuffer, 40, 130, "NO EPUB BOOKS", 3, 0x00);
        return;
    }

    gfx::draw_text(framebuffer, 40, 70, "BOOKS", 2, 0x00);
    for (uint8_t index = 0; index < books.count; ++index)
    {
        const uint16_t y = static_cast<uint16_t>(108 + index * 42);
        gfx::draw_text(framebuffer, 40, y, books.titles[index], 2, 0x00);
        gfx::fill_rect(framebuffer, 40, y + 24, framebuffer->width - 80, 1, 0x04);
    }
}

bool find_first_book(const char* mount_path, char* path, size_t capacity)
{
    if (mount_path == nullptr || path == nullptr || capacity == 0)
        return false;
    char paths[max_books][book_path_length] = {};
    const size_t count = find_books(mount_path, paths, max_books);
    if (count == 0 || strlen(paths[0]) + 1 > capacity)
        return false;
    strcpy(path, paths[0]);
    return true;
}

size_t find_books(const char* mount_path, char paths[][book_path_length], size_t capacity)
{
    if (mount_path == nullptr || paths == nullptr || capacity == 0)
        return 0;
    size_t count = 0;
    collect_books(mount_path, paths, capacity, &count, 0);
    sort_paths(paths, count);
    return count;
}

} // namespace ui
} // namespace xreader
