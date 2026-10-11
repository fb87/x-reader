#include "epub.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

enum { DEFAULT_METADATA_BYTES = 64U * 1024U, DEFAULT_ITEMS = 1024U, DEFAULT_TEXT_BYTES = 256U };

struct manifest_item {
    char* id;
    char* href;
    char* media_type;
};

struct spine_ref {
    char* idref;
};

struct epub_book {
    struct zip_archive* archive;
    char* opf_path;
    char* title;
    char* creator;
    struct manifest_item* manifest;
    uint32_t manifest_count;
    struct spine_ref* spine;
    uint32_t spine_count;
};

static char* copy_text(const char* text, size_t length) {
    char* result = malloc(length + 1U);
    if (result == NULL)
        return NULL;
    memcpy(result, text, length);
    result[length] = '\0';
    return result;
}

static bool name_is(const char* name, size_t length, const char* expected) {
    const char* colon = memchr(name, ':', length);
    if (colon != NULL) {
        const size_t prefix = (size_t)(colon - name) + 1U;
        name = colon + 1;
        length -= prefix;
    }
    return strlen(expected) == length && strncmp(name, expected, length) == 0;
}

static const char* attribute(const char* start, const char* end, const char* wanted,
                             size_t* length) {
    const char* cursor = start;
    while (cursor < end && isspace((unsigned char)*cursor) == 0 && *cursor != '/')
        cursor++;
    while (cursor < end) {
        while (cursor < end && isspace((unsigned char)*cursor) != 0)
            cursor++;
        const char* name = cursor;
        while (cursor < end && isspace((unsigned char)*cursor) == 0 && *cursor != '=' &&
               *cursor != '>')
            cursor++;
        const size_t name_length = (size_t)(cursor - name);
        while (cursor < end && isspace((unsigned char)*cursor) != 0)
            cursor++;
        if (cursor >= end || *cursor != '=')
            break;
        cursor++;
        while (cursor < end && isspace((unsigned char)*cursor) != 0)
            cursor++;
        if (cursor >= end || (*cursor != '\'' && *cursor != '"'))
            break;
        const char quote = *cursor++;
        const char* value = cursor;
        while (cursor < end && *cursor != quote)
            cursor++;
        if (name_is(name, name_length, wanted)) {
            *length = (size_t)(cursor - value);
            return value;
        }
        if (cursor < end)
            cursor++;
    }
    return NULL;
}

static const char* local_name(const char* start, size_t length, size_t* local_length) {
    const char* colon = memchr(start, ':', length);
    if (colon == NULL) {
        *local_length = length;
        return start;
    }
    *local_length = length - (size_t)(colon + 1 - start);
    return colon + 1;
}

static bool find_tag(const char* data, size_t size, const char* wanted, const char** cursor,
                     const char** tag_start, const char** tag_end, bool* closing) {
    const char* position = *cursor;
    while ((size_t)(position - data) < size) {
        const char* open = memchr(position, '<', size - (size_t)(position - data));
        if (open == NULL)
            return false;
        if (strncmp(open, "<!--", 4) == 0) {
            const char* end_comment = strstr(open + 4, "-->");
            if (end_comment == NULL)
                return false;
            position = end_comment + 3;
            continue;
        }
        const char* end = strchr(open, '>');
        if (end == NULL || (size_t)(end - data) >= size)
            return false;
        const char* name = open + 1;
        bool is_closing = false;
        if (*name == '/') {
            is_closing = true;
            name++;
        }
        while (name < end && isspace((unsigned char)*name) != 0)
            name++;
        const char* name_end = name;
        while (name_end < end && isspace((unsigned char)*name_end) == 0 && *name_end != '/' &&
               *name_end != '>')
            name_end++;
        size_t local_length = 0U;
        const char* local = local_name(name, (size_t)(name_end - name), &local_length);
        if (name_is(local, local_length, wanted)) {
            *cursor = end + 1;
            *tag_start = open;
            *tag_end = end;
            *closing = is_closing;
            return true;
        }
        position = end + 1;
    }
    return false;
}

