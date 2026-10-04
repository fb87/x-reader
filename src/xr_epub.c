#include "xr/xr_epub.h"

#include <stdbool.h>
#include <string.h>

#define ZIP_LOCAL_SIGNATURE 0x04034b50UL
#define ZIP_CENTRAL_SIGNATURE 0x02014b50UL
#define ZIP_END_SIGNATURE 0x06054b50UL
#define ZIP_CENTRAL_HEADER_SIZE 46u

static uint16_t le16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool range_ok(uint32_t start, uint32_t length, uint32_t total)
{
    return start <= total && length <= total - start;
}

static xr_epub_status_t read_at(const xr_storage_t *storage, uint32_t offset,
                                void *destination, uint32_t size)
{
    if (!storage || !storage->read || !range_ok(offset, size, storage->size))
        return XR_EPUB_ERROR_IO;
    return storage->read(storage->context, offset, destination, size) ?
        XR_EPUB_OK : XR_EPUB_ERROR_IO;
}

static xr_epub_status_t read_u32(const xr_storage_t *storage, uint32_t offset,
                                 uint32_t *value)
{
    unsigned char bytes[4];
    xr_epub_status_t status = read_at(storage, offset, bytes, sizeof(bytes));
    if (status == XR_EPUB_OK) *value = le32(bytes);
    return status;
}

static bool string_equal_at(const xr_storage_t *storage, uint32_t offset,
                            uint16_t length, const char *string)
{
    uint16_t i;
    for (i = 0; i < length; ++i) {
        unsigned char c;
        if (!string[i] || read_at(storage, offset + i, &c, 1) != XR_EPUB_OK ||
            c != (unsigned char)string[i]) return false;
    }
    return string[length] == '\0';
}

/* Returns a central-directory entry's 46-byte header and its offset. */
static xr_epub_status_t zip_find(const xr_storage_t *storage, const char *path,
                                 unsigned char header[46], uint32_t *header_at)
{
    uint32_t end_at, start, pos, signature, directory_at, directory_size, directory_end;
    unsigned char end[22];
    uint16_t entries, i;

    if (!storage || !path || storage->size < sizeof(end)) return XR_EPUB_ERROR_FORMAT;
    start = storage->size > 65557u ? storage->size - 65557u : 0;
    end_at = storage->size - sizeof(end);
    for (pos = end_at;; --pos) {
        if (read_u32(storage, pos, &signature) != XR_EPUB_OK) return XR_EPUB_ERROR_IO;
        if (signature == ZIP_END_SIGNATURE) break;
        if (pos == start) return XR_EPUB_ERROR_FORMAT;
    }
    if (read_at(storage, pos, end, sizeof(end)) != XR_EPUB_OK) return XR_EPUB_ERROR_IO;
    if (le16(end + 4) != 0 || le16(end + 6) != 0 || le16(end + 8) != le16(end + 10) ||
        le16(end + 8) == UINT16_MAX || le32(end + 12) == UINT32_MAX ||
        le32(end + 16) == UINT32_MAX) return XR_EPUB_ERROR_UNSUPPORTED;
    entries = le16(end + 10);
    directory_size = le32(end + 12);
    directory_at = le32(end + 16);
    if (!range_ok(directory_at, directory_size, storage->size)) return XR_EPUB_ERROR_FORMAT;
    directory_end = directory_at + directory_size;
    pos = directory_at;
    for (i = 0; i < entries; ++i) {
        uint16_t name_size, extra_size, comment_size;
        uint32_t next;
        if (!range_ok(pos, ZIP_CENTRAL_HEADER_SIZE, directory_end) ||
            read_at(storage, pos, header, ZIP_CENTRAL_HEADER_SIZE) != XR_EPUB_OK)
            return XR_EPUB_ERROR_FORMAT;
        if (le32(header) != ZIP_CENTRAL_SIGNATURE) return XR_EPUB_ERROR_FORMAT;
        name_size = le16(header + 28);
        extra_size = le16(header + 30);
        comment_size = le16(header + 32);
        next = pos + ZIP_CENTRAL_HEADER_SIZE;
        if (!range_ok(next, (uint32_t)name_size + extra_size + comment_size, directory_end))
            return XR_EPUB_ERROR_FORMAT;
        if (string_equal_at(storage, next, name_size, path)) {
            *header_at = pos;
            return XR_EPUB_OK;
        }
        pos = next + name_size + extra_size + comment_size;
    }
    return XR_EPUB_ERROR_NOT_FOUND;
}

