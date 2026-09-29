#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace services
{
namespace file_browser
{

static constexpr size_t max_entries = 32;
static constexpr size_t path_length = 256;
static constexpr size_t name_length = 96;

struct entry_t
{
    char name[name_length];
    char path[path_length];
    uint32_t size;
    bool directory;
    bool epub;
};

struct listing_t
{
    char root[path_length];
    char path[path_length];
    uint8_t count;
    entry_t entries[max_entries];
};

esp_err_t open(const char* root, const char* path, listing_t* listing);
esp_err_t parent(listing_t* listing);
bool at_root(const listing_t* listing);

} // namespace file_browser
} // namespace services
} // namespace xreader
