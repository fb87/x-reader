#include "book.hpp"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "css.hpp"
#include "esp_heap_caps.h"
#include "image.hpp"
#include "inflate.hpp"
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

// PSRAM-less boards (e.g. XTeink/esp32c3) have zero MALLOC_CAP_SPIRAM-capable
// regions, and heap_caps_malloc requires every requested capability bit to be
// satisfiable -- a bare SPIRAM|8BIT request always returns null there. Prefer
// SPIRAM when it exists, but fall back to plain internal RAM everywhere in
// this file so EPUB parsing works on both boards.
static void* alloc_prefer_spiram(size_t size)
{
    void* buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer == nullptr)
        buffer = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    return buffer;
}

static void* calloc_prefer_spiram(size_t count, size_t size)
{
    void* buffer = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer == nullptr)
        buffer = heap_caps_calloc(count, size, MALLOC_CAP_8BIT);
    return buffer;
}

static esp_err_t read_entry(zip::archive_t* archive, const char* name, char** data, size_t* size)
{
    zip::entry_t entry = {};
    esp_err_t error = zip::find(archive, name, &entry);
    if (error != ESP_OK)
        return error;
    char* buffer = static_cast<char*>(alloc_prefer_spiram(entry.uncompressed_size + 1));
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
    if (length == 5U && memcmp(source, "&amp;", 5U) == 0)
    {
        *codepoint = '&';
        return true;
    }
    if (length == 4U && memcmp(source, "&lt;", 4U) == 0)
    {
        *codepoint = '<';
        return true;
    }
    if (length == 4U && memcmp(source, "&gt;", 4U) == 0)
    {
        *codepoint = '>';
        return true;
    }
    if (length == 6U && memcmp(source, "&quot;", 6U) == 0)
    {
        *codepoint = '"';
        return true;
    }
    if (length == 6U && memcmp(source, "&apos;", 6U) == 0)
    {
        *codepoint = '\'';
        return true;
    }
    if (length == 6U && memcmp(source, "&nbsp;", 6U) == 0)
    {
        *codepoint = 0x00a0U;
        return true;
    }
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
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
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

// A chapter's cache directory sits beside the EPUB file itself (not under the
// SD mount root, unlike the library catalog cache) so book.cpp never needs to
// know the SD mount path -- callers keep passing just the book's own file
// path, exactly as load_metadata()/load_document() always have.
static constexpr uint32_t chapter_cache_magic = 0x50484358U; // "XCHP"
static constexpr uint16_t chapter_cache_version = 1;

struct chapter_cache_header_t
{
    uint32_t magic;
    uint16_t version;
    uint16_t image_count;
    uint32_t source_size;
    int64_t source_mtime;
    uint32_t text_length;
    uint32_t checksum; // FNV-1a-32 over the cached text file's bytes
    document_image_t images[document_image_count];
};

// Accumulates one chapter's decoded text into a small flush buffer, writing
// it out to the (temp) cache file as it fills, instead of an unbounded
// in-RAM buffer. document_image_t offsets are recorded against `length`,
// which tracks the same cumulative logical position append_codepoint et al.
// always have, just backed by a file instead of a fixed array now.
struct chapter_builder_t
{
    FILE* file;
    char flush_buffer[2048];
    size_t flush_used;
    size_t length;
    uint32_t checksum;
    uint8_t image_count;
    document_image_t images[document_image_count];
    char trailing[2]; // last two bytes written, for append_break()'s "already ends in \n" checks
    bool error;
};

static uint32_t book_hash(const char* path)
{
    uint32_t hash = 2166136261U;
    for (const char* cursor = path; *cursor != '\0'; ++cursor)
        hash = (hash ^ static_cast<uint8_t>(*cursor)) * 16777619U;
    return hash;
}

// Like directory_name(), but for real filesystem paths (up to book_path_length
// in main/ui/library.hpp, i.e. 512 bytes) rather than in-archive entry names,
// which directory_name()'s book_text_length (128) cap would silently truncate.
static bool filesystem_directory_name(const char* path, char* output, size_t capacity)
{
    const char* slash = strrchr(path, '/');
    const size_t length = slash == nullptr ? 0U : static_cast<size_t>(slash - path);
    if (length + 1U > capacity)
        return false;
    memcpy(output, path, length);
    output[length] = '\0';
    return true;
}

static bool ensure_directory(const char* path)
{
    if (mkdir(path, 0755) == 0)
        return true;
    struct stat info = {};
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

// Creates <dirname(book_path)>/.xreader-chapters/<hash8>/ (mkdir is not
// recursive, so both levels are created explicitly) and writes it to
// `leaf_directory`.
static bool ensure_chapter_cache_directory(const char* book_path, char* leaf_directory,
                                           size_t capacity)
{
    char directory[document_cache_path_length] = {};
    if (!filesystem_directory_name(book_path, directory, sizeof(directory)))
        return false;
    char chapters_directory[document_cache_path_length] = {};
    const int chapters_written = snprintf(chapters_directory, sizeof(chapters_directory),
                                          "%s/.xreader-chapters", directory);
    if (chapters_written < 0 || static_cast<size_t>(chapters_written) >= sizeof(chapters_directory))
        return false;
    if (!ensure_directory(chapters_directory))
        return false;
    const int leaf_written = snprintf(leaf_directory, capacity, "%s/%08lx", chapters_directory,
                                      static_cast<unsigned long>(book_hash(book_path)));
    if (leaf_written < 0 || static_cast<size_t>(leaf_written) >= capacity)
        return false;
    return ensure_directory(leaf_directory);
}

static bool resolve_cache_paths(const char* book_path, uint8_t spine_index, char* text_path,
                                char* header_path, size_t capacity)
{
    char directory[document_cache_path_length] = {};
    if (!filesystem_directory_name(book_path, directory, sizeof(directory)))
        return false;
    const uint32_t hash = book_hash(book_path);
    const int text_written =
        snprintf(text_path, capacity, "%s/.xreader-chapters/%08lx/ch%03u.txt", directory,
                static_cast<unsigned long>(hash), static_cast<unsigned>(spine_index));
    if (text_written < 0 || static_cast<size_t>(text_written) >= capacity)
        return false;
    const int header_written =
        snprintf(header_path, capacity, "%s/.xreader-chapters/%08lx/ch%03u.hdr", directory,
                static_cast<unsigned long>(hash), static_cast<unsigned>(spine_index));
    if (header_written < 0 || static_cast<size_t>(header_written) >= capacity)
        return false;
    return true;
}

static esp_err_t read_chapter_header(const char* header_path, chapter_cache_header_t* header)
{
    FILE* file = fopen(header_path, "rb");
    if (file == nullptr)
        return ESP_ERR_NOT_FOUND;
    const size_t read = fread(header, 1, sizeof(*header), file);
    fclose(file);
    if (read != sizeof(*header) || header->magic != chapter_cache_magic ||
        header->version != chapter_cache_version)
        return ESP_FAIL;
    return ESP_OK;
}

// Re-reads the cached text file sequentially to confirm it matches the
// header's checksum/length -- a plain streamed read, far cheaper than the
// XML parse it lets us skip, but still enough to catch a corrupt/truncated
// cache (e.g. an interrupted write on a previous, differently-crashed run).
static esp_err_t verify_chapter_text(const char* text_path, uint32_t expected_checksum,
                                     uint32_t expected_length)
{
    FILE* file = fopen(text_path, "rb");
    if (file == nullptr)
        return ESP_ERR_NOT_FOUND;
    uint32_t checksum = 2166136261U;
    uint32_t total = 0U;
    uint8_t buffer[512];
    size_t read = 0;
    while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0U)
    {
        for (size_t index = 0; index < read; ++index)
            checksum = (checksum ^ buffer[index]) * 16777619U;
        total += static_cast<uint32_t>(read);
    }
    fclose(file);
    if (total != expected_length || checksum != expected_checksum)
        return ESP_FAIL;
    return ESP_OK;
}

static void builder_flush(chapter_builder_t* builder)
{
    if (builder->error || builder->flush_used == 0U)
        return;
    if (fwrite(builder->flush_buffer, 1, builder->flush_used, builder->file) != builder->flush_used)
        builder->error = true;
    builder->flush_used = 0U;
}

static void builder_put_bytes(chapter_builder_t* builder, const char* data, size_t length)
{
    if (builder == nullptr || builder->error || length == 0U)
        return;
    if (length >= 2U)
    {
        builder->trailing[0] = data[length - 2U];
        builder->trailing[1] = data[length - 1U];
    }
    else
    {
        builder->trailing[0] = builder->trailing[1];
        builder->trailing[1] = data[0];
    }
    size_t offset = 0;
    while (offset < length)
    {
        const size_t space = sizeof(builder->flush_buffer) - builder->flush_used;
        const size_t chunk = (length - offset) < space ? (length - offset) : space;
        memcpy(builder->flush_buffer + builder->flush_used, data + offset, chunk);
        builder->flush_used += chunk;
        offset += chunk;
        if (builder->flush_used == sizeof(builder->flush_buffer))
            builder_flush(builder);
    }
    for (size_t index = 0; index < length; ++index)
        builder->checksum =
            (builder->checksum ^ static_cast<uint8_t>(data[index])) * 16777619U;
    builder->length += length;
}

static void append_codepoint(chapter_builder_t* builder, uint32_t codepoint, bool* last_space)
{
    if (builder == nullptr || last_space == nullptr)
        return;
    if (codepoint == 0x00a0U ||
        (codepoint <= 0x7fU && isspace(static_cast<unsigned char>(codepoint))))
    {
        if (!*last_space && builder->length + 1U < chapter_text_length_limit)
            builder_put_bytes(builder, " ", 1);
        *last_space = true;
        return;
    }
    char encoded[4] = {};
    const size_t bytes = encode_utf8(codepoint, encoded);
    if (builder->length + bytes >= chapter_text_length_limit)
        return;
    builder_put_bytes(builder, encoded, bytes);
    *last_space = false;
}

static void append_text(chapter_builder_t* builder, const char* source, size_t length,
                        bool* last_space, bool preserve_whitespace = false)
{
    if (builder == nullptr || source == nullptr || last_space == nullptr)
        return;
    size_t index = 0;
    while (index < length && builder->length + 1U < chapter_text_length_limit)
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
        if (second_bytes > 0U && gfx::compose_unicode(first, second, third_bytes > 0U ? third : 0U,
                                                      &composed, &consumed_codepoints))
        {
            append_codepoint(builder, composed, last_space);
            index += first_bytes + second_bytes;
            if (consumed_codepoints == 3U)
                index += third_bytes;
            continue;
        }

        if (preserve_whitespace && first <= 0x7fU &&
            (first == '\t' || first == '\r' || first == '\n' || first == ' '))
        {
            const char normalized = first == '\r' ? '\n' : static_cast<char>(first);
            if (builder->length + 1U < chapter_text_length_limit)
            {
                builder_put_bytes(builder, &normalized, 1);
                *last_space = false;
            }
        }
        else
        {
            append_codepoint(builder, first, last_space);
        }
        index += first_bytes;
    }
}

