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
    uint8_t orientation;
    uint8_t margin_mode;
    uint8_t paragraph_spacing;
    uint8_t text_alignment;
    uint8_t reverse_page_turn;
    uint8_t invert_colors;
    uint8_t show_clock;
    uint32_t sleep_timeout_minutes;
};

struct bookmark_t
{
    uint32_t spine;
    uint32_t page;
};

static constexpr uint8_t max_bookmarks_per_book = 8;

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
esp_err_t load_bookmarks_for_book(const char* path, bookmark_t* bookmarks, uint8_t capacity,
                                  uint8_t* count);
esp_err_t save_bookmarks_for_book(const char* path, const bookmark_t* bookmarks, uint8_t count);
esp_err_t copy_book_state(const char* old_path, const char* new_path);

} // namespace persistence
} // namespace storage
} // namespace xreader