xr_epub_status_t xr_epub_extract(const xr_epub_t *epub, const char *path,
                                 void *destination, uint32_t destination_size,
                                 uint32_t *extracted_size)
{
    unsigned char central[46], local[30];
    uint32_t central_at, local_at, data_at, compressed, uncompressed;
    uint16_t flags, method, name_size, extra_size;
    xr_epub_status_t status;

    if (!epub || !epub->storage || !path || !destination || !extracted_size)
        return XR_EPUB_ERROR_ARGUMENT;
    status = zip_find(epub->storage, path, central, &central_at);
    (void)central_at;
    if (status != XR_EPUB_OK) return status;
    flags = le16(central + 8);
    method = le16(central + 10);
    compressed = le32(central + 20);
    uncompressed = le32(central + 24);
    local_at = le32(central + 42);
    if (flags & 1u) return XR_EPUB_ERROR_UNSUPPORTED;
    if (uncompressed > destination_size) return XR_EPUB_ERROR_BUFFER_TOO_SMALL;
    if (!range_ok(local_at, sizeof(local), epub->storage->size) ||
        read_at(epub->storage, local_at, local, sizeof(local)) != XR_EPUB_OK ||
        le32(local) != ZIP_LOCAL_SIGNATURE) return XR_EPUB_ERROR_FORMAT;
    name_size = le16(local + 26);
    extra_size = le16(local + 28);
    data_at = local_at + 30u;
    if (data_at < local_at || !range_ok(data_at, (uint32_t)name_size + extra_size,
                                        epub->storage->size)) return XR_EPUB_ERROR_FORMAT;
    data_at += name_size + extra_size;
    if (!range_ok(data_at, compressed, epub->storage->size)) return XR_EPUB_ERROR_FORMAT;
    if (method == 0) {
        if (compressed != uncompressed) return XR_EPUB_ERROR_FORMAT;
        status = read_at(epub->storage, data_at, destination, uncompressed);
    } else if (method == 8 && epub->storage->inflate_raw) {
        status = epub->storage->inflate_raw(epub->storage->context, epub->storage,
                                            data_at, compressed, destination, uncompressed) ?
            XR_EPUB_OK : XR_EPUB_ERROR_IO;
    } else {
        return XR_EPUB_ERROR_UNSUPPORTED;
    }
    if (status == XR_EPUB_OK) *extracted_size = uncompressed;
    return status;
}

static bool name_is(const char *start, const char *end, const char *name)
{
    const char *local = start;
    const char *p;
    for (p = start; p < end; ++p) if (*p == ':') local = p + 1;
    while (local < end && *name && *local == *name) { ++local; ++name; }
    return local == end && *name == '\0';
}

static const char *tag_next(const char *p, const char *limit, const char **name,
                            const char **name_end, const char **tag_end)
{
    while (p < limit) {
        while (p < limit && *p != '<') ++p;
        if (p == limit || ++p == limit) return NULL;
        if (*p == '/' || *p == '!' || *p == '?') {
            while (p < limit && *p != '>') ++p;
            if (p == limit) return NULL;
            ++p;
            continue; /* never return stale name pointers for closing tags */
        }
        *name = p;
        while (p < limit && *p != '>' && *p != '/' && *p != ' ' && *p != '\t' &&
               *p != '\r' && *p != '\n') ++p;
        *name_end = p;
        while (p < limit && *p != '>') ++p;
        if (p == limit) return NULL;
        *tag_end = p;
        return p + 1;
    }
    return NULL;
}

static bool copy_value(char *destination, uint32_t size, const char *value,
                       const char *end)
{
    uint32_t n = (uint32_t)(end - value);
    if (n >= size) return false;
    memcpy(destination, value, n);
    destination[n] = '\0';
    return true;
}

