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

static char ascii_codepoint(uint32_t codepoint)
{
    if (codepoint >= 0x20U && codepoint <= 0x7eU)
        return static_cast<char>(codepoint);
    if (codepoint == 0x00d0U || codepoint == 0x0110U)
        return 'D';
    if (codepoint == 0x00f0U || codepoint == 0x0111U)
        return 'd';
    if (codepoint == 0x01a0U)
        return 'O';
    if (codepoint == 0x01a1U)
        return 'o';
    if (codepoint == 0x01afU)
        return 'U';
    if (codepoint == 0x01b0U)
        return 'u';
    if ((codepoint >= 0x00c0U && codepoint <= 0x00d6U) ||
        (codepoint >= 0x0100U && codepoint <= 0x024fU && (codepoint & 1U) == 0) ||
        (codepoint >= 0x1ea0U && codepoint <= 0x1ef9U && (codepoint & 1U) == 0))
    {
        if (codepoint <= 0x00c5U || (codepoint >= 0x0100U && codepoint <= 0x0105U) ||
            (codepoint >= 0x1ea0U && codepoint <= 0x1eb7U))
            return 'A';
        if (codepoint <= 0x00cbU || (codepoint >= 0x0118U && codepoint <= 0x011bU) ||
            (codepoint >= 0x1eb8U && codepoint <= 0x1ec7U))
            return 'E';
        if (codepoint <= 0x00cfU || (codepoint >= 0x0128U && codepoint <= 0x012fU) ||
            (codepoint >= 0x1ec8U && codepoint <= 0x1ecbU))
            return 'I';
        if (codepoint <= 0x00d6U || (codepoint >= 0x014cU && codepoint <= 0x0151U) ||
            (codepoint >= 0x1eccU && codepoint <= 0x1ee3U))
            return 'O';
        return 'U';
    }
    if ((codepoint >= 0x00e0U && codepoint <= 0x00ffU) ||
        (codepoint >= 0x0100U && codepoint <= 0x024fU) ||
        (codepoint >= 0x1ea0U && codepoint <= 0x1effU))
    {
        if (codepoint <= 0x00e5U || (codepoint >= 0x0101U && codepoint <= 0x0105U) ||
            (codepoint >= 0x1ea1U && codepoint <= 0x1eb7U))
            return 'a';
        if (codepoint <= 0x00ebU || (codepoint >= 0x0119U && codepoint <= 0x011bU) ||
            (codepoint >= 0x1eb9U && codepoint <= 0x1ec7U))
            return 'e';
        if (codepoint <= 0x00efU || (codepoint >= 0x0129U && codepoint <= 0x012fU) ||
            (codepoint >= 0x1ec9U && codepoint <= 0x1ecbU))
            return 'i';
        if (codepoint <= 0x00f6U || (codepoint >= 0x014dU && codepoint <= 0x0151U) ||
            (codepoint >= 0x1ecdU && codepoint <= 0x1ee3U))
            return 'o';
        return 'u';
    }
    return '?';
}