static char* resolve_path(const char* base, const char* href) {
    char buffer[1024];
    const char* slash = strrchr(base, '/');
    const size_t prefix = slash == NULL ? 0U : (size_t)(slash - base + 1);
    if (prefix + strlen(href) >= sizeof(buffer))
        return NULL;
    memcpy(buffer, base, prefix);
    strcpy(&buffer[prefix], href);
    char* parts[128];
    size_t count = 0U;
    char* token = strtok(buffer, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
        } else if (strcmp(token, "..") == 0) {
            if (count == 0U)
                return NULL;
            count--;
        } else if (count < sizeof(parts) / sizeof(parts[0])) {
            parts[count++] = token;
        } else {
            return NULL;
        }
        token = strtok(NULL, "/");
    }
    size_t length = 0U;
    for (size_t index = 0U; index < count; ++index)
        length += strlen(parts[index]) + (index != 0U);
    char* result = malloc(length + 1U);
    if (result == NULL)
        return NULL;
    result[0] = '\0';
    for (size_t index = 0U; index < count; ++index) {
        if (index != 0U)
            strcat(result, "/");
        strcat(result, parts[index]);
    }
    return result;
}

static enum epub_status parse_container(struct zip_archive* archive, char** opf_path) {
    uint32_t index = 0U;
    uint8_t* data = NULL;
    size_t length = 0U;
    if (!zip_entry_find(archive, "META-INF/container.xml", &index) ||
        zip_entry_read_all(archive, index, &data, &length) != ZIP_OK)
        return EPUB_MALFORMED;
    const char* cursor = (const char*)data;
    const char* start = NULL;
    const char* end = NULL;
    bool closing = false;
    if (!find_tag((const char*)data, length, "rootfile", &cursor, &start, &end, &closing) ||
        closing) {
        zip_data_free(data);
        return EPUB_MALFORMED;
    }
    size_t value_length = 0U;
    const char* value = attribute(start + 1, end, "full-path", &value_length);
    *opf_path = value == NULL ? NULL : copy_text(value, value_length);
    zip_data_free(data);
    return *opf_path == NULL ? EPUB_MALFORMED : EPUB_OK;
}

static char* first_text(const uint8_t* data, size_t length, const char* wanted,
                        uint32_t max_bytes) {
    const char* cursor = (const char*)data;
    const char* start = NULL;
    const char* end = NULL;
    bool closing = false;
    if (!find_tag((const char*)data, length, wanted, &cursor, &start, &end, &closing) || closing)
        return NULL;
    const char* text = end + 1;
    const char* text_end = strchr(text, '<');
    if (text_end == NULL)
        return NULL;
    while (text < text_end && isspace((unsigned char)*text) != 0)
        text++;
    while (text_end > text && isspace((unsigned char)text_end[-1]) != 0)
        text_end--;
    const size_t text_length = (size_t)(text_end - text);
    return text_length > max_bytes ? NULL : copy_text(text, text_length);
}

static enum epub_status parse_opf(struct epub_book* book, const struct epub_limits* limits,
                                  const uint8_t* data, size_t length) {
    book->title = first_text(data, length, "title", limits->max_text_bytes);
    book->creator = first_text(data, length, "creator", limits->max_text_bytes);
    const char* cursor = (const char*)data;
    const char* start = NULL;
    const char* end = NULL;
    bool closing = false;
    unsigned capture = 0U;
    char* captured = NULL;
    while (find_tag((const char*)data, length, "metadata", &cursor, &start, &end, &closing) &&
           !closing) {
        const char* metadata_end = cursor;
        size_t local_length = 0U;
        const char* local = NULL;
        while (
            find_tag((const char*)data, length, "title", &metadata_end, &start, &end, &closing)) {
            if (closing)
                continue;
            local = local_name(start + 1, (size_t)(end - start - 1), &local_length);
            const char* text = end + 1;
            const char* text_end = strchr(text, '<');
            if (text_end == NULL)
                break;
            captured = copy_text(text, (size_t)(text_end - text));
            if (captured != NULL) {
                free(book->title);
                book->title = captured;
            }
            break;
        }
        (void)local;
        (void)capture;
        break;
    }
    cursor = (const char*)data;
    while (find_tag((const char*)data, length, "item", &cursor, &start, &end, &closing)) {
        if (closing)
            continue;
        if (book->manifest_count >= limits->max_manifest_items)
            return EPUB_LIMIT;
        size_t id_length = 0U;
        size_t href_length = 0U;
        size_t type_length = 0U;
        const char* id = attribute(start + 1, end, "id", &id_length);
        const char* href = attribute(start + 1, end, "href", &href_length);
        const char* type = attribute(start + 1, end, "media-type", &type_length);
        if (id == NULL || href == NULL || type == NULL)
            continue;
        struct manifest_item* item = &book->manifest[book->manifest_count++];
        item->id = copy_text(id, id_length);
        char* href_copy = copy_text(href, href_length);
        item->href = href_copy == NULL ? NULL : resolve_path(book->opf_path, href_copy);
        free(href_copy);
        item->media_type = copy_text(type, type_length);
        if (item->id == NULL || item->href == NULL || item->media_type == NULL) {
            return EPUB_OUT_OF_MEMORY;
        }
    }
    cursor = (const char*)data;
    while (find_tag((const char*)data, length, "itemref", &cursor, &start, &end, &closing)) {
        if (closing)
            continue;
        if (book->spine_count >= limits->max_spine_items)
            return EPUB_LIMIT;
        size_t idref_length = 0U;
        const char* idref = attribute(start + 1, end, "idref", &idref_length);
        if (idref == NULL)
            continue;
        book->spine[book->spine_count].idref = copy_text(idref, idref_length);
        if (book->spine[book->spine_count++].idref == NULL)
            return EPUB_OUT_OF_MEMORY;
    }
    return EPUB_OK;
}

