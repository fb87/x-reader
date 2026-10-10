#include "zip.h"

#include <stdlib.h>
#include <string.h>

enum {
    END_SIGNATURE = 0x06054b50U,
    CENTRAL_SIGNATURE = 0x02014b50U,
    LOCAL_SIGNATURE = 0x04034b50U,
    END_MINIMUM_SIZE = 22U,
    END_MAXIMUM_SEARCH = 65557U,
    CENTRAL_FIXED_SIZE = 46U,
};

struct zip_entry_storage {
    struct zip_entry public;
    char* name;
};

struct zip_archive {
    struct zip_source source;
    struct zip_limits limits;
    struct zip_entry_storage* entries;
    uint32_t count;
    uint64_t total_uncompressed;
};

static uint16_t read_u16(const uint8_t* data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8U;
}

static uint32_t read_u32(const uint8_t* data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8U | (uint32_t)data[2] << 16U |
           (uint32_t)data[3] << 24U;
}

static uint32_t crc32_bytes(const uint8_t* data, size_t length) {
    uint32_t crc = UINT32_MAX;
    for (size_t index = 0U; index < length; ++index) {
        crc ^= data[index];
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xedb88320U : crc >> 1U;
        }
    }
    return ~crc;
}

static bool range_valid(uint64_t offset, uint64_t length, uint64_t size) {
    return offset <= size && length <= size - offset;
}

static bool path_valid(const char* name, size_t length) {
    if (length == 0U || name[0] == '/' || name[0] == '\\' || name[0] == '\0') {
        return false;
    }
    const bool directory = name[length - 1U] == '/';
    const size_t path_length = directory ? length - 1U : length;
    size_t component_start = 0U;
    for (size_t index = 0U; index <= path_length; ++index) {
        const bool end = index == path_length || name[index] == '/';
        if (end) {
            const size_t component_length = index - component_start;
            if (component_length == 0U || (component_length == 2U && name[component_start] == '.' &&
                                           name[component_start + 1U] == '.')) {
                return false;
            }
            component_start = index + 1U;
        } else if (name[index] == '\\' || name[index] == ':') {
            return false;
        }
    }
    return true;
}

static enum zip_status read_bytes(const struct zip_source* source, uint64_t offset, void* buffer,
                                  size_t length) {
    if (!range_valid(offset, length, source->size) || source->read_at == NULL) {
        return ZIP_MALFORMED;
    }
    return source->read_at(source->context, offset, buffer, length);
}

static void free_entries(struct zip_archive* archive) {
    for (uint32_t index = 0U; index < archive->count; ++index) {
        free(archive->entries[index].name);
    }
    free(archive->entries);
}

static enum zip_status parse_entry(struct zip_archive* archive, uint64_t offset,
                                   struct zip_entry_storage* output, uint64_t* consumed) {
    uint8_t header[CENTRAL_FIXED_SIZE];
    enum zip_status status = read_bytes(&archive->source, offset, header, sizeof(header));
    if (status != ZIP_OK || read_u32(header) != CENTRAL_SIGNATURE) {
        return status == ZIP_OK ? ZIP_MALFORMED : status;
    }
    const uint16_t flags = read_u16(&header[8]);
    const uint16_t method = read_u16(&header[10]);
    const uint32_t compressed_size = read_u32(&header[20]);
    const uint32_t uncompressed_size = read_u32(&header[24]);
    const uint16_t name_length = read_u16(&header[28]);
    const uint16_t extra_length = read_u16(&header[30]);
    const uint16_t comment_length = read_u16(&header[32]);
    const uint32_t local_offset = read_u32(&header[42]);
    if ((flags & 1U) != 0U || (method != 0U && method != 8U) || name_length == 0U ||
        name_length > archive->limits.max_name_bytes ||
        uncompressed_size > archive->limits.max_entry_uncompressed_bytes ||
        !range_valid(offset + CENTRAL_FIXED_SIZE,
                     (uint64_t)name_length + extra_length + comment_length, archive->source.size)) {
        return (flags & 1U) != 0U || (method != 0U && method != 8U) ? ZIP_UNSUPPORTED : ZIP_LIMIT;
    }
    char* name = malloc((size_t)name_length + 1U);
    if (name == NULL) {
        return ZIP_OUT_OF_MEMORY;
    }
    status = read_bytes(&archive->source, offset + CENTRAL_FIXED_SIZE, name, name_length);
    if (status != ZIP_OK) {
        free(name);
        return status;
    }
    name[name_length] = '\0';
    if (!path_valid(name, name_length)) {
        free(name);
        return ZIP_MALFORMED;
    }
    output->name = name;
    output->public = (struct zip_entry){
        .name = name,
        .method = method,
        .flags = flags,
        .crc32 = read_u32(&header[16]),
        .compressed_size = compressed_size,
        .uncompressed_size = uncompressed_size,
        .local_header_offset = local_offset,
    };
    *consumed = (uint64_t)CENTRAL_FIXED_SIZE + name_length + extra_length + comment_length;
    return ZIP_OK;
}