static bool attribute(const char *p, const char *end, const char *wanted,
                      char *destination, uint32_t destination_size)
{
    while (p < end) {
        const char *key, *key_end, *value;
        char quote;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '/')) ++p;
        key = p;
        while (p < end && *p != '=' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
        key_end = p;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
        if (p == end || *p != '=') { while (p < end && *p != ' ') ++p; continue; }
        ++p;
        while (p < end && (*p == ' ' || *p == '\t')) ++p;
        if (p == end || (*p != '\'' && *p != '\"')) return false;
        quote = *p++;
        value = p;
        while (p < end && *p != quote) ++p;
        if (p == end) return false;
        if (name_is(key, key_end, wanted)) return copy_value(destination, destination_size, value, p);
        ++p;
    }
    return false;
}

static xr_epub_status_t parse_container(xr_epub_t *epub, const char *xml, uint32_t size)
{
    const char *p = xml, *limit = xml + size, *name, *name_end, *tag_end;
    while ((p = tag_next(p, limit, &name, &name_end, &tag_end)) != NULL) {
        if (name_is(name, name_end, "rootfile")) {
            if (!attribute(name_end, tag_end, "full-path", epub->package_path,
                           sizeof(epub->package_path))) return XR_EPUB_ERROR_FORMAT;
            return XR_EPUB_OK;
        }
    }
    return XR_EPUB_ERROR_FORMAT;
}

static uint16_t manifest_index(const xr_epub_t *epub, const char *id)
{
    uint16_t i;
    for (i = 0; i < epub->manifest_count; ++i)
        if (strcmp(epub->manifest[i].id, id) == 0) return i;
    return XR_EPUB_NO_ITEM;
}

static xr_epub_status_t parse_opf(xr_epub_t *epub, const char *xml, uint32_t size)
{
    const char *p = xml, *limit = xml + size, *name, *name_end, *tag_end;
    char id[XR_EPUB_ID_MAX];
    while ((p = tag_next(p, limit, &name, &name_end, &tag_end)) != NULL) {
        if (name_is(name, name_end, "item")) {
            xr_epub_manifest_item_t *item;
            if (epub->manifest_count == epub->manifest_capacity) return XR_EPUB_ERROR_CAPACITY;
            item = &epub->manifest[epub->manifest_count];
            if (!attribute(name_end, tag_end, "id", item->id, sizeof(item->id)) ||
                !attribute(name_end, tag_end, "href", item->href, sizeof(item->href)))
                return XR_EPUB_ERROR_FORMAT;
            ++epub->manifest_count;
        } else if (name_is(name, name_end, "itemref")) {
            uint16_t index;
            if (!attribute(name_end, tag_end, "idref", id, sizeof(id))) return XR_EPUB_ERROR_FORMAT;
            index = manifest_index(epub, id);
            if (index == XR_EPUB_NO_ITEM) return XR_EPUB_ERROR_FORMAT;
            if (epub->spine_count == epub->spine_capacity) return XR_EPUB_ERROR_CAPACITY;
            epub->spine[epub->spine_count++].manifest_index = index;
        }
    }
    if (!epub->title[0]) {
        const char *start = strstr(xml, "<dc:title");
        if (start) {
            start = strchr(start, '>');
            if (start) {
                const char *end = strstr(++start, "</dc:title>");
                if (end) copy_value(epub->title, sizeof(epub->title), start, end);
            }
        }
    }
    return epub->spine_count ? XR_EPUB_OK : XR_EPUB_ERROR_FORMAT;
}