static void append_break(chapter_builder_t* builder, bool* last_space, bool force_double = false)
{
    if (builder == nullptr || last_space == nullptr)
        return;
    if (builder->length > 0U && builder->trailing[1] != '\n' &&
        builder->length + 1U < chapter_text_length_limit)
        builder_put_bytes(builder, "\n", 1);
    if (force_double && builder->length > 0U && builder->length + 1U < chapter_text_length_limit &&
        (builder->length < 2U || builder->trailing[0] != '\n'))
        builder_put_bytes(builder, "\n", 1);
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
    char* spine_id = static_cast<char*>(calloc_prefer_spiram(1, book_text_length));
    char* item_ids = static_cast<char*>(calloc_prefer_spiram(book_spine_length, book_text_length));
    char* item_hrefs = static_cast<char*>(calloc_prefer_spiram(book_spine_length, book_text_length));
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

    struct stat source_info = {};
    if (stat(path, &source_info) != 0)
        return ESP_ERR_NOT_FOUND;

    char text_path[document_cache_path_length] = {};
    char header_path[document_cache_path_length] = {};
    if (!resolve_cache_paths(path, spine_index, text_path, header_path, document_cache_path_length))
        return ESP_ERR_INVALID_SIZE;
    copy_field(document->cache_path, text_path, sizeof(document->cache_path));

    chapter_cache_header_t header = {};
    if (read_chapter_header(header_path, &header) == ESP_OK &&
        header.source_size == static_cast<uint32_t>(source_info.st_size) &&
        header.source_mtime == static_cast<int64_t>(source_info.st_mtime) &&
        verify_chapter_text(text_path, header.checksum, header.text_length) == ESP_OK)
    {
        document->length = header.text_length;
        document->image_count = static_cast<uint8_t>(header.image_count);
        memcpy(document->images, header.images, sizeof(document->images));
        return ESP_OK;
    }

    // Cache miss or stale (source EPUB changed, or no cache yet) -- parse the
    // chapter now, same XML walk as before, just targeting a chapter_builder_t
    // that streams to the (temp) cache file instead of an in-RAM buffer.
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

    char cache_directory[document_cache_path_length] = {};
    if (!ensure_chapter_cache_directory(path, cache_directory, sizeof(cache_directory)))
    {
        heap_caps_free(data);
        zip::close(&archive);
        return ESP_FAIL;
    }
    char temp_text_path[document_cache_path_length] = {};
    if (static_cast<size_t>(snprintf(temp_text_path, sizeof(temp_text_path), "%s.tmp", text_path)) >=
        sizeof(temp_text_path))
    {
        heap_caps_free(data);
        zip::close(&archive);
        return ESP_ERR_INVALID_SIZE;
    }

    chapter_builder_t builder = {};
    builder.checksum = 2166136261U;
    builder.file = fopen(temp_text_path, "wb");
    if (builder.file == nullptr)
    {
        heap_caps_free(data);
        zip::close(&archive);
        return ESP_FAIL;
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
                append_break(&builder, &last_space, true);
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
            append_break(&builder, &last_space,
                         xml::name_is(&token, "h1") || xml::name_is(&token, "h2"));
        if ((token.type == xml::token_start || token.type == xml::token_empty) &&
            xml::name_is(&token, "li") && builder.length + 2U < chapter_text_length_limit)
        {
            builder_put_bytes(&builder, "- ", 2);
            last_space = false;
        }

        if ((token.type == xml::token_start || token.type == xml::token_empty) &&
            xml::name_is(&token, "img") && builder.image_count < document_image_count)
        {
            char src[book_text_length] = {};
            char image_href[book_text_length] = {};
            if (attribute(&token, "src", src, sizeof(src)) &&
                join_path(document_directory, src, image_href))
            {
                zip::entry_t image_entry = {};
                if (zip::find(&archive, image_href, &image_entry) == ESP_OK &&
                    image_entry.uncompressed_size > 0U &&
                    image_entry.uncompressed_size <= 2U * 1024U * 1024U)
                {
                    uint8_t* image_data =
                        static_cast<uint8_t*>(alloc_prefer_spiram(image_entry.uncompressed_size));
                    if (image_data != nullptr)
                    {
                        size_t image_size = 0;
                        image::info_t info = {};
                        if (zip::read(&archive, &image_entry, image_data,
                                      image_entry.uncompressed_size, &image_size) == ESP_OK &&
                            image::inspect(image_data, image_size, &info) == ESP_OK)
                        {
                            append_break(&builder, &last_space);
                            document_image_t& image_ref = builder.images[builder.image_count++];
                            image_ref.text_offset = builder.length;
                            copy_field(image_ref.href, image_href, sizeof(image_ref.href));
                            image_ref.width = info.width;
                            image_ref.height = info.height;
                            char marker[4] = {};
                            const size_t marker_bytes = encode_utf8(0xfffcU, marker);
                            if (builder.length + marker_bytes < chapter_text_length_limit)
                                builder_put_bytes(&builder, marker, marker_bytes);
                            append_break(&builder, &last_space);
                        }
                        heap_caps_free(image_data);
                    }
                }
            }
        }
        else if (token.type == xml::token_text)
        {
            append_text(&builder, token.value, token.value_length, &last_space, preformatted);
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
    builder_flush(&builder);
    const int close_result = fclose(builder.file);
    builder.file = nullptr;
    const bool write_ok = !builder.error && close_result == 0;
    heap_caps_free(data);
    zip::close(&archive);

    if (!write_ok)
    {
        unlink(temp_text_path);
        return ESP_FAIL;
    }
    unlink(text_path);
    if (rename(temp_text_path, text_path) != 0)
    {
        unlink(temp_text_path);
        return ESP_FAIL;
    }

    chapter_cache_header_t new_header = {};
    new_header.magic = chapter_cache_magic;
    new_header.version = chapter_cache_version;
    new_header.image_count = builder.image_count;
    new_header.source_size = static_cast<uint32_t>(source_info.st_size);
    new_header.source_mtime = static_cast<int64_t>(source_info.st_mtime);
    new_header.text_length = static_cast<uint32_t>(builder.length);
    new_header.checksum = builder.checksum;
    memcpy(new_header.images, builder.images, sizeof(new_header.images));
    char temp_header_path[document_cache_path_length] = {};
    if (static_cast<size_t>(snprintf(temp_header_path, sizeof(temp_header_path), "%s.tmp",
                                     header_path)) < sizeof(temp_header_path))
    {
        FILE* header_file = fopen(temp_header_path, "wb");
        if (header_file != nullptr)
        {
            const bool header_fwrite_ok =
                fwrite(&new_header, 1, sizeof(new_header), header_file) == sizeof(new_header);
            const bool header_write_ok = header_fwrite_ok && fclose(header_file) == 0;
            if (header_write_ok)
            {
                unlink(header_path);
                rename(temp_header_path, header_path);
            }
            else
            {
                unlink(temp_header_path);
            }
        }
    }
    // A failure writing the header is not fatal -- the next open just treats
    // this chapter as an uncached miss and re-parses it.

    document->length = builder.length;
    document->image_count = builder.image_count;
    memcpy(document->images, builder.images, sizeof(document->images));
    return ESP_OK;
}

} // namespace epub
} // namespace xreader