enum zip_status zip_open(const struct zip_source* source, const struct zip_limits* limits,
                         struct zip_archive** result) {
    if (source == NULL || limits == NULL || result == NULL || source->read_at == NULL ||
        limits->max_entries == 0U || limits->max_name_bytes == 0U ||
        source->size < END_MINIMUM_SIZE) {
        return ZIP_INVALID_ARGUMENT;
    }
    const uint64_t search_size =
        source->size < END_MAXIMUM_SEARCH ? source->size : END_MAXIMUM_SEARCH;
    const uint64_t search_offset = source->size - search_size;
    uint8_t* tail = malloc((size_t)search_size);
    if (tail == NULL) {
        return ZIP_OUT_OF_MEMORY;
    }
    enum zip_status status = read_bytes(source, search_offset, tail, (size_t)search_size);
    if (status != ZIP_OK) {
        free(tail);
        return status;
    }
    uint64_t end_offset = UINT64_MAX;
    for (uint64_t index = search_size - END_MINIMUM_SIZE + 1U; index-- > 0U;) {
        if (read_u32(&tail[index]) == END_SIGNATURE) {
            end_offset = search_offset + index;
            break;
        }
    }
    if (end_offset == UINT64_MAX || !range_valid(end_offset, END_MINIMUM_SIZE, source->size)) {
        free(tail);
        return ZIP_MALFORMED;
    }
    const uint8_t* end = &tail[end_offset - search_offset];
    const uint16_t disk = read_u16(&end[4]);
    const uint16_t central_disk = read_u16(&end[6]);
    const uint16_t disk_entries = read_u16(&end[8]);
    const uint16_t total_entries = read_u16(&end[10]);
    const uint32_t central_size = read_u32(&end[12]);
    const uint32_t central_offset = read_u32(&end[16]);
    free(tail);
    if (disk != 0U || central_disk != 0U || disk_entries != total_entries ||
        total_entries > limits->max_entries ||
        !range_valid(central_offset, central_size, source->size)) {
        return disk != 0U || central_disk != 0U ? ZIP_UNSUPPORTED : ZIP_LIMIT;
    }
    struct zip_archive* archive = calloc(1U, sizeof(*archive));
    if (archive == NULL) {
        return ZIP_OUT_OF_MEMORY;
    }
    archive->source = *source;
    archive->limits = *limits;
    archive->entries = calloc(total_entries, sizeof(*archive->entries));
    if (archive->entries == NULL && total_entries != 0U) {
        free(archive);
        return ZIP_OUT_OF_MEMORY;
    }
    uint64_t offset = central_offset;
    for (uint32_t index = 0U; index < total_entries; ++index) {
        uint64_t consumed = 0U;
        status = parse_entry(archive, offset, &archive->entries[index], &consumed);
        if (status != ZIP_OK) {
            zip_close(archive);
            return status;
        }
        for (uint32_t previous = 0U; previous < index; ++previous) {
            if (strcmp(archive->entries[previous].name, archive->entries[index].name) == 0) {
                zip_close(archive);
                return ZIP_MALFORMED;
            }
        }
        if (archive->entries[index].public.uncompressed_size >
            archive->limits.max_total_uncompressed_bytes - archive->total_uncompressed) {
            zip_close(archive);
            return ZIP_LIMIT;
        }
        archive->total_uncompressed += archive->entries[index].public.uncompressed_size;
        if (consumed > central_size - (offset - central_offset)) {
            zip_close(archive);
            return ZIP_MALFORMED;
        }
        offset += consumed;
    }
    archive->count = total_entries;
    *result = archive;
    return ZIP_OK;
}