xr_epub_status_t xr_epub_open(xr_epub_t *epub, const xr_storage_t *storage,
                              void *scratch, uint32_t scratch_size,
                              xr_epub_manifest_item_t *manifest,
                              uint16_t manifest_capacity,
                              xr_epub_spine_item_t *spine,
                              uint16_t spine_capacity)
{
    uint32_t size;
    xr_epub_status_t status;
    if (!epub || !storage || !storage->read || !scratch || scratch_size < 2 ||
        !manifest || !manifest_capacity || !spine || !spine_capacity)
        return XR_EPUB_ERROR_ARGUMENT;
    memset(epub, 0, sizeof(*epub));
    epub->storage = storage;
    epub->scratch = scratch;
    epub->scratch_size = scratch_size;
    epub->manifest = manifest;
    epub->manifest_capacity = manifest_capacity;
    epub->spine = spine;
    epub->spine_capacity = spine_capacity;
    status = xr_epub_extract(epub, "META-INF/container.xml", scratch, scratch_size - 1, &size);
    if (status != XR_EPUB_OK) return status;
    ((char *)scratch)[size] = '\0';
    status = parse_container(epub, scratch, size);
    if (status != XR_EPUB_OK) return status;
    status = xr_epub_extract(epub, epub->package_path, scratch, scratch_size - 1, &size);
    if (status != XR_EPUB_OK) return status;
    ((char *)scratch)[size] = '\0';
    return parse_opf(epub, scratch, size);
}

xr_epub_status_t xr_epub_spine_path(const xr_epub_t *epub, uint16_t spine_index,
                                    char *path, uint32_t path_size)
{
    const char *slash;
    const char *href;
    uint32_t prefix, href_size, read, written, segment_start, segment_size;
    uint16_t item;
    if (!epub || !path || spine_index >= epub->spine_count) return XR_EPUB_ERROR_ARGUMENT;
    item = epub->spine[spine_index].manifest_index;
    if (item >= epub->manifest_count) return XR_EPUB_ERROR_FORMAT;
    href = epub->manifest[item].href;
    if (href[0] == '/') { ++href; prefix = 0; }
    else {
        slash = strrchr(epub->package_path, '/');
        prefix = slash ? (uint32_t)(slash - epub->package_path + 1) : 0;
    }
    href_size = 0;
    while (href[href_size] && href[href_size] != '#') ++href_size;
    if (prefix > path_size || href_size >= path_size - prefix) return XR_EPUB_ERROR_BUFFER_TOO_SMALL;
    memcpy(path, epub->package_path, prefix);
    memcpy(path + prefix, href, href_size);
    path[prefix + href_size] = '\0';

    /* Normalize EPUB's relative href segments without allocating a second path. */
    read = written = 0;
    while (read < prefix + href_size) {
        while (read < prefix + href_size && path[read] == '/') ++read;
        segment_start = read;
        while (read < prefix + href_size && path[read] != '/') ++read;
        segment_size = read - segment_start;
        if (segment_size == 0 || (segment_size == 1 && path[segment_start] == '.')) continue;
        if (segment_size == 2 && path[segment_start] == '.' && path[segment_start + 1] == '.') {
            if (!written) return XR_EPUB_ERROR_FORMAT;
            while (written && path[written - 1] != '/') --written;
            if (written) --written;
            continue;
        }
        if (written) path[written++] = '/';
        memmove(path + written, path + segment_start, segment_size);
        written += segment_size;
    }
    path[written] = '\0';
    return XR_EPUB_OK;
}

static bool append_char(char *text, uint32_t size, uint32_t *used, char c)
{
    if (*used + 1 >= size) return false;
    text[(*used)++] = c;
    return true;
}

static bool append_entity(const char **source, const char *limit, char *text,
                          uint32_t size, uint32_t *used)
{
    const char *p = *source;
    char c = 0;
    if (limit - p >= 4 && memcmp(p, "amp;", 4) == 0) { c = '&'; p += 4; }
    else if (limit - p >= 3 && memcmp(p, "lt;", 3) == 0) { c = '<'; p += 3; }
    else if (limit - p >= 3 && memcmp(p, "gt;", 3) == 0) { c = '>'; p += 3; }
    else if (limit - p >= 5 && memcmp(p, "quot;", 5) == 0) { c = '\"'; p += 5; }
    else if (limit - p >= 5 && memcmp(p, "apos;", 5) == 0) { c = '\''; p += 5; }
    else return append_char(text, size, used, '&');
    *source = p;
    return append_char(text, size, used, c);
}

