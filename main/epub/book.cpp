#include "book.hpp"

#include <ctype.h>
#include <string.h>

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
        while (start < end && (*start == ' ' || *start == '\t' || *start == '\n'))
            ++start;
        if (start + key_length > end || memcmp(start, key, key_length) != 0 ||
            (start + key_length < end && start[key_length] != '=' &&
             !isspace(static_cast<unsigned char>(start[key_length]))))
        {
            while (start < end && *start != ' ' && *start != '\t' && *start != '\n')
                ++start;
            continue;
        }
        start += key_length;
        while (start < end && (*start == ' ' || *start == '='))
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

static void directory_name(const char* path, char* output)
{
    strncpy(output, path, book_text_length - 1);
    char* slash = strrchr(output, '/');
    if (slash != nullptr)
        slash[1] = '\0';
    else
        output[0] = '\0';
}

static bool join_path(const char* directory, const char* href, char* output)
{
    const size_t directory_length = strlen(directory);
    const size_t href_length = strlen(href);
    if (directory_length + href_length >= book_text_length)
        return false;
    memcpy(output, directory, directory_length);
    memcpy(output + directory_length, href, href_length + 1);
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
    strncpy(book->opf_path, rootfile, book_text_length - 1);

    char* opf = nullptr;
    size_t opf_size = 0;
    error = read_entry(&archive, rootfile, &opf, &opf_size);
    if (error != ESP_OK)
    {
        zip::close(&archive);
        return error;
    }
    xml::init(&reader, opf, opf_size);
    char* spine_id = static_cast<char*>(heap_caps_calloc(1, book_text_length,
                                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    char* item_ids = static_cast<char*>(heap_caps_calloc(
        book_spine_length, book_text_length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    char* item_hrefs = static_cast<char*>(heap_caps_calloc(
        book_spine_length, book_text_length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
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
        if (token.type == xml::token_empty && xml::name_is(&token, "itemref"))
        {
            attribute(&token, "idref", spine_id, sizeof(spine_id));
            if (book->spine_count < book_spine_length && spine_id[0] != '\0')
            {
                const uint8_t index = book->spine_count++;
                strncpy(book->spine[index].id, spine_id, book_text_length - 1);
                for (uint8_t item = 0; item < book_spine_length; ++item)
                {
                    if (strcmp(item_ids + item * book_text_length, spine_id) == 0)
                    {
                        strncpy(book->spine[index].href,
                                item_hrefs + item * book_text_length, book_text_length - 1);
                        if (index == 0)
                            strncpy(book->first_document, item_hrefs + item * book_text_length,
                                    book_text_length - 1);
                        break;
                    }
                }
            }
        }
        if (token.type == xml::token_empty && xml::name_is(&token, "item"))
        {
            char id[book_text_length] = {};
            char href[book_text_length] = {};
            if (attribute(&token, "id", id, sizeof(id)) &&
                attribute(&token, "href", href, sizeof(href)))
            {
                for (uint8_t item = 0; item < book_spine_length; ++item)
                {
                    if (item_ids[item * book_text_length] == '\0')
                    {
                        strncpy(item_ids + item * book_text_length, id, book_text_length - 1);
                        strncpy(item_hrefs + item * book_text_length, href, book_text_length - 1);
                        break;
                    }
                }
            }
        }
    }
    heap_caps_free(opf);
    heap_caps_free(spine_id);
    heap_caps_free(item_ids);
    heap_caps_free(item_hrefs);
    zip::close(&archive);
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
    bool last_space = true;
    while (xml::next(&reader, &token) == ESP_OK && token.type != xml::token_eof)
    {
        if (token.type == xml::token_start &&
            (xml::name_is(&token, "body") || xml::name_is(&token, "html")))
            in_body = true;
        if (token.type == xml::token_end && xml::name_is(&token, "body"))
            in_body = false;
        if (!in_body)
            continue;
        if (token.type == xml::token_start &&
            (xml::name_is(&token, "p") || xml::name_is(&token, "br") ||
             xml::name_is(&token, "h1") || xml::name_is(&token, "h2") ||
             xml::name_is(&token, "li")))
        {
            if (document->length + 1 < document_text_length && document->length > 0)
                document->text[document->length++] = '\n';
            last_space = true;
        }
        if (token.type == xml::token_text)
        {
            for (size_t index = 0;
                 index < token.value_length && document->length + 1 < document_text_length; ++index)
            {
                const char value = token.value[index];
                if (isspace(static_cast<unsigned char>(value)))
                {
                    if (!last_space && document->length + 1 < document_text_length)
                        document->text[document->length++] = ' ';
                    last_space = true;
                }
                else
                {
                    document->text[document->length++] = value;
                    last_space = false;
                }
            }
        }
    }
    document->text[document->length] = '\0';
    heap_caps_free(data);
    return ESP_OK;
}

} // namespace epub
} // namespace xreader
