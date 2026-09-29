#include "book_manager.hpp"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace xreader
{
namespace services
{
namespace book_manager
{
namespace
{
static bool ends_with(const char* text, const char* suffix)
{
    if (text == nullptr || suffix == nullptr)
        return false;
    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length && strcmp(text + text_length - suffix_length, suffix) == 0;
}

static bool make_directory(const char* path)
{
    if (mkdir(path, 0755) == 0)
        return true;
    struct stat info = {};
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}
} // namespace

result_t import_books(const char* mount_path)
{
    result_t result = {};
    result.error = ESP_OK;
    if (mount_path == nullptr)
    {
        result.error = ESP_ERR_INVALID_ARG;
        return result;
    }

    char source_directory[256] = {};
    char destination_directory[256] = {};
    snprintf(source_directory, sizeof(source_directory), "%s/import", mount_path);
    snprintf(destination_directory, sizeof(destination_directory), "%s/books", mount_path);
    if (!make_directory(destination_directory))
    {
        result.error = ESP_FAIL;
        return result;
    }

    DIR* directory = opendir(source_directory);
    if (directory == nullptr)
    {
        result.error = ESP_ERR_NOT_FOUND;
        return result;
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(directory)) != nullptr)
    {
        if (entry->d_name[0] == '.' || !ends_with(entry->d_name, ".epub"))
            continue;
        char source[512] = {};
        char destination[512] = {};
        snprintf(source, sizeof(source), "%s/%s", source_directory, entry->d_name);
        snprintf(destination, sizeof(destination), "%s/%s", destination_directory, entry->d_name);
        struct stat info = {};
        if (stat(destination, &info) == 0)
            continue;
        if (rename(source, destination) == 0)
            ++result.imported;
        else
            result.error = ESP_FAIL;
    }
    closedir(directory);
    return result;
}

result_t cleanup(const char* mount_path)
{
    result_t result = {};
    result.error = ESP_OK;
    if (mount_path == nullptr)
    {
        result.error = ESP_ERR_INVALID_ARG;
        return result;
    }

    const char* const directories[] = {mount_path, "/sdcard/books", "/sdcard/import"};
    for (const char* directory_path : directories)
    {
        char resolved[256] = {};
        if (directory_path == mount_path)
            snprintf(resolved, sizeof(resolved), "%s", mount_path);
        else
        {
            const char* suffix = strrchr(directory_path, '/');
            snprintf(resolved, sizeof(resolved), "%s/%s", mount_path,
                     suffix == nullptr ? directory_path : suffix + 1);
        }
        DIR* directory = opendir(resolved);
        if (directory == nullptr)
            continue;
        struct dirent* entry = nullptr;
        while ((entry = readdir(directory)) != nullptr)
        {
            if (entry->d_name[0] == '.')
                continue;
            if (!ends_with(entry->d_name, ".part") && !ends_with(entry->d_name, ".tmp"))
                continue;
            char path[512] = {};
            snprintf(path, sizeof(path), "%s/%s", resolved, entry->d_name);
            if (unlink(path) == 0)
                ++result.removed;
            else
                result.error = ESP_FAIL;
        }
        closedir(directory);
    }
    return result;
}

} // namespace book_manager
} // namespace services
} // namespace xreader
