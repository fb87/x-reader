#ifndef READER_ZIP_H
#define READER_ZIP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum zip_status {
    ZIP_OK = 0,
    ZIP_INVALID_ARGUMENT,
    ZIP_IO_ERROR,
    ZIP_MALFORMED,
    ZIP_LIMIT,
    ZIP_UNSUPPORTED,
    ZIP_OUT_OF_MEMORY,
};

struct zip_source {
    void* context;
    uint64_t size;
    enum zip_status (*read_at)(void* context, uint64_t offset, void* buffer, size_t length);
    enum zip_status (*inflate_raw)(void* context, uint64_t offset, size_t compressed_size,
                                   void* output, size_t output_size);
};

struct zip_limits {
    uint32_t max_entries;
    uint32_t max_name_bytes;
    uint64_t max_entry_uncompressed_bytes;
    uint64_t max_total_uncompressed_bytes;
    uint32_t max_read_all_bytes;
};

struct zip_archive;

struct zip_entry {
    const char* name;
    uint16_t method;
    uint32_t flags;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t local_header_offset;
};

enum zip_status zip_open(const struct zip_source* source, const struct zip_limits* limits,
                         struct zip_archive** archive);
void zip_close(struct zip_archive* archive);
uint32_t zip_entry_count(const struct zip_archive* archive);
enum zip_status zip_entry_get(const struct zip_archive* archive, uint32_t index,
                              struct zip_entry* entry);
bool zip_entry_find(const struct zip_archive* archive, const char* name, uint32_t* index);
enum zip_status zip_entry_read_all(const struct zip_archive* archive, uint32_t index,
                                   uint8_t** data, size_t* length);
void zip_data_free(uint8_t* data);

#endif
