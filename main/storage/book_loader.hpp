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
bool poll(epub::book_t* book, epub::document_t* document, esp_err_t* result);

} // namespace book_loader
} // namespace storage
} // namespace xreader