enum epub_status epub_open(struct zip_archive* archive, const struct epub_limits* input_limits,
                           struct epub_book** result) {
    const struct epub_limits defaults = {64U * 1024U, 1024U, 1024U, 256U};
    const struct epub_limits* limits = input_limits == NULL ? &defaults : input_limits;
    if (archive == NULL || result == NULL || limits->max_metadata_bytes == 0U)
        return EPUB_INVALID_ARGUMENT;
    uint32_t index = 0U;
    uint8_t* data = NULL;
    size_t length = 0U;
    if (!zip_entry_find(archive, "mimetype", &index) ||
        zip_entry_read_all(archive, index, &data, &length) != ZIP_OK ||
        length != strlen("application/epub+zip") ||
        memcmp(data, "application/epub+zip", length) != 0) {
        zip_data_free(data);
        return EPUB_MALFORMED;
    }
    zip_data_free(data);
    struct epub_book* book = calloc(1U, sizeof(*book));
    if (book == NULL)
        return EPUB_OUT_OF_MEMORY;
    book->archive = archive;
    book->manifest = calloc(limits->max_manifest_items, sizeof(*book->manifest));
    book->spine = calloc(limits->max_spine_items, sizeof(*book->spine));
    if (book->manifest == NULL || book->spine == NULL) {
        epub_close(book);
        return EPUB_OUT_OF_MEMORY;
    }
    enum epub_status status = parse_container(archive, &book->opf_path);
    if (status != EPUB_OK) {
        epub_close(book);
        return status;
    }
    if (!zip_entry_find(archive, book->opf_path, &index) ||
        zip_entry_read_all(archive, index, &data, &length) != ZIP_OK ||
        length > limits->max_metadata_bytes) {
        zip_data_free(data);
        epub_close(book);
        return EPUB_LIMIT;
    }
    status = parse_opf(book, limits, data, length);
    zip_data_free(data);
    if (status != EPUB_OK) {
        epub_close(book);
        return status;
    }
    *result = book;
    return EPUB_OK;
}

void epub_close(struct epub_book* book) {
    if (book == NULL)
        return;
    free(book->opf_path);
    free(book->title);
    free(book->creator);
    for (uint32_t index = 0U; index < book->manifest_count; ++index) {
        free(book->manifest[index].id);
        free(book->manifest[index].href);
        free(book->manifest[index].media_type);
    }
    for (uint32_t index = 0U; index < book->spine_count; ++index)
        free(book->spine[index].idref);
    free(book->manifest);
    free(book->spine);
    free(book);
}

const char* epub_title(const struct epub_book* book) { return book == NULL ? NULL : book->title; }
const char* epub_creator(const struct epub_book* book) {
    return book == NULL ? NULL : book->creator;
}
const char* epub_opf_path(const struct epub_book* book) {
    return book == NULL ? NULL : book->opf_path;
}
uint32_t epub_spine_count(const struct epub_book* book) {
    return book == NULL ? 0U : book->spine_count;
}