static bool text_break_tag(const char *tag, const char *end)
{
    const char *name = tag;
    const char *name_end;
    if (name < end && *name == '/') ++name;
    name_end = name;
    while (name_end < end && *name_end != ' ' && *name_end != '\t' &&
           *name_end != '\r' && *name_end != '\n' && *name_end != '/') ++name_end;
    return name_is(name, name_end, "br") || name_is(name, name_end, "p") ||
        name_is(name, name_end, "div") || name_is(name, name_end, "li") ||
        name_is(name, name_end, "h1") || name_is(name, name_end, "h2") ||
        name_is(name, name_end, "h3") || name_is(name, name_end, "tr");
}

static xr_epub_status_t xhtml_text(const char *source, uint32_t source_size,
                                   char *text, uint32_t text_size, uint32_t *out)
{
    const char *p = source, *limit = source + source_size;
    uint32_t used = 0;
    bool space = true;
    const char *ignored = NULL;
    if (!text || !text_size) return XR_EPUB_ERROR_ARGUMENT;
    while (p < limit) {
        if (ignored && *p != '<') {
            ++p;
            continue;
        }
        if (*p == '<') {
            const char *tag = ++p;
            while (p < limit && *p != '>') ++p;
            if (p == limit) return XR_EPUB_ERROR_FORMAT;
            const char *tag_name = tag;
            if (*tag_name == '/') ++tag_name;
            const char *tag_name_end = tag_name;
            while (tag_name_end < p && *tag_name_end != ' ' && *tag_name_end != '\t' &&
                   *tag_name_end != '\r' && *tag_name_end != '\n' && *tag_name_end != '/') ++tag_name_end;
            bool closing = tag != tag_name;
            if (ignored) {
                if (closing && name_is(tag_name, tag_name_end, ignored)) ignored = NULL;
                ++p;
                continue;
            }
            if (!closing && (name_is(tag_name, tag_name_end, "head") ||
                             name_is(tag_name, tag_name_end, "style") ||
                             name_is(tag_name, tag_name_end, "script"))) {
                ignored = name_is(tag_name, tag_name_end, "head") ? "head" :
                          name_is(tag_name, tag_name_end, "style") ? "style" : "script";
                ++p;
                continue;
            }
            bool break_tag = text_break_tag(tag, p);
            ++p;
            if (break_tag && !space) { if (!append_char(text, text_size, &used, '\n')) return XR_EPUB_ERROR_BUFFER_TOO_SMALL; space = true; }
        } else if (*p == '&') {
            ++p;
            if (!append_entity(&p, limit, text, text_size, &used)) return XR_EPUB_ERROR_BUFFER_TOO_SMALL;
            space = false;
        } else if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            ++p;
            if (!space) { if (!append_char(text, text_size, &used, ' ')) return XR_EPUB_ERROR_BUFFER_TOO_SMALL; space = true; }
        } else {
            if (!append_char(text, text_size, &used, *p++)) return XR_EPUB_ERROR_BUFFER_TOO_SMALL;
            space = false;
        }
    }
    while (used && (text[used - 1] == ' ' || text[used - 1] == '\n')) --used;
    text[used] = '\0';
    if (out) *out = used;
    return XR_EPUB_OK;
}

xr_epub_status_t xr_epub_spine_text(const xr_epub_t *epub, uint16_t spine_index,
                                    void *scratch, uint32_t scratch_size,
                                    char *text, uint32_t text_size,
                                    uint32_t *text_size_out)
{
    char path[XR_EPUB_PATH_MAX];
    uint32_t size;
    xr_epub_status_t status;
    if (!scratch || scratch_size < 2) return XR_EPUB_ERROR_ARGUMENT;
    status = xr_epub_spine_path(epub, spine_index, path, sizeof(path));
    if (status != XR_EPUB_OK) return status;
    status = xr_epub_extract(epub, path, scratch, scratch_size - 1, &size);
    if (status != XR_EPUB_OK) return status;
    ((char *)scratch)[size] = '\0';
    return xhtml_text(scratch, size, text, text_size, text_size_out);
}

const char *xr_epub_status_string(xr_epub_status_t status)
{
    static const char *const names[] = {
        "ok", "invalid argument", "storage I/O error", "invalid EPUB format",
        "unsupported EPUB feature", "archive entry not found", "buffer too small",
        "caller capacity exceeded"
    };
    return (unsigned)status < sizeof(names) / sizeof(names[0]) ? names[status] : "unknown EPUB error";
}
