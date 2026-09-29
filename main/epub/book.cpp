#include "book.hpp"

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "esp_heap_caps.h"
#include "inflate.hpp"
#include "css.hpp"
#include "image.hpp"
#include "xml.hpp"
#include "zip.hpp"

#include "gfx/font.hpp"

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

static size_t encode_utf8(uint32_t codepoint, char output[4])
{
    if (codepoint <= 0x7fU)
    {
        output[0] = static_cast<char>(codepoint);
        return 1;
    }
    if (codepoint <= 0x7ffU)
    {
        output[0] = static_cast<char>(0xc0U | (codepoint >> 6U));
        output[1] = static_cast<char>(0x80U | (codepoint & 0x3fU));
        return 2;
    }
    if (codepoint <= 0xffffU)
    {
        output[0] = static_cast<char>(0xe0U | (codepoint >> 12U));
        output[1] = static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU));
        output[2] = static_cast<char>(0x80U | (codepoint & 0x3fU));
        return 3;
    }
    if (codepoint <= 0x10ffffU)
    {
        output[0] = static_cast<char>(0xf0U | (codepoint >> 18U));
        output[1] = static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3fU));
        output[2] = static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU));
        output[3] = static_cast<char>(0x80U | (codepoint & 0x3fU));
        return 4;
    }
    output[0] = '?';
    return 1;
}

static bool is_combining(uint32_t codepoint)
{
    return codepoint >= 0x0300U && codepoint <= 0x036fU;
}

static bool vietnamese_shape_mark(uint32_t codepoint)
{
    return codepoint == 0x0302U || codepoint == 0x0306U || codepoint == 0x031bU;
}

static bool parse_entity(const char* source, size_t length, uint32_t* codepoint)
{
    if (source == nullptr || length < 3U || source[0] != '&' || source[length - 1U] != ';' ||
        codepoint == nullptr)
        return false;
    if (length == 5U && memcmp(source, "&amp;", 5U) == 0) { *codepoint = '&'; return true; }
    if (length == 4U && memcmp(source, "&lt;", 4U) == 0) { *codepoint = '<'; return true; }
    if (length == 4U && memcmp(source, "&gt;", 4U) == 0) { *codepoint = '>'; return true; }
    if (length == 6U && memcmp(source, "&quot;", 6U) == 0) { *codepoint = '"'; return true; }
    if (length == 6U && memcmp(source, "&apos;", 6U) == 0) { *codepoint = '\''; return true; }
    if (length == 6U && memcmp(source, "&nbsp;", 6U) == 0) { *codepoint = 0x00a0U; return true; }
    if (source[1] != '#')
        return false;
    uint32_t value = 0;
    size_t index = 2U;
    int base = 10;
    if (index < length - 1U && (source[index] == 'x' || source[index] == 'X'))
    {
        base = 16;
        ++index;
    }
    if (index >= length - 1U)
        return false;
    for (; index < length - 1U; ++index)
    {
        int digit = -1;
        const char c = source[index];
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        if (digit < 0 || digit >= base)
            return false;
        if (value > (0x10ffffU - static_cast<uint32_t>(digit)) / static_cast<uint32_t>(base))
            return false;
        value = value * static_cast<uint32_t>(base) + static_cast<uint32_t>(digit);
    }
    if (value == 0U || value > 0x10ffffU || (value >= 0xd800U && value <= 0xdfffU))
        return false;
    *codepoint = value;
    return true;
}

static void append_codepoint(document_t* document, uint32_t codepoint, bool* last_space)
{
    if (document == nullptr || last_space == nullptr)
        return;
    if (codepoint == 0x00a0U || (codepoint <= 0x7fU && isspace(static_cast<unsigned char>(codepoint))))
    {
        if (!*last_space && document->length + 1U < document_text_length)
            document->text[document->length++] = ' ';
        *last_space = true;
        return;
    }
    char encoded[4] = {};
    const size_t bytes = encode_utf8(codepoint, encoded);
    if (document->length + bytes >= document_text_length)
        return;
    memcpy(document->text + document->length, encoded, bytes);
    document->length += bytes;
    *last_space = false;
}

