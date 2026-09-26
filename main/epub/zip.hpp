#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"

namespace xreader
{
namespace epub
{
namespace zip
{

static constexpr size_t max_entry_name_length = 128;

struct archive_t
{
    FILE* file;
    uint32_t central_directory_offset;
    uint16_t entry_count;
};

struct entry_t
{
    char name[max_entry_name_length];
    uint32_t local_header_offset;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t compression_method;
};

esp_err_t open(archive_t* archive, const char* path);
void close(archive_t* archive);
esp_err_t find(archive_t* archive, const char* name, entry_t* entry);
esp_err_t read(archive_t* archive, const entry_t* entry, uint8_t* output, size_t output_capacity,
               size_t* output_size);

} // namespace zip
} // namespace epub
} // namespace xreader
