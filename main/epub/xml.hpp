#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace epub
{
namespace xml
{

enum token_type_t : uint8_t
{
    token_eof,
    token_start,
    token_end,
    token_empty,
    token_text,
};

struct token_t
{
    token_type_t type;
    const char* name;
    size_t name_length;
    const char* value;
    size_t value_length;
};

struct reader_t
{
    const char* data;
    size_t size;
    size_t offset;
};

void init(reader_t* reader, const char* data, size_t size);
esp_err_t next(reader_t* reader, token_t* token);
bool name_is(const token_t* token, const char* name);

} // namespace xml
} // namespace epub
} // namespace xreader
