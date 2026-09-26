#include "xml.hpp"

#include <string.h>

namespace xreader
{
namespace epub
{
namespace xml
{
namespace
{

static bool whitespace(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static bool prefix(const char* data, size_t size, size_t offset, const char* value)
{
    const size_t length = strlen(value);
    return offset + length <= size && memcmp(data + offset, value, length) == 0;
}

static size_t find_sequence(const char* data, size_t size, size_t offset, const char* value)
{
    const size_t length = strlen(value);
    for (size_t index = offset; index + length <= size; ++index)
    {
        if (memcmp(data + index, value, length) == 0)
        {
            return index;
        }
    }
    return size;
}

} // namespace

void init(reader_t* reader, const char* data, size_t size)
{
    if (reader == nullptr)
    {
        return;
    }
    reader->data = data;
    reader->size = size;
    reader->offset = 0;
}

esp_err_t next(reader_t* reader, token_t* token)
{
    if (reader == nullptr || token == nullptr || reader->data == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }
    *token = {};

    while (reader->offset < reader->size)
    {
        if (reader->data[reader->offset] != '<')
        {
            const size_t start = reader->offset;
            while (reader->offset < reader->size && reader->data[reader->offset] != '<')
            {
                ++reader->offset;
            }
            token->type = token_text;
            token->value = reader->data + start;
            token->value_length = reader->offset - start;
            return ESP_OK;
        }

        if (prefix(reader->data, reader->size, reader->offset, "<!--"))
        {
            reader->offset = find_sequence(reader->data, reader->size, reader->offset + 4, "-->");
            if (reader->offset == reader->size)
            {
                return ESP_ERR_INVALID_RESPONSE;
            }
            reader->offset += 3;
            continue;
        }
        if (prefix(reader->data, reader->size, reader->offset, "<?"))
        {
            reader->offset = find_sequence(reader->data, reader->size, reader->offset + 2, "?>");
            if (reader->offset == reader->size)
            {
                return ESP_ERR_INVALID_RESPONSE;
            }
            reader->offset += 2;
            continue;
        }
        if (prefix(reader->data, reader->size, reader->offset, "<![CDATA["))
        {
            const size_t start = reader->offset + 9;
            reader->offset = find_sequence(reader->data, reader->size, start, "]]>");
            if (reader->offset == reader->size)
            {
                return ESP_ERR_INVALID_RESPONSE;
            }
            token->type = token_text;
            token->value = reader->data + start;
            token->value_length = reader->offset - start;
            reader->offset += 3;
            return ESP_OK;
        }
        if (prefix(reader->data, reader->size, reader->offset, "<!"))
        {
            reader->offset = find_sequence(reader->data, reader->size, reader->offset + 2, ">");
            if (reader->offset == reader->size)
            {
                return ESP_ERR_INVALID_RESPONSE;
            }
            ++reader->offset;
            continue;
        }

        const size_t tag_start = reader->offset + 1;
        size_t name_start = tag_start;
        token->type = token_start;
        if (reader->data[name_start] == '/')
        {
            token->type = token_end;
            ++name_start;
        }
        while (name_start < reader->size && whitespace(reader->data[name_start]))
        {
            ++name_start;
        }
        const size_t name_end = name_start;
        size_t cursor = name_start;
        while (cursor < reader->size && !whitespace(reader->data[cursor]) &&
               reader->data[cursor] != '/' && reader->data[cursor] != '>')
        {
            ++cursor;
        }
        if (cursor == name_end)
        {
            return ESP_ERR_INVALID_RESPONSE;
        }
        token->name = reader->data + name_start;
        token->name_length = cursor - name_start;
        const size_t close = find_sequence(reader->data, reader->size, cursor, ">");
        if (close == reader->size)
        {
            return ESP_ERR_INVALID_RESPONSE;
        }
        size_t marker = close;
        while (marker > cursor && whitespace(reader->data[marker - 1]))
        {
            --marker;
        }
        if (token->type == token_start && marker > cursor && reader->data[marker - 1] == '/')
        {
            token->type = token_empty;
        }
        token->value = reader->data + cursor;
        token->value_length = close - cursor;
        reader->offset = close + 1;
        return ESP_OK;
    }

    token->type = token_eof;
    return ESP_OK;
}

bool name_is(const token_t* token, const char* name)
{
    if (token == nullptr || name == nullptr || token->name == nullptr)
    {
        return false;
    }
    const size_t length = strlen(name);
    return token->name_length == length && memcmp(token->name, name, length) == 0;
}

} // namespace xml
} // namespace epub
} // namespace xreader
