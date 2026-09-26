#pragma once

#include "esp_err.h"

namespace xreader
{
namespace storage
{
namespace book_loader
{

esp_err_t start(const char* path);

}
} // namespace storage
} // namespace xreader
