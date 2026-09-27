#pragma once

#include "esp_err.h"

#include "epub/book.hpp"

namespace xreader
{
namespace storage
{
namespace book_loader
{

esp_err_t start(const char* path);
esp_err_t start_document(const char* path, const epub::book_t* book, uint8_t spine_index);
bool poll(epub::book_t* book, epub::document_t* document, esp_err_t* result);

} // namespace book_loader
} // namespace storage
} // namespace xreader
