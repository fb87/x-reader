#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace epub
{

static constexpr size_t book_text_length = 128;
static constexpr size_t book_spine_length = 32;
static constexpr size_t book_toc_length = 32;
static constexpr size_t document_image_count = 8;
// Cache file path: dirname(book path, up to 512) + "/.xreader-chapters/" (19)
// + an 8-hex-digit book hash + "/ch###.txt" (10), rounded up with headroom.
static constexpr size_t document_cache_path_length = 576;
// Sanity ceiling on a single cached chapter's decoded text, enforced while
// streaming it to disk. Unlike the old fixed in-RAM buffer this replaced,
// this is not a RAM cost -- it only bounds how large one chapter's SD cache
// file (and therefore one book) is allowed to get.
static constexpr size_t chapter_text_length_limit = 4U * 1024U * 1024U;

struct spine_item_t
{
    char id[book_text_length];
    char href[book_text_length];
    char title[book_text_length];
};

struct toc_item_t
{
    char title[book_text_length];
    char href[book_text_length];
    uint8_t spine_index;
};

struct book_t
{
    char title[book_text_length];
    char author[book_text_length];
    char opf_path[book_text_length];
    char first_document[book_text_length];
    char opf_directory[book_text_length];
    char cover_href[book_text_length];
    uint8_t spine_count;
    uint8_t toc_count;
    spine_item_t spine[book_spine_length];
    toc_item_t toc[book_toc_length];
};

struct document_image_t
{
    size_t text_offset;
    char href[book_text_length];
    uint16_t width;
    uint16_t height;
};

// A loaded chapter's decoded plain text lives in an SD-card cache file (see
// epub/book.cpp and epub/chapter_stream.hpp), not in RAM -- this struct is
// just a handle to it, plus the small always-resident image index. This is
// what lets a chapter of any size be "open" without a per-chapter RAM cap.
struct document_t
{
    char cache_path[document_cache_path_length];
    size_t length;
    uint8_t image_count;
    document_image_t images[document_image_count];
};

esp_err_t load_metadata(const char* path, book_t* book);
esp_err_t load_document(const char* path, const book_t* book, uint8_t spine_index,
                        document_t* document);
esp_err_t load_resource(const char* path, const char* archive_href, uint8_t** data, size_t* size);

} // namespace epub
} // namespace xreader
