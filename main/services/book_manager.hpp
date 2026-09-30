#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "services/library_index.hpp"

namespace xreader
{
namespace services
{
namespace book_manager
{

struct result_t
{
    uint16_t imported;
    uint16_t removed;
    esp_err_t error;
};

result_t import_books(const char* mount_path);
result_t cleanup(const char* mount_path);
esp_err_t rename_book(const char* path, const char* new_name, char* new_path,
                      size_t new_path_capacity);
esp_err_t delete_book(const char* path);
uint16_t duplicate_count(const library_index::catalog_t* catalog);

} // namespace book_manager
} // namespace services
} // namespace xreader
