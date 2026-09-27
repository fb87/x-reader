#include "book.hpp"

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "esp_heap_caps.h"
#include "inflate.hpp"
#include "xml.hpp"
#include "zip.hpp"

namespace xreader
{
namespace epub
{
namespace
{

static bool attribute(const xml::token_t* token, const char* key, char* output, size_t capacity)
{
    const char* start = token->value;
    const char* end = token->value + token->value_length;
    const size_t key_length = strlen(key);
    while (start < end)
    {
        while (start < end && isspace(static_cast<unsigned char>(*start)))
            ++start;
        if (start + key_length > end || memcmp(start, key, key_length) != 0 ||
            (start + key_length < end && start[key_length] != '=' &&
             !isspace(static_cast<unsigned char>(start[key_length]))))
        {
            while (start < end && !isspace(static_cast<unsigned char>(*start)))
                ++start;
            continue;
        }
        start += key_length;
        while (start < end && (isspace(static_cast<unsigned char>(*start)) || *start == '='))
            ++start;
        if (start >= end || (*start != '\'' && *start != '"'))
            return false;
        const char quote = *start++;
        const char* value_start = start;
        while (start < end && *start != quote)
            ++start;
        const size_t length = static_cast<size_t>(start - value_start);
        if (length >= capacity)
            return false;
        memcpy(output, value_start, length);
        output[length] = '\0';
        return true;
    }
    return false;
}

static esp_err_t read_entry(zip::archive_t* archive, const char* name, char** data, size_t* size)
{
    zip::entry_t entry = {};
    esp_err_t error = zip::find(archive, name, &entry);
    if (error != ESP_OK)
        return error;
    char* buffer = static_cast<char*>(
        heap_caps_malloc(entry.uncompressed_size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (buffer == nullptr)
        return ESP_ERR_NO_MEM;
    error = zip::read(archive, &entry, reinterpret_cast<uint8_t*>(buffer), entry.uncompressed_size,
                      size);
    if (error != ESP_OK)
    {
        heap_caps_free(buffer);
        return error;
    }
    buffer[*size] = '\0';
    *data = buffer;
    return ESP_OK;
}

static void copy_text(char* destination, const char* source, size_t length)
{
    if (length >= book_text_length)
        length = book_text_length - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static void trim_copy(char* destination, const char* source, size_t length)
{
    while (length > 0 && isspace(static_cast<unsigned char>(*source)))
    {
        ++source;
        --length;
    }
    while (length > 0 && isspace(static_cast<unsigned char>(source[length - 1])))
        --length;
    copy_text(destination, source, length);
}

static void copy_field(char* destination, const char* source, size_t capacity)
{
    if (capacity == 0)
        return;
    size_t length = strlen(source);
    if (length >= capacity)
        length = capacity - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static void append_text(document_t* document, const char* source, size_t length, bool* last_space)
{
    size_t index = 0;
    while (index < length && document->length + 1 < document_text_length)
    {
        char value = source[index];
        if (value == '&')
        {
            const char* end = static_cast<const char*>(memchr(source + index, ';', length - index));
            if (end != nullptr)
            {
                const size_t entity_length = static_cast<size_t>(end - (source + index));
                if (entity_length == 4 && memcmp(source + index, "&amp", 4) == 0)
                    value = '&';
                else if (entity_length == 3 && memcmp(source + index, "&lt", 3) == 0)
                    value = '<';
                else if (entity_length == 3 && memcmp(source + index, "&gt", 3) == 0)
                    value = '>';
                else if (entity_length == 5 && memcmp(source + index, "&quot", 5) == 0)
                    value = '"';
                else if (entity_length == 5 && memcmp(source + index, "&apos", 5) == 0)
                    value = '\'';
                else
                    value = ' ';
                index = static_cast<size_t>(end - source) + 1;
            }
        }
        else if (static_cast<uint8_t>(value) >= 0x80U)
        {
            const uint8_t first = static_cast<uint8_t>(value);
            size_t consumed = 1;
            if ((first & 0xe0U) == 0xc0U && index + 1 < length)
                consumed = 2;
            else if ((first & 0xf0U) == 0xe0U && index + 2 < length)
                consumed = 3;
            if (document->length + consumed >= document_text_length || index + consumed > length)
                break;
            memcpy(document->text + document->length, source + index, consumed);
            document->length += consumed;
            index += consumed;
            *last_space = false;
            continue;
        }
        else
        {
            ++index;
        }
        if (isspace(static_cast<unsigned char>(value)))
        {
            if (!*last_space && document->length + 1 < document_text_length)
                document->text[document->length++] = ' ';
            *last_space = true;
        }
        else
        {
            document->text[document->length++] = value;
            *last_space = false;
        }
    }
}

static void directory_name(const char* path, char* output)
{
    copy_field(output, path, book_text_length);
    char* slash = strrchr(output, '/');
    if (slash != nullptr)
        slash[1] = '\0';
    else
        output[0] = '\0';
}

static int hex_digit(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

static bool join_path(const char* directory, const char* href, char* output)
{
    char combined[book_text_length * 2] = {};
    const size_t directory_length = strlen(directory);
    const size_t href_length = strlen(href);
    if (directory_length + href_length + 2 > sizeof(combined))
        return false;
    memcpy(combined, directory, directory_length);
    size_t length = directory_length;
    if (length > 0 && combined[length - 1] != '/')
        combined[length++] = '/';
    for (size_t index = 0; index < href_length && length + 1 < sizeof(combined); ++index)
    {
        if (href[index] == '#')
            break;
        if (href[index] == '%' && index + 2 < href_length)
        {
            const int high = hex_digit(href[index + 1]);
            const int low = hex_digit(href[index + 2]);
            if (high >= 0 && low >= 0)
            {
                combined[length++] = static_cast<char>((high << 4) | low);
                index += 2;
                continue;
            }
        }
        combined[length++] = href[index];
    }
    combined[length] = '\0';

    size_t output_length = 0;
    size_t cursor = 0;
    while (cursor < length)
    {
        while (cursor < length && combined[cursor] == '/')
            ++cursor;
        const size_t segment_start = cursor;
        while (cursor < length && combined[cursor] != '/')
            ++cursor;
        const size_t segment_length = cursor - segment_start;
        if (segment_length == 0 || (segment_length == 1 && combined[segment_start] == '.'))
            continue;
        if (segment_length == 2 && combined[segment_start] == '.' &&
            combined[segment_start + 1] == '.')
        {
            while (output_length > 0 && output[output_length - 1] != '/')
                --output_length;
            if (output_length > 0)
                --output_length;
            continue;
        }
        if (output_length != 0)
            output[output_length++] = '/';
        if (output_length + segment_length >= book_text_length)
            return false;
        memcpy(output + output_length, combined + segment_start, segment_length);
        output_length += segment_length;
    }
    output[output_length] = '\0';
    return true;
}

} // namespace

esp_err_t load_metadata(const char* path, book_t* book)
{
    if (path == nullptr || book == nullptr)
        return ESP_ERR_INVALID_ARG;
    memset(book, 0, sizeof(*book));
    zip::archive_t archive = {};
    esp_err_t error = zip::open(&archive, path);
    if (error != ESP_OK)
        return error;

    char* container = nullptr;
    size_t container_size = 0;
    error = read_entry(&archive, "META-INF/container.xml", &container, &container_size);
    if (error != ESP_OK)
    {
        zip::close(&archive);
        return error;
    }
    xml::reader_t reader = {};
    xml::init(&reader, container, container_size);
    xml::token_t token = {};
    char rootfile[book_text_length] = {};
    while (xml::next(&reader, &token) == ESP_OK && token.type != xml::token_eof)
    {
        if (token.type == xml::token_empty && xml::name_is(&token, "rootfile"))
        {
            attribute(&token, "full-path", rootfile, sizeof(rootfile));
            break;
        }
    }
    heap_caps_free(container);
    if (rootfile[0] == '\0')
    {
        zip::close(&archive);
        return ESP_ERR_NOT_FOUND;
    }
    char normalized_rootfile[book_text_length] = {};
    if (!join_path("", rootfile, normalized_rootfile))
    {
        zip::close(&archive);
        return ESP_ERR_INVALID_SIZE;
    }
    copy_field(book->opf_path, normalized_rootfile, book_text_length);

    char* opf = nullptr;
    size_t opf_size = 0;
    error = read_entry(&archive, normalized_rootfile, &opf, &opf_size);
    if (error != ESP_OK)
    {
        zip::close(&archive);
        return error;
    }
    xml::init(&reader, opf, opf_size);
    char* spine_id = static_cast<char*>(
        heap_caps_calloc(1, book_text_length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    char* item_ids = static_cast<char*>(
        heap_caps_calloc(book_spine_length, book_text_length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    char* item_hrefs = static_cast<char*>(
        heap_caps_calloc(book_spine_length, book_text_length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    char toc_href[book_text_length] = {};
    if (spine_id == nullptr || item_ids == nullptr || item_hrefs == nullptr)
    {
        heap_caps_free(spine_id);
        heap_caps_free(item_ids);
        heap_caps_free(item_hrefs);
        heap_caps_free(opf);
        zip::close(&archive);
        return ESP_ERR_NO_MEM;
    }
    directory_name(rootfile, book->opf_directory);
    bool in_title = false;
    bool in_creator = false;
    while (xml::next(&reader, &token) == ESP_OK && token.type != xml::token_eof)
    {
        if (token.type == xml::token_start && xml::name_is(&token, "dc:title"))
        {
            in_title = true;
        }
        else if (token.type == xml::token_start && xml::name_is(&token, "dc:creator"))
        {
            in_creator = true;
        }
        else if (token.type == xml::token_end && xml::name_is(&token, "dc:title"))
        {
            in_title = false;
        }
        else if (token.type == xml::token_end && xml::name_is(&token, "dc:creator"))
        {
            in_creator = false;
        }
        else if (token.type == xml::token_text && token.value_length > 0)
        {
            if (in_title && book->title[0] == '\0')
                trim_copy(book->title, token.value, token.value_length);
            if (in_creator && book->author[0] == '\0')
                trim_copy(book->author, token.value, token.value_length);
        }
        if ((token.type == xml::token_empty || token.type == xml::token_start) &&
            xml::name_is(&token, "itemref"))
        {
            attribute(&token, "idref", spine_id, book_text_length);
            if (book->spine_count < book_spine_length && spine_id[0] != '\0')
            {
                const uint8_t index = book->spine_count++;
                copy_field(book->spine[index].id, spine_id, book_text_length);
                for (uint8_t item = 0; item < book_spine_length; ++item)
                {
                    if (strcmp(item_ids + item * book_text_length, spine_id) == 0)
                    {
                        copy_field(book->spine[index].href, item_hrefs + item * book_text_length,
                                   book_text_length);
                        if (index == 0)
                            copy_field(book->first_document, item_hrefs + item * book_text_length,
                                       book_text_length);
                        break;
                    }
                }
            }
        }
        if ((token.type == xml::token_empty || token.type == xml::token_start) &&
            xml::name_is(&token, "item"))
        {
            char id[book_text_length] = {};
            char href[book_text_length] = {};
            char media_type[book_text_length] = {};
            if (attribute(&token, "id", id, sizeof(id)) &&
                attribute(&token, "href", href, sizeof(href)))
            {
                if (attribute(&token, "media-type", media_type, sizeof(media_type)) &&
                    strcmp(media_type, "application/x-dtbncx+xml") == 0)
                    copy_field(toc_href, href, sizeof(toc_href));
                for (uint8_t item = 0; item < book_spine_length; ++item)
                {
                    if (item_ids[item * book_text_length] == '\0')
                    {
                        copy_field(item_ids + item * book_text_length, id, book_text_length);
                        copy_field(item_hrefs + item * book_text_length, href, book_text_length);
                        break;
                    }
                }
            }
        }
    }
    for (uint8_t spine = 0; spine < book->spine_count; ++spine)
    {
        for (uint8_t item = 0; item < book_spine_length; ++item)
        {
            if (strcmp(item_ids + item * book_text_length, book->spine[spine].id) == 0)
            {
                copy_field(book->spine[spine].href, item_hrefs + item * book_text_length,
                           book_text_length);
                if (spine == 0)
                    copy_field(book->first_document, book->spine[spine].href, book_text_length);
                break;
            }
        }
    }
    if (toc_href[0] != '\0' && book->toc_count < book_toc_length)
    {
        char toc_path[book_text_length] = {};
        if (join_path(book->opf_directory, toc_href, toc_path))
        {
            char* toc = nullptr;
            size_t toc_size = 0;
            if (read_entry(&archive, toc_path, &toc, &toc_size) == ESP_OK)
            {
                xml::init(&reader, toc, toc_size);
                char title[book_text_length] = {};
                bool in_text = false;
                while (xml::next(&reader, &token) == ESP_OK && token.type != xml::token_eof)
                {
                    if (token.type == xml::token_start && xml::name_is(&token, "text"))
                        in_text = true;
                    else if (token.type == xml::token_end && xml::name_is(&token, "text"))
                        in_text = false;
                    else if (token.type == xml::token_text && in_text)
                        trim_copy(title, token.value, token.value_length);
                    else if (token.type == xml::token_empty && xml::name_is(&token, "content"))
                    {
                        char src[book_text_length] = {};
                        if (attribute(&token, "src", src, sizeof(src)) && title[0] != '\0')
                        {
                            char* fragment = strchr(src, '#');
                            if (fragment != nullptr)
                                *fragment = '\0';
                            const uint8_t index = book->toc_count++;
                            copy_field(book->toc[index].title, title, book_text_length);
                            copy_field(book->toc[index].href, src, book_text_length);
                            book->toc[index].spine_index = 0;
                            for (uint8_t spine = 0; spine < book->spine_count; ++spine)
                            {
                                if (strcmp(book->spine[spine].href, src) == 0)
                                {
                                    book->toc[index].spine_index = spine;
                                    break;
                                }
                            }
                            title[0] = '\0';
                            if (book->toc_count == book_toc_length)
                                break;
                        }
                    }
                }
                heap_caps_free(toc);
            }
        }
    }
    heap_caps_free(opf);
    heap_caps_free(spine_id);
    heap_caps_free(item_ids);
    heap_caps_free(item_hrefs);
    zip::close(&archive);
    if (book->title[0] == '\0')
    {
        const char* filename = strrchr(path, '/');
        filename = filename == nullptr ? path : filename + 1;
        copy_text(book->title, filename, strlen(filename));
        char* extension = strrchr(book->title, '.');
        if (extension != nullptr && strcasecmp(extension, ".epub") == 0)
            *extension = '\0';
    }
    return ESP_OK;
}

esp_err_t load_document(const char* path, const book_t* book, uint8_t spine_index,
                        document_t* document)
{
    if (path == nullptr || book == nullptr || document == nullptr ||
        spine_index >= book->spine_count)
        return ESP_ERR_INVALID_ARG;
    memset(document, 0, sizeof(*document));
    char entry_name[book_text_length] = {};
    if (!join_path(book->opf_directory, book->spine[spine_index].href, entry_name))
        return ESP_ERR_INVALID_SIZE;
    zip::archive_t archive = {};
    esp_err_t error = zip::open(&archive, path);
    if (error != ESP_OK)
        return error;
    char* data = nullptr;
    size_t size = 0;
    error = read_entry(&archive, entry_name, &data, &size);
    zip::close(&archive);
    if (error != ESP_OK)
        return error;
    xml::reader_t reader = {};
    xml::init(&reader, data, size);
    xml::token_t token = {};
    bool in_body = false;
    bool ignored = false;
    bool last_space = true;
    while (xml::next(&reader, &token) == ESP_OK && token.type != xml::token_eof)
    {
        if (token.type == xml::token_start && xml::name_is(&token, "body"))
            in_body = true;
        if (token.type == xml::token_end && xml::name_is(&token, "body"))
            in_body = false;
        if (token.type == xml::token_start &&
            (xml::name_is(&token, "head") || xml::name_is(&token, "style") ||
             xml::name_is(&token, "script")))
            ignored = true;
        if (token.type == xml::token_end &&
            (xml::name_is(&token, "head") || xml::name_is(&token, "style") ||
             xml::name_is(&token, "script")))
            ignored = false;
        if (!in_body || ignored)
            continue;
        if (token.type == xml::token_start &&
            (xml::name_is(&token, "p") || xml::name_is(&token, "br") ||
             xml::name_is(&token, "div") || xml::name_is(&token, "section") ||
             xml::name_is(&token, "h1") || xml::name_is(&token, "h2") ||
             xml::name_is(&token, "h3") || xml::name_is(&token, "li")))
        {
            if (document->length + 1 < document_text_length && document->length > 0)
                document->text[document->length++] = '\n';
            last_space = true;
        }
        if (token.type == xml::token_text)
            append_text(document, token.value, token.value_length, &last_space);
    }
    document->text[document->length] = '\0';
    heap_caps_free(data);
    return ESP_OK;
}

} // namespace epub
} // namespace xreader
