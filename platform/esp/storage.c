#include "storage.h"

#include "board_config.h"
#include "epub.h"
#include "zip.h"

#include <dirent.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <limits.h>
#include <miniz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct firmware_archive {
    FILE* file;
    struct zip_archive* archive;
};

static const char* const TAG = "x-reader-storage";

static enum zip_status archive_read_at(void* opaque, uint64_t offset, void* buffer, size_t length) {
    FILE* file = opaque;
    if (offset > LONG_MAX || fseek(file, (long)offset, SEEK_SET) != 0 ||
        fread(buffer, 1U, length, file) != length) {
        return ZIP_IO_ERROR;
    }
    return ZIP_OK;
}

static enum zip_status archive_inflate(void* opaque, uint64_t offset, size_t compressed_size,
                                       void* output, size_t output_size) {
    FILE* file = opaque;
    uint8_t* compressed = malloc(compressed_size == 0U ? 1U : compressed_size);
    if (compressed == NULL) {
        return ZIP_OUT_OF_MEMORY;
    }
    const enum zip_status read_status = archive_read_at(file, offset, compressed, compressed_size);
    if (read_status != ZIP_OK) {
        free(compressed);
        return read_status;
    }
    const size_t result =
        tinfl_decompress_mem_to_mem(output, output_size, compressed, compressed_size, 0);
    free(compressed);
    return result == output_size ? ZIP_OK : ZIP_MALFORMED;
}

static int entry_compare(const void* left, const void* right) {
    const struct platform_entry* a = left;
    const struct platform_entry* b = right;
    if (a->directory != b->directory) {
        return a->directory ? -1 : 1;
    }
    return strcmp(a->name, b->name);
}

esp_err_t m5paper_storage_mount(struct m5paper_storage* storage) {
    if (storage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t device = SDSPI_DEVICE_CONFIG_DEFAULT();
    device.gpio_cs = M5PAPER_STORAGE_SPI_CS;
    device.host_id = VSPI_HOST;
    const esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16U * 1024U,
    };
    const esp_err_t status =
        esp_vfs_fat_sdspi_mount("/sdcard", &host, &device, &mount, &storage->card);
    storage->mounted = status == ESP_OK;
    return status;
}

void m5paper_storage_unmount(struct m5paper_storage* storage) {
    if (storage == NULL || !storage->mounted) {
        return;
    }
    (void)esp_vfs_fat_sdcard_unmount("/sdcard", storage->card);
    storage->card = NULL;
    storage->mounted = false;
}

enum platform_status m5paper_storage_list(void* opaque, const char* root, const char* relative,
                                          struct platform_entry* entries, uint32_t capacity,
                                          uint32_t* count) {
    struct m5paper_storage* storage = opaque;
    if (storage == NULL || !storage->mounted || root == NULL || relative == NULL ||
        entries == NULL || count == NULL) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    char directory[PLATFORM_PATH_MAX];
    const int written = relative[0] == '\0'
                            ? snprintf(directory, sizeof(directory), "%s", root)
                            : snprintf(directory, sizeof(directory), "%s/%s", root, relative);
    if (written < 0 || (size_t)written >= sizeof(directory)) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    DIR* stream = opendir(directory);
    if (stream == NULL) {
        return PLATFORM_ERROR;
    }
    uint32_t found = 0U;
    struct dirent* item = NULL;
    while ((item = readdir(stream)) != NULL && found < capacity) {
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) {
            continue;
        }
        char full_path[PLATFORM_PATH_MAX];
        const int full_written =
            snprintf(full_path, sizeof(full_path), "%s/%s", directory, item->d_name);
        if (full_written < 0 || (size_t)full_written >= sizeof(full_path)) {
            continue;
        }
        struct stat file = {0};
        if (stat(full_path, &file) != 0 || (!S_ISDIR(file.st_mode) && !S_ISREG(file.st_mode))) {
            continue;
        }
        const size_t name_length = strlen(item->d_name);
        if (name_length >= PLATFORM_NAME_MAX) {
            continue;
        }
        struct platform_entry* entry = &entries[found];
        memcpy(entry->name, item->d_name, name_length + 1U);
        epub_repair_mojibake(entry->name, sizeof(entry->name));
        const int path_written =
            relative[0] == '\0'
                ? snprintf(entry->path, sizeof(entry->path), "%s", item->d_name)
                : snprintf(entry->path, sizeof(entry->path), "%s/%s", relative, item->d_name);
        if (path_written < 0 || (size_t)path_written >= sizeof(entry->path)) {
            continue;
        }
        entry->directory = S_ISDIR(file.st_mode);
        entry->size = entry->directory ? 0U : (uint64_t)file.st_size;
        ++found;
    }
    closedir(stream);
    qsort(entries, found, sizeof(*entries), entry_compare);
    *count = found;
    if (relative[0] == '\0') {
        ESP_LOGI(TAG, "listed %u root entries", found);
    }
    return PLATFORM_OK;
}

enum platform_status m5paper_storage_open_archive(void* opaque, const char* root, const char* path,
                                                  struct platform_archive* result) {
    struct m5paper_storage* storage = opaque;
    if (storage == NULL || !storage->mounted || root == NULL || path == NULL || result == NULL) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    char full_path[PLATFORM_PATH_MAX];
    const int written = snprintf(full_path, sizeof(full_path), "%s/%s", root, path);
    if (written < 0 || (size_t)written >= sizeof(full_path)) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    FILE* file = fopen(full_path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return PLATFORM_ERROR;
    }
    const long end = ftell(file);
    if (end < 0) {
        fclose(file);
        return PLATFORM_ERROR;
    }
    struct firmware_archive* firmware = calloc(1U, sizeof(*firmware));
    if (firmware == NULL) {
        fclose(file);
        return PLATFORM_ERROR;
    }
    firmware->file = file;
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
    if (zip_open(&source, &limits, &firmware->archive) != ZIP_OK) {
        fclose(file);
        free(firmware);
        return PLATFORM_ERROR;
    }
    result->archive = firmware->archive;
    result->context = firmware;
    return PLATFORM_OK;
}

void m5paper_storage_close_archive(void* opaque, struct platform_archive* archive) {
    (void)opaque;
    if (archive == NULL || archive->context == NULL) {
        return;
    }
    struct firmware_archive* firmware = archive->context;
    zip_close(firmware->archive);
    fclose(firmware->file);
    free(firmware);
    archive->archive = NULL;
    archive->context = NULL;
}
