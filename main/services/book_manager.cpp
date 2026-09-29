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
    return text_length >= suffix_length &&
           strcmp(text + text_length - suffix_length, suffix) == 0;
}



static uint64_t file_hash(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
        return 0U;
    uint64_t hash = 1469598103934665603ULL;
    uint8_t buffer[512] = {};
    size_t count = 0U;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0U)
        for (size_t index = 0; index < count; ++index)
        {
            hash ^= buffer[index];
            hash *= 1099511628211ULL;
        }
    fclose(file);
    return hash;
}

static bool safe_name(const char* name)
{
    if (name == nullptr || name[0] == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return false;
    for (const char* p = name; *p != '\0'; ++p)
        if (*p == '/' || *p == '\\' || static_cast<unsigned char>(*p) < 0x20U)
            return false;
    return true;
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


esp_err_t rename_book(const char* path, const char* new_name, char* new_path, size_t new_path_capacity)
{
    if (path == nullptr || !safe_name(new_name))
        return ESP_ERR_INVALID_ARG;
    const char* slash = strrchr(path, '/');
    if (slash == nullptr)
        return ESP_ERR_INVALID_ARG;
    char directory[512] = {};
    const size_t directory_length = static_cast<size_t>(slash - path);
    if (directory_length == 0U || directory_length >= sizeof(directory))
        return ESP_ERR_INVALID_SIZE;
    memcpy(directory, path, directory_length);
    directory[directory_length] = '\0';

    char filename[160] = {};
    if (ends_with(new_name, ".epub"))
        snprintf(filename, sizeof(filename), "%s", new_name);
    else
        snprintf(filename, sizeof(filename), "%s.epub", new_name);
    char destination[512] = {};
    const int written = snprintf(destination, sizeof(destination), "%s/%s", directory, filename);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(destination))
        return ESP_ERR_INVALID_SIZE;
    if (strcmp(path, destination) == 0)
    {
        if (new_path != nullptr && new_path_capacity > 0U)
            snprintf(new_path, new_path_capacity, "%s", destination);
        return ESP_OK;
    }
    struct stat info = {};
    if (stat(destination, &info) == 0)
        return ESP_ERR_INVALID_STATE;
    if (rename(path, destination) != 0)
        return ESP_FAIL;
    if (new_path != nullptr && new_path_capacity > 0U)
        snprintf(new_path, new_path_capacity, "%s", destination);
    return ESP_OK;
}

esp_err_t delete_book(const char* path)
{
    if (path == nullptr || !ends_with(path, ".epub"))
        return ESP_ERR_INVALID_ARG;
    return unlink(path) == 0 ? ESP_OK : ESP_FAIL;
}

uint16_t duplicate_count(const library_index::catalog_t* catalog)
{
    if (catalog == nullptr)
        return 0U;
    uint16_t duplicates = 0U;
    uint64_t hashes[library_index::max_books] = {};
    for (uint16_t left = 0; left < catalog->count; ++left)
    {
        for (uint16_t right = 0; right < left; ++right)
        {
            if (catalog->entries[left].file_size != catalog->entries[right].file_size)
                continue;
            if (hashes[left] == 0U)
                hashes[left] = file_hash(catalog->entries[left].path);
            if (hashes[right] == 0U)
                hashes[right] = file_hash(catalog->entries[right].path);
            if (hashes[left] != 0U && hashes[left] == hashes[right])
            {
                ++duplicates;
                break;
            }
        }
    }
    return duplicates;
}

} // namespace book_manager
} // namespace services
} // namespace xreader