static void append_text(document_t* document, const char* source, size_t length, bool* last_space,
                        bool preserve_whitespace = false)
{
    if (document == nullptr || source == nullptr || last_space == nullptr)
        return;
    size_t index = 0;
    while (index < length && document->length + 1U < document_text_length)
    {
        uint32_t first = 0;
        size_t first_bytes = 0;
        if (source[index] == '&')
        {
            const char* end = static_cast<const char*>(memchr(source + index, ';', length - index));
            if (end != nullptr)
            {
                const size_t entity_bytes = static_cast<size_t>(end - (source + index)) + 1U;
                if (parse_entity(source + index, entity_bytes, &first))
                {
                    first_bytes = entity_bytes;
                }
            }
        }
        if (first_bytes == 0U)
        {
            first_bytes = gfx::decode_utf8(source + index, &first);
            if (first_bytes == 0U || index + first_bytes > length)
            {
                first = static_cast<unsigned char>(source[index]);
                first_bytes = 1U;
            }
        }

        uint32_t second = 0;
        uint32_t third = 0;
        size_t second_bytes = 0;
        size_t third_bytes = 0;
        if (index + first_bytes < length)
        {
            second_bytes = gfx::decode_utf8(source + index + first_bytes, &second);
            if (second_bytes == 0U || index + first_bytes + second_bytes > length ||
                !is_combining(second))
                second_bytes = 0U;
        }
        if (second_bytes > 0U && index + first_bytes + second_bytes < length)
        {
            third_bytes = gfx::decode_utf8(source + index + first_bytes + second_bytes, &third);
            if (third_bytes == 0U || index + first_bytes + second_bytes + third_bytes > length ||
                !is_combining(third))
                third_bytes = 0U;
        }

        // Canonically equivalent Vietnamese text is common in EPUBs.  Put the
        // shape mark (horn/breve/circumflex) before the tone mark before looking
        // up the generated NFC composition table.
        if (second_bytes > 0U && third_bytes > 0U && !vietnamese_shape_mark(second) &&
            vietnamese_shape_mark(third))
        {
            const uint32_t temporary = second;
            second = third;
            third = temporary;
            const size_t temporary_bytes = second_bytes;
            second_bytes = third_bytes;
            third_bytes = temporary_bytes;
        }

        uint32_t composed = 0;
        size_t consumed_codepoints = 0;
        if (second_bytes > 0U &&
            gfx::compose_unicode(first, second, third_bytes > 0U ? third : 0U, &composed,
                                 &consumed_codepoints))
        {
            append_codepoint(document, composed, last_space);
            index += first_bytes + second_bytes;
            if (consumed_codepoints == 3U)
                index += third_bytes;
            continue;
        }

        if (preserve_whitespace && first <= 0x7fU &&
            (first == '\t' || first == '\r' || first == '\n' || first == ' '))
        {
            const char normalized = first == '\r' ? '\n' : static_cast<char>(first);
            if (document->length + 1U < document_text_length)
            {
                document->text[document->length++] = normalized;
                *last_space = false;
            }
        }
        else
        {
            append_codepoint(document, first, last_space);
        }
        index += first_bytes;
    }
}

static void append_break(document_t* document, bool* last_space, bool force_double = false)
{
    if (document == nullptr || last_space == nullptr)
        return;
    if (document->length > 0U && document->text[document->length - 1U] != '\n' &&
        document->length + 1U < document_text_length)
        document->text[document->length++] = '\n';
    if (force_double && document->length > 0U && document->length + 1U < document_text_length &&
        (document->length < 2U || document->text[document->length - 2U] != '\n'))
        document->text[document->length++] = '\n';
    *last_space = true;
}

