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
static constexpr size_t document_text_length = 24576;

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
    uint8_t spine_count;
    uint8_t toc_count;
    spine_item_t spine[book_spine_length];
    toc_item_t toc[book_toc_length];
};

struct document_t
{
    char text[document_text_length];
    size_t length;
};

esp_err_t load_metadata(const char* path, book_t* book);
esp_err_t load_document(const char* path, const book_t* book, uint8_t spine_index,
                        document_t* document);

} // namespace epub
} // namespace xreader