void zip_close(struct zip_archive* archive) {
    if (archive == NULL)
        return;
    free_entries(archive);
    free(archive);
}

uint32_t zip_entry_count(const struct zip_archive* archive) {
    return archive == NULL ? 0U : archive->count;
}

enum zip_status zip_entry_get(const struct zip_archive* archive, uint32_t index,
                              struct zip_entry* entry) {
    if (archive == NULL || entry == NULL)
        return ZIP_INVALID_ARGUMENT;
    if (index >= archive->count)
        return ZIP_MALFORMED;
    *entry = archive->entries[index].public;
    return ZIP_OK;
}

bool zip_entry_find(const struct zip_archive* archive, const char* name, uint32_t* index) {
    if (archive == NULL || name == NULL || index == NULL)
        return false;
    for (uint32_t current = 0U; current < archive->count; ++current) {
        if (strcmp(archive->entries[current].name, name) == 0) {
            *index = current;
            return true;
        }
    }
    return false;
}

enum zip_status zip_entry_read_all(const struct zip_archive* archive, uint32_t index,
                                   uint8_t** data, size_t* length) {
    if (archive == NULL || data == NULL || length == NULL)
        return ZIP_INVALID_ARGUMENT;
    if (index >= archive->count)
        return ZIP_MALFORMED;
    const struct zip_entry* entry = &archive->entries[index].public;
    if (entry->uncompressed_size > archive->limits.max_read_all_bytes)
        return ZIP_LIMIT;
    if (entry->method != 0U && entry->method != 8U)
        return ZIP_UNSUPPORTED;
    if (entry->method == 0U && entry->uncompressed_size != entry->compressed_size)
        return ZIP_MALFORMED;
    uint8_t local[30];
    enum zip_status status =
        read_bytes(&archive->source, entry->local_header_offset, local, sizeof(local));
    if (status != ZIP_OK || read_u32(local) != LOCAL_SIGNATURE) {
        return status == ZIP_OK ? ZIP_MALFORMED : status;
    }
    const uint16_t local_flags = read_u16(&local[6]);
    const uint16_t local_method = read_u16(&local[8]);
    const uint16_t name_length = read_u16(&local[26]);
    const uint16_t extra_length = read_u16(&local[28]);
    if ((local_flags & 1U) != 0U || local_method != entry->method ||
        !range_valid((uint64_t)entry->local_header_offset + sizeof(local),
                     (uint64_t)name_length + extra_length + entry->compressed_size,
                     archive->source.size)) {
        return ZIP_MALFORMED;
    }
    const uint64_t data_offset =
        (uint64_t)entry->local_header_offset + sizeof(local) + name_length + extra_length;
    if (name_length != strlen(entry->name))
        return ZIP_MALFORMED;
    char* local_name = malloc((size_t)name_length + 1U);
    if (local_name == NULL)
        return ZIP_OUT_OF_MEMORY;
    status = read_bytes(&archive->source, (uint64_t)entry->local_header_offset + sizeof(local),
                        local_name, name_length);
    if (status != ZIP_OK) {
        free(local_name);
        return status;
    }
    local_name[name_length] = '\0';
    const bool same_name = strcmp(local_name, entry->name) == 0;
    free(local_name);
    if (!same_name)
        return ZIP_MALFORMED;
    uint8_t* output = malloc(entry->uncompressed_size == 0U ? 1U : entry->uncompressed_size);
    if (output == NULL)
        return ZIP_OUT_OF_MEMORY;
    if (entry->method == 0U) {
        status = read_bytes(&archive->source, data_offset, output, entry->uncompressed_size);
    } else if (archive->source.inflate_raw == NULL) {
        free(output);
        return ZIP_UNSUPPORTED;
    } else {
        status =
            archive->source.inflate_raw(archive->source.context, data_offset,
                                        entry->compressed_size, output, entry->uncompressed_size);
    }
    if (status != ZIP_OK) {
        free(output);
        return status;
    }
    if (crc32_bytes(output, entry->uncompressed_size) != entry->crc32) {
        free(output);
        return ZIP_MALFORMED;
    }
    *data = output;
    *length = entry->uncompressed_size;
    return ZIP_OK;
}

void zip_data_free(uint8_t* data) { free(data); }
