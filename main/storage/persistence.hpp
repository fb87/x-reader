#pragma once

#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace storage
{
namespace persistence
{

esp_err_t init();
esp_err_t load_page(uint32_t* page);
esp_err_t save_page(uint32_t page);
esp_err_t load_page_for_book(const char* path, uint32_t* page);
esp_err_t save_page_for_book(const char* path, uint32_t page);

} // namespace persistence
} // namespace storage
} // namespace xreader
