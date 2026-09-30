#include "file_browser.hpp"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

namespace xreader
{
namespace services
{
namespace file_browser
{
namespace
{
static void copy_text(char* destination, size_t capacity, const char* source)
{
    if (destination == nullptr || capacity == 0U)
        return;
    if (source == nullptr)
    {
        destination[0] = '\0';
        return;
    }
    const size_t length = strlen(source) < capacity - 1U ? strlen(source) : capacity - 1U;
    memcpy(destination, source, length);
    destination[length] = '\0';
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

static bool ends_with_epub(const char* name)
{
    if (name == nullptr)
        return false;
    const size_t n = strlen(name);
    return n >= 5U && name[n - 5U] == '.' &&
           tolower(static_cast<unsigned char>(name[n - 4U])) == 'e' &&
           tolower(static_cast<unsigned char>(name[n - 3U])) == 'p' &&
           tolower(static_cast<unsigned char>(name[n - 2U])) == 'u' &&
           tolower(static_cast<unsigned char>(name[n - 1U])) == 'b';
}

static bool path_under_root(const char* root, const char* path)
{
    if (root == nullptr || path == nullptr)
        return false;
    const size_t root_length = strlen(root);
    if (root_length == 0U || strncmp(root, path, root_length) != 0)
        return false;
    return path[root_length] == '\0' || path[root_length] == '/';
}

static void sort_entries(listing_t* listing)
{
    for (uint8_t index = 1; index < listing->count; ++index)
    {
        const entry_t current = listing->entries[index];
        uint8_t position = index;
        while (position > 0U)
        {
            const entry_t& previous = listing->entries[position - 1U];
            const bool before = current.directory != previous.directory
                                    ? current.directory
                                    : compare_ci(current.name, previous.name) < 0;
            if (!before)
                break;
            listing->entries[position] = previous;
            --position;
        }
        listing->entries[position] = current;
    }
}
} // namespace

esp_err_t open(const char* root, const char* path, listing_t* listing)
{
    if (root == nullptr || listing == nullptr)
        return ESP_ERR_INVALID_ARG;
    const char* requested = path == nullptr || path[0] == '\0' ? root : path;
    if (!path_under_root(root, requested))
        return ESP_ERR_INVALID_ARG;

    char root_copy[path_length] = {};
    char path_copy[path_length] = {};
    copy_text(root_copy, sizeof(root_copy), root);
    copy_text(path_copy, sizeof(path_copy), requested);
    DIR* directory = opendir(path_copy);
    if (directory == nullptr)
        return ESP_ERR_NOT_FOUND;

    memset(listing, 0, sizeof(*listing));
    copy_text(listing->root, sizeof(listing->root), root_copy);
    copy_text(listing->path, sizeof(listing->path), path_copy);
    requested = listing->path;

    struct dirent* item = nullptr;
    while (listing->count < max_entries && (item = readdir(directory)) != nullptr)
    {
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0 ||
            item->d_name[0] == '.')
            continue;
        char item_path[path_length] = {};
        const int written =
            snprintf(item_path, sizeof(item_path), "%s/%s", requested, item->d_name);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(item_path))
            continue;
        struct stat info = {};
        if (stat(item_path, &info) != 0)
            continue;
        if (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode))
            continue;

        entry_t& entry = listing->entries[listing->count++];
        copy_text(entry.name, sizeof(entry.name), item->d_name);
        copy_text(entry.path, sizeof(entry.path), item_path);
        entry.directory = S_ISDIR(info.st_mode);
        entry.epub = !entry.directory && ends_with_epub(item->d_name);
        entry.size = !entry.directory && info.st_size > 0 &&
                             static_cast<uint64_t>(info.st_size) <= UINT32_MAX
                         ? static_cast<uint32_t>(info.st_size)
                         : 0U;
    }
    closedir(directory);
    sort_entries(listing);
    return ESP_OK;
}

bool at_root(const listing_t* listing)
{
    return listing != nullptr && strcmp(listing->root, listing->path) == 0;
}

esp_err_t parent(listing_t* listing)
{
    if (listing == nullptr)
        return ESP_ERR_INVALID_ARG;
    if (at_root(listing))
        return ESP_OK;
    char target[path_length] = {};
    copy_text(target, sizeof(target), listing->path);
    char* slash = strrchr(target, '/');
    if (slash == nullptr)
        return ESP_ERR_INVALID_STATE;
    if (slash <= target + strlen(listing->root))
        copy_text(target, sizeof(target), listing->root);
    else
        *slash = '\0';
    return open(listing->root, target, listing);
}

} // namespace file_browser
} // namespace services
} // namespace xreader