static bool contains_word_ci(const char* text, const char* word)
{
    if (text == nullptr || word == nullptr || word[0] == '\0')
        return false;
    const size_t word_length = strlen(word);
    for (const char* cursor = text; *cursor != '\0'; ++cursor)
    {
        if (cursor != text && !isspace(static_cast<unsigned char>(cursor[-1])))
            continue;
        size_t index = 0;
        while (index < word_length && cursor[index] != '\0' &&
               tolower(static_cast<unsigned char>(cursor[index])) ==
                   tolower(static_cast<unsigned char>(word[index])))
            ++index;
        if (index == word_length &&
            (cursor[index] == '\0' || isspace(static_cast<unsigned char>(cursor[index]))))
            return true;
    }
    return false;
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
    char cover_id[book_text_length] = {};
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
            xml::name_is(&token, "meta"))
        {
            char name[book_text_length] = {};
            char content[book_text_length] = {};
            if (attribute(&token, "name", name, sizeof(name)) &&
                attribute(&token, "content", content, sizeof(content)) &&
                strcasecmp(name, "cover") == 0)
                copy_field(cover_id, content, sizeof(cover_id));
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
            char properties[book_text_length] = {};
            if (attribute(&token, "id", id, sizeof(id)) &&
                attribute(&token, "href", href, sizeof(href)))
            {
                if (attribute(&token, "media-type", media_type, sizeof(media_type)) &&
                    strcmp(media_type, "application/x-dtbncx+xml") == 0)
                    copy_field(toc_href, href, sizeof(toc_href));
                if (attribute(&token, "properties", properties, sizeof(properties)) &&
                    contains_word_ci(properties, "cover-image"))
                    join_path(book->opf_directory, href, book->cover_href);
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
    if (book->cover_href[0] == '\0' && cover_id[0] != '\0')
    {
        for (uint8_t item = 0; item < book_spine_length; ++item)
        {
            if (strcmp(item_ids + item * book_text_length, cover_id) == 0)
            {
                join_path(book->opf_directory, item_hrefs + item * book_text_length,
                          book->cover_href);
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

esp_err_t load_resource(const char* path, const char* archive_href, uint8_t** data, size_t* size)
{
    if (path == nullptr || archive_href == nullptr || data == nullptr || size == nullptr)
        return ESP_ERR_INVALID_ARG;
    *data = nullptr;
    *size = 0;
    zip::archive_t archive = {};
    esp_err_t error = zip::open(&archive, path);
    if (error != ESP_OK)
        return error;
    char* buffer = nullptr;
    error = read_entry(&archive, archive_href, &buffer, size);
    zip::close(&archive);
    if (error != ESP_OK)
        return error;
    *data = reinterpret_cast<uint8_t*>(buffer);
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
    char document_directory[book_text_length] = {};
    directory_name(entry_name, document_directory);

    zip::archive_t archive = {};
    esp_err_t error = zip::open(&archive, path);
    if (error != ESP_OK)
        return error;
    char* data = nullptr;
    size_t size = 0;
    error = read_entry(&archive, entry_name, &data, &size);
    if (error != ESP_OK)
    {
        zip::close(&archive);
        return error;
    }

    xml::reader_t reader = {};
    xml::init(&reader, data, size);
    xml::token_t token = {};
    bool in_body = false;
    bool ignored = false;
    bool last_space = true;
    bool preformatted = false;
    uint16_t depth = 0;
    uint16_t pre_depth = 0;
    uint16_t hidden_depth = 0;

    while (xml::next(&reader, &token) == ESP_OK && token.type != xml::token_eof)
    {
        if (token.type == xml::token_start)
            ++depth;

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

        if (hidden_depth > 0U)
        {
            if (token.type == xml::token_start)
                ++hidden_depth;
            else if (token.type == xml::token_end)
                --hidden_depth;
            if (token.type == xml::token_end && depth > 0U)
                --depth;
            continue;
        }

        if (!in_body || ignored)
        {
            if (token.type == xml::token_end && depth > 0U)
                --depth;
            continue;
        }

        css::style_t inline_style = {};
        if (token.type == xml::token_start || token.type == xml::token_empty)
        {
            char style_text[256] = {};
            if (attribute(&token, "style", style_text, sizeof(style_text)))
                inline_style = css::parse_inline(style_text);
            if (inline_style.hidden)
            {
                if (token.type == xml::token_start)
                    hidden_depth = 1U;
                if (token.type == xml::token_end && depth > 0U)
                    --depth;
                continue;
            }
            if (inline_style.page_break_before)
                append_break(document, &last_space, true);
        }

        if (token.type == xml::token_start &&
            (xml::name_is(&token, "pre") || inline_style.preformatted))
        {
            preformatted = true;
            pre_depth = depth;
        }

        const bool structural_block =
            (token.type == xml::token_start || token.type == xml::token_empty) &&
            (xml::name_is(&token, "p") || xml::name_is(&token, "br") ||
             xml::name_is(&token, "div") || xml::name_is(&token, "section") ||
             xml::name_is(&token, "article") || xml::name_is(&token, "blockquote") ||
             xml::name_is(&token, "h1") || xml::name_is(&token, "h2") ||
             xml::name_is(&token, "h3") || xml::name_is(&token, "h4") ||
             xml::name_is(&token, "li") || xml::name_is(&token, "pre"));
        if (structural_block || inline_style.block)
            append_break(document, &last_space,
                         xml::name_is(&token, "h1") || xml::name_is(&token, "h2"));
        if ((token.type == xml::token_start || token.type == xml::token_empty) &&
            xml::name_is(&token, "li") && document->length + 2U < document_text_length)
        {
            document->text[document->length++] = '-';
            document->text[document->length++] = ' ';
            last_space = false;
        }

        if ((token.type == xml::token_start || token.type == xml::token_empty) &&
            xml::name_is(&token, "img") && document->image_count < document_image_count)
        {
            char src[book_text_length] = {};
            char image_href[book_text_length] = {};
            if (attribute(&token, "src", src, sizeof(src)) &&
                join_path(document_directory, src, image_href))
            {
                zip::entry_t image_entry = {};
                if (zip::find(&archive, image_href, &image_entry) == ESP_OK &&
                    image_entry.uncompressed_size > 0U && image_entry.uncompressed_size <= 2U * 1024U * 1024U)
                {
                    uint8_t* image_data = static_cast<uint8_t*>(heap_caps_malloc(
                        image_entry.uncompressed_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
                    if (image_data != nullptr)
                    {
                        size_t image_size = 0;
                        image::info_t info = {};
                        if (zip::read(&archive, &image_entry, image_data, image_entry.uncompressed_size,
                                      &image_size) == ESP_OK &&
                            image::inspect(image_data, image_size, &info) == ESP_OK)
                        {
                            append_break(document, &last_space);
                            document_image_t& image_ref = document->images[document->image_count++];
                            image_ref.text_offset = document->length;
                            copy_field(image_ref.href, image_href, sizeof(image_ref.href));
                            image_ref.width = info.width;
                            image_ref.height = info.height;
                            char marker[4] = {};
                            const size_t marker_bytes = encode_utf8(0xfffcU, marker);
                            if (document->length + marker_bytes < document_text_length)
                            {
                                memcpy(document->text + document->length, marker, marker_bytes);
                                document->length += marker_bytes;
                            }
                            append_break(document, &last_space);
                        }
                        heap_caps_free(image_data);
                    }
                }
            }
        }
        else if (token.type == xml::token_text)
        {
            append_text(document, token.value, token.value_length, &last_space, preformatted);
        }

        if (token.type == xml::token_end)
        {
            if (preformatted && pre_depth == depth)
            {
                preformatted = false;
                pre_depth = 0;
            }
            if (depth > 0U)
                --depth;
        }
    }
    document->text[document->length] = '\0';
    heap_caps_free(data);
    zip::close(&archive);
    return ESP_OK;
}

} // namespace epub
} // namespace xreader
