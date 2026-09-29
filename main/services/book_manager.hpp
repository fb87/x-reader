#pragma once

#include <stdint.h>

#include "esp_err.h"

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

} // namespace book_manager
} // namespace services
} // namespace xreader