static void copy_text(char* destination, const char* source, size_t length)
{
    size_t output_length = 0;
    size_t input = 0;
    while (input < length && output_length + 1 < book_text_length)
    {
        uint32_t codepoint = 0;
        size_t consumed = 1;
        const uint8_t first = static_cast<uint8_t>(source[input]);
        if (first < 0x80U)
        {
            codepoint = first;
        }
        else if ((first & 0xe0U) == 0xc0U && input + 1 < length)
        {
            codepoint = static_cast<uint32_t>(first & 0x1fU) << 6;
            codepoint |= static_cast<uint8_t>(source[input + 1]) & 0x3fU;
            consumed = 2;
        }
        else if ((first & 0xf0U) == 0xe0U && input + 2 < length)
        {
            codepoint = static_cast<uint32_t>(first & 0x0fU) << 12;
            codepoint |= static_cast<uint32_t>(static_cast<uint8_t>(source[input + 1]) & 0x3fU)
                         << 6;
            codepoint |= static_cast<uint8_t>(source[input + 2]) & 0x3fU;
            consumed = 3;
        }
        else
        {
            codepoint = '?';
        }
        destination[output_length++] = ascii_codepoint(codepoint);
        input += consumed;
    }
    destination[output_length] = '\0';
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
            uint32_t codepoint = 0;
            size_t consumed = 1;
            if ((first & 0xe0U) == 0xc0U && index + 1 < length)
            {
                codepoint = static_cast<uint32_t>(first & 0x1fU) << 6;
                codepoint |= static_cast<uint8_t>(source[index + 1]) & 0x3fU;
                consumed = 2;
            }
            else if ((first & 0xf0U) == 0xe0U && index + 2 < length)
            {
                codepoint = static_cast<uint32_t>(first & 0x0fU) << 12;
                codepoint |= static_cast<uint32_t>(static_cast<uint8_t>(source[index + 1]) & 0x3fU)
                             << 6;
                codepoint |= static_cast<uint8_t>(source[index + 2]) & 0x3fU;
                consumed = 3;
            }
            value = '?';
            if ((codepoint >= 0x00c0U && codepoint <= 0x00ffU) ||
                (codepoint >= 0x0100U && codepoint <= 0x024fU) ||
                (codepoint >= 0x1ea0U && codepoint <= 0x1effU))
            {
                if (codepoint == 0x00d0U || codepoint == 0x0110U)
                    value = 'D';
                else if (codepoint == 0x00f0U || codepoint == 0x0111U)
                    value = 'd';
                else if ((codepoint >= 0x00c0U && codepoint <= 0x00c5U) ||
                         (codepoint >= 0x0100U && codepoint <= 0x0105U) ||
                         (codepoint >= 0x1ea0U && codepoint <= 0x1eb7U))
                    value = 'A';
                else if ((codepoint >= 0x00e0U && codepoint <= 0x00e5U) ||
                         (codepoint >= 0x0101U && codepoint <= 0x0106U) ||
                         (codepoint >= 0x1ea1U && codepoint <= 0x1eb7U))
                    value = 'a';
                else if ((codepoint >= 0x00c8U && codepoint <= 0x00cbU) ||
                         (codepoint >= 0x0118U && codepoint <= 0x011bU) ||
                         (codepoint >= 0x1eb8U && codepoint <= 0x1ec7U))
                    value = 'E';
                else if ((codepoint >= 0x00e8U && codepoint <= 0x00ebU) ||
                         (codepoint >= 0x0119U && codepoint <= 0x011cU) ||
                         (codepoint >= 0x1eb9U && codepoint <= 0x1ec7U))
                    value = 'e';
                else if ((codepoint >= 0x00ccU && codepoint <= 0x00cfU) ||
                         (codepoint >= 0x0128U && codepoint <= 0x012fU) ||
                         (codepoint >= 0x1ec8U && codepoint <= 0x1ecbU))
                    value = 'I';
                else if ((codepoint >= 0x00ecU && codepoint <= 0x00efU) ||
                         (codepoint >= 0x0129U && codepoint <= 0x0130U) ||
                         (codepoint >= 0x1ec9U && codepoint <= 0x1ecbU))
                    value = 'i';
                else if ((codepoint >= 0x00d2U && codepoint <= 0x00d6U) ||
                         (codepoint >= 0x014cU && codepoint <= 0x0151U) ||
                         (codepoint >= 0x1eccU && codepoint <= 0x1ee3U))
                    value = 'O';
                else if ((codepoint >= 0x00f2U && codepoint <= 0x00f6U) ||
                         (codepoint >= 0x014dU && codepoint <= 0x0152U) ||
                         (codepoint >= 0x1ecdU && codepoint <= 0x1ee3U))
                    value = 'o';
                else if ((codepoint >= 0x00d9U && codepoint <= 0x00dcU) ||
                         (codepoint >= 0x0168U && codepoint <= 0x016fU) ||
                         (codepoint >= 0x1ee4U && codepoint <= 0x1ef1U))
                    value = 'U';
                else if ((codepoint >= 0x00f9U && codepoint <= 0x00fcU) ||
                         (codepoint >= 0x0169U && codepoint <= 0x0170U) ||
                         (codepoint >= 0x1ee5U && codepoint <= 0x1ef1U))
                    value = 'u';
                else if (codepoint >= 0x00ddU && codepoint <= 0x00deU)
                    value = 'Y';
                else if (codepoint >= 0x00fdU && codepoint <= 0x00ffU)
                    value = 'y';
                else if (codepoint == 0x01a0U)
                    value = 'O';
                else if (codepoint == 0x01a1U)
                    value = 'o';
                else if (codepoint == 0x01afU)
                    value = 'U';
                else if (codepoint == 0x01b0U)
                    value = 'u';
            }
            index += consumed;
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
    strncpy(output, path, book_text_length - 1);
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
    strncpy(book->opf_path, normalized_rootfile, book_text_length - 1);
    book->opf_path[book_text_length - 1] = '\0';

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
                        strncpy(book->spine[index].href, item_hrefs + item * book_text_length,
                                book_text_length - 1);
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
            char media_type[book_text_length] = {};
            if (attribute(&token, "id", id, sizeof(id)) &&
                attribute(&token, "href", href, sizeof(href)))
            {
                if (attribute(&token, "media-type", media_type, sizeof(media_type)) &&
                    strcmp(media_type, "application/x-dtbncx+xml") == 0)
                    strncpy(toc_href, href, sizeof(toc_href) - 1);
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
                            strncpy(book->toc[index].title, title, book_text_length - 1);
                            strncpy(book->toc[index].href, src, book_text_length - 1);
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
