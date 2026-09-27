#pragma once

#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace storage
{
namespace persistence
{

struct settings_t
{
    uint8_t text_scale;
    uint8_t line_spacing;
    uint8_t refresh_mode;
    uint32_t sleep_timeout_minutes;
};

esp_err_t init();
void default_settings(settings_t* settings);
esp_err_t load_settings(settings_t* settings);
esp_err_t save_settings(const settings_t* settings);
esp_err_t load_page(uint32_t* page);
esp_err_t save_page(uint32_t page);
esp_err_t load_page_for_book(const char* path, uint32_t* page);
esp_err_t save_page_for_book(const char* path, uint32_t page);
esp_err_t load_position_for_book(const char* path, uint32_t* spine, uint32_t* page);
esp_err_t save_position_for_book(const char* path, uint32_t spine, uint32_t page);

} // namespace persistence
} // namespace storage
} // namespace xreader
