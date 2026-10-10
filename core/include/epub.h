#ifndef READER_EPUB_H
#define READER_EPUB_H

#include "zip.h"

#include <stdint.h>

enum epub_status {
    EPUB_OK = 0,
    EPUB_INVALID_ARGUMENT,
    EPUB_MALFORMED,
    EPUB_LIMIT,
    EPUB_UNSUPPORTED,
    EPUB_OUT_OF_MEMORY,
};

struct epub_limits {
    uint32_t max_metadata_bytes;
    uint32_t max_manifest_items;
    uint32_t max_spine_items;
    uint32_t max_text_bytes;
};

struct epub_book;

struct epub_spine_item {
    const char* id;
    const char* href;
    const char* media_type;
};

enum epub_status epub_open(struct zip_archive* archive, const struct epub_limits* limits,
                           struct epub_book** book);
void epub_close(struct epub_book* book);
const char* epub_title(const struct epub_book* book);
const char* epub_creator(const struct epub_book* book);
const char* epub_opf_path(const struct epub_book* book);
uint32_t epub_spine_count(const struct epub_book* book);
enum epub_status epub_spine_get(const struct epub_book* book, uint32_t index,
                                struct epub_spine_item* item);
enum epub_status epub_spine_text(const struct epub_book* book, uint32_t index, uint8_t** text,
                                 size_t* length);
void epub_text_free(uint8_t* text);

#endif