enum epub_status epub_spine_get(const struct epub_book* book, uint32_t index,
                                struct epub_spine_item* item) {
    if (book == NULL || item == NULL)
        return EPUB_INVALID_ARGUMENT;
    if (index >= book->spine_count)
        return EPUB_MALFORMED;
    for (uint32_t manifest = 0U; manifest < book->manifest_count; ++manifest) {
        if (strcmp(book->manifest[manifest].id, book->spine[index].idref) == 0) {
            *item =
                (struct epub_spine_item){book->manifest[manifest].id, book->manifest[manifest].href,
                                         book->manifest[manifest].media_type};
            return EPUB_OK;
        }
    }
    return EPUB_MALFORMED;
}

enum epub_status epub_spine_text(const struct epub_book* book, uint32_t index, uint8_t** text,
                                 size_t* length) {
    if (book == NULL || text == NULL || length == NULL)
        return EPUB_INVALID_ARGUMENT;
    struct epub_spine_item item;
    if (epub_spine_get(book, index, &item) != EPUB_OK)
        return EPUB_MALFORMED;
    uint32_t entry_index = 0U;
    if (!zip_entry_find(book->archive, item.href, &entry_index))
        return EPUB_MALFORMED;
    uint8_t* source = NULL;
    size_t source_length = 0U;
    if (zip_entry_read_all(book->archive, entry_index, &source, &source_length) != ZIP_OK) {
        return EPUB_LIMIT;
    }
    uint8_t* output = malloc(source_length + 1U);
    if (output == NULL) {
        zip_data_free(source);
        return EPUB_OUT_OF_MEMORY;
    }
    size_t output_length = 0U;
    bool in_tag = false;
    bool skip_content = false;
    for (size_t position = 0U; position < source_length; ++position) {
        const uint8_t character = source[position];
        if (character == '<') {
            in_tag = true;
            if (position + 6U < source_length &&
                (strncmp((const char*)&source[position + 1U], "style", 5U) == 0 ||
                 strncmp((const char*)&source[position + 1U], "script", 6U) == 0))
                skip_content = true;
            if (position + 2U < source_length && source[position + 1U] == '/') {
                if (strncmp((const char*)&source[position + 2U], "style", 5U) == 0 ||
                    strncmp((const char*)&source[position + 2U], "script", 6U) == 0)
                    skip_content = false;
                if (source[position + 2U] == 'p' || source[position + 2U] == 'h' ||
                    source[position + 2U] == 'l') {
                    while (output_length > 0U && output[output_length - 1U] == ' ')
                        output_length--;
                    if (output_length > 0U && output[output_length - 1U] != '\n')
                        output[output_length++] = '\n';
                    if (output_length > 1U && output[output_length - 2U] != '\n')
                        output[output_length++] = '\n';
                }
            }
        } else if (character == '>') {
            in_tag = false;
        } else if (!in_tag && !skip_content && output_length < source_length) {
            if (isspace(character) != 0) {
                if (output_length > 0U && output[output_length - 1U] != ' ' &&
                    output[output_length - 1U] != '\n')
                    output[output_length++] = ' ';
            } else {
                output[output_length++] = character;
            }
        }
    }
    size_t compact_length = 0U;
    for (size_t position = 0U; position < output_length; ++position) {
        const uint8_t character = output[position];
        if (character == '\n') {
            while (compact_length > 0U && isspace(output[compact_length - 1U]) != 0 &&
                   output[compact_length - 1U] != '\n')
                compact_length--;
            if (compact_length > 0U &&
                (output[compact_length - 1U] != '\n' ||
                 (compact_length > 1U && output[compact_length - 2U] != '\n')))
                output[compact_length++] = character;
        } else if (compact_length == 0U || output[compact_length - 1U] != '\n' ||
                   isspace(character) == 0) {
            output[compact_length++] = character;
        }
    }
    output_length = compact_length;
    while (output_length > 0U &&
           (output[output_length - 1U] == '\n' || output[output_length - 1U] == ' '))
        output_length--;
    output[output_length] = '\0';
    zip_data_free(source);
    *text = output;
    *length = output_length;
    return EPUB_OK;
}

void epub_text_free(uint8_t* text) { free(text); }
