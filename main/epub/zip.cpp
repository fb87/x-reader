#include "zip.hpp"

#include <string.h>

#include "esp_heap_caps.h"
#include "inflate.hpp"

namespace xreader
{
namespace epub
{
namespace zip
{

namespace
{

static constexpr uint32_t end_signature = 0x06054b50;
static constexpr uint32_t central_signature = 0x02014b50;
static constexpr uint32_t local_signature = 0x04034b50;
static constexpr size_t end_record_size = 22;
static constexpr size_t max_comment_length = 65535;
static constexpr size_t max_search_size = end_record_size + max_comment_length;
static constexpr uint32_t max_entry_size = 4 * 1024 * 1024;

static uint16_t read_u16(const uint8_t* data)
{
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

static uint32_t read_u32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

static bool read_exact(FILE* file, void* buffer, size_t size)
{
    return fread(buffer, 1, size, file) == size;
}

static esp_err_t locate_end_record(FILE* file, uint32_t* central_offset, uint16_t* entry_count)
{
    if (fseek(file, 0, SEEK_END) != 0)
    {
        return ESP_ERR_INVALID_STATE;
    }
    const long file_size = ftell(file);
    if (file_size < static_cast<long>(end_record_size))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    const size_t search_size = (static_cast<unsigned long>(file_size) < max_search_size)
                                   ? static_cast<size_t>(file_size)
                                   : max_search_size;
    uint8_t* search_buffer =
        static_cast<uint8_t*>(heap_caps_malloc(search_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (search_buffer == nullptr)
    {
        return ESP_ERR_NO_MEM;
    }
    if (fseek(file, file_size - static_cast<long>(search_size), SEEK_SET) != 0 ||
        !read_exact(file, search_buffer, search_size))
    {
        heap_caps_free(search_buffer);
        return ESP_ERR_INVALID_STATE;
    }

    for (size_t index = search_size - end_record_size + 1; index > 0; --index)
    {
        const size_t offset = index - 1;
        if (read_u32(search_buffer + offset) != end_signature)
        {
            continue;
        }
        const uint16_t disk_entries = read_u16(search_buffer + offset + 10);
        const uint32_t directory_size = read_u32(search_buffer + offset + 12);
        const uint32_t directory_offset = read_u32(search_buffer + offset + 16);
        if (read_u16(search_buffer + offset + 4) != 0 || disk_entries == 0xffff ||
            directory_size == 0xffffffff || directory_offset == 0xffffffff)
        {
            heap_caps_free(search_buffer);
            return ESP_ERR_NOT_SUPPORTED;
        }
        *entry_count = disk_entries;
        *central_offset = directory_offset;
        heap_caps_free(search_buffer);
        return ESP_OK;
    }
    heap_caps_free(search_buffer);
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t read_central_entry(FILE* file, entry_t* entry, char* name_buffer,
                                    size_t name_capacity)
{
    uint8_t header[46] = {};
    if (!read_exact(file, header, sizeof(header)))
    {
        return ESP_ERR_INVALID_SIZE;
    }
    if (read_u32(header) != central_signature)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const uint16_t name_length = read_u16(header + 28);
    const uint16_t extra_length = read_u16(header + 30);
    const uint16_t comment_length = read_u16(header + 32);
    if (name_length == 0 || name_length >= name_capacity)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    if (!read_exact(file, name_buffer, name_length))
    {
        return ESP_ERR_INVALID_SIZE;
    }
    name_buffer[name_length] = '\0';
    if (fseek(file, extra_length + comment_length, SEEK_CUR) != 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    memset(entry, 0, sizeof(*entry));
    memcpy(entry->name, name_buffer,
           (name_length < max_entry_name_length - 1) ? name_length : max_entry_name_length - 1);
    entry->compression_method = read_u16(header + 10);
    entry->compressed_size = read_u32(header + 20);
    entry->uncompressed_size = read_u32(header + 24);
    entry->local_header_offset = read_u32(header + 42);
    return ESP_OK;
}

} // namespace

esp_err_t open(archive_t* archive, const char* path)
{
    if (archive == nullptr || path == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memset(archive, 0, sizeof(*archive));
    archive->file = fopen(path, "rb");
    if (archive->file == nullptr)
    {
        return ESP_ERR_NOT_FOUND;
    }

    const esp_err_t error =
        locate_end_record(archive->file, &archive->central_directory_offset, &archive->entry_count);
    if (error != ESP_OK)
    {
        close(archive);
        return error;
    }
    return ESP_OK;
}

void close(archive_t* archive)
{
    if (archive == nullptr)
    {
        return;
    }
    if (archive->file != nullptr)
    {
        fclose(archive->file);
    }
    memset(archive, 0, sizeof(*archive));
}

esp_err_t find(archive_t* archive, const char* name, entry_t* entry)
{
    if (archive == nullptr || archive->file == nullptr || name == nullptr || entry == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (fseek(archive->file, archive->central_directory_offset, SEEK_SET) != 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    char name_buffer[max_entry_name_length] = {};
    for (uint16_t index = 0; index < archive->entry_count; ++index)
    {
        entry_t candidate = {};
        const esp_err_t error =
            read_central_entry(archive->file, &candidate, name_buffer, sizeof(name_buffer));
        if (error != ESP_OK)
        {
            return error;
        }
        if (strcmp(candidate.name, name) == 0)
        {
            *entry = candidate;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t read(archive_t* archive, const entry_t* entry, uint8_t* output, size_t output_capacity,
               size_t* output_size)
{
    if (archive == nullptr || archive->file == nullptr || entry == nullptr || output == nullptr ||
        output_size == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (entry->compression_method != 0 && entry->compression_method != 8)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (entry->uncompressed_size > output_capacity)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    if (entry->uncompressed_size > max_entry_size || entry->compressed_size > max_entry_size)
        return ESP_ERR_INVALID_SIZE;

    if (fseek(archive->file, entry->local_header_offset, SEEK_SET) != 0)
    {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t header[30] = {};
    if (!read_exact(archive->file, header, sizeof(header)) || read_u32(header) != local_signature)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }
    const uint16_t name_length = read_u16(header + 26);
    const uint16_t extra_length = read_u16(header + 28);
    if (fseek(archive->file, name_length + extra_length, SEEK_CUR) != 0)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    if (entry->compression_method == 0)
    {
        if (entry->uncompressed_size != entry->compressed_size ||
            !read_exact(archive->file, output, entry->uncompressed_size))
        {
            return ESP_ERR_INVALID_SIZE;
        }
        *output_size = entry->uncompressed_size;
        return ESP_OK;
    }

    uint8_t* compressed = static_cast<uint8_t*>(
        heap_caps_malloc(entry->compressed_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (compressed == nullptr)
    {
        return ESP_ERR_NO_MEM;
    }
    if (!read_exact(archive->file, compressed, entry->compressed_size))
    {
        heap_caps_free(compressed);
        return ESP_ERR_INVALID_SIZE;
    }
    const esp_err_t error =
        inflate::decode(compressed, entry->compressed_size, output, output_capacity, output_size);
    heap_caps_free(compressed);
    if (error != ESP_OK || *output_size != entry->uncompressed_size)
    {
        return error == ESP_OK ? ESP_ERR_INVALID_SIZE : error;
    }
    return ESP_OK;
}

} // namespace zip
} // namespace epub
} // namespace xreader
