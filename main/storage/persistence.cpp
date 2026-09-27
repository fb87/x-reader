#include "persistence.hpp"

#include <stdio.h>
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

namespace xreader
{
namespace storage
{
namespace persistence
{

namespace
{
static constexpr const char* namespace_name = "reader";
static constexpr const char* page_key = "page";
static constexpr const char* text_scale_key = "text_scale";
static constexpr const char* line_spacing_key = "line_space";
static constexpr const char* refresh_mode_key = "refresh";
static constexpr const char* sleep_timeout_key = "sleep_min";
static char cached_path[512] = {};
static uint32_t cached_spine = 0;
static uint32_t cached_page = 0;
static bool cache_valid = false;

static uint32_t book_hash(const char* path)
{
    uint32_t hash = 2166136261U;
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(path);
         *cursor != '\0'; ++cursor)
        hash = (hash ^ *cursor) * 16777619U;
    return hash;
}

static esp_err_t open_page(const char* path, nvs_open_mode_t mode, nvs_handle_t* handle, char* key)
{
    if (path == nullptr || handle == nullptr || key == nullptr)
        return ESP_ERR_INVALID_ARG;
    snprintf(key, 16, "p%08lx", static_cast<unsigned long>(book_hash(path)));
    return nvs_open(namespace_name, mode, handle);
}

static void position_key(const char* prefix, const char* path, char* key)
{
    snprintf(key, 16, "%s%08lx", prefix, static_cast<unsigned long>(book_hash(path)));
}
} // namespace

esp_err_t init()
{
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        error = nvs_flash_erase();
        if (error == ESP_OK)
            error = nvs_flash_init();
    }
    return error;
}

void default_settings(settings_t* settings)
{
    if (settings == nullptr)
        return;
    settings->text_scale = 2;
    settings->line_spacing = 0;
    settings->refresh_mode = 0;
    settings->sleep_timeout_minutes = 60;
}

esp_err_t load_settings(settings_t* settings)
{
    if (settings == nullptr)
        return ESP_ERR_INVALID_ARG;
    default_settings(settings);
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (error != ESP_OK)
        return error;
    uint8_t text_scale = 0;
    uint8_t line_spacing = 0;
    uint8_t refresh_mode = 0;
    uint32_t sleep_timeout = 0;
    if (nvs_get_u8(handle, text_scale_key, &text_scale) == ESP_OK &&
        (text_scale == 1 || text_scale == 2))
        settings->text_scale = text_scale;
    if (nvs_get_u8(handle, line_spacing_key, &line_spacing) == ESP_OK && line_spacing <= 1)
        settings->line_spacing = line_spacing;
    if (nvs_get_u8(handle, refresh_mode_key, &refresh_mode) == ESP_OK && refresh_mode <= 1)
        settings->refresh_mode = refresh_mode;
    if (nvs_get_u32(handle, sleep_timeout_key, &sleep_timeout) == ESP_OK && sleep_timeout > 0)
        settings->sleep_timeout_minutes = sleep_timeout;
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t save_settings(const settings_t* settings)
{
    if (settings == nullptr)
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (error != ESP_OK)
        return error;
    error = nvs_set_u8(handle, text_scale_key, settings->text_scale);
    if (error == ESP_OK)
        error = nvs_set_u8(handle, line_spacing_key, settings->line_spacing);
    if (error == ESP_OK)
        error = nvs_set_u8(handle, refresh_mode_key, settings->refresh_mode);
    if (error == ESP_OK)
        error = nvs_set_u32(handle, sleep_timeout_key, settings->sleep_timeout_minutes);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t load_page(uint32_t* page)
{
    if (page == nullptr)
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (error != ESP_OK)
        return error;
    error = nvs_get_u32(handle, page_key, page);
    nvs_close(handle);
    return error;
}

esp_err_t save_page(uint32_t page)
{
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (error != ESP_OK)
        return error;
    error = nvs_set_u32(handle, page_key, page);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t load_page_for_book(const char* path, uint32_t* page)
{
    if (path == nullptr || page == nullptr)
        return ESP_ERR_INVALID_ARG;
    if (cache_valid && strcmp(path, cached_path) == 0)
    {
        *page = cached_page;
        return ESP_OK;
    }
    nvs_handle_t handle = 0;
    char key[16] = {};
    esp_err_t error = open_page(path, NVS_READONLY, &handle, key);
    if (error != ESP_OK)
        return error;
    error = nvs_get_u32(handle, key, page);
    nvs_close(handle);
    if (error == ESP_OK)
    {
        snprintf(cached_path, sizeof(cached_path), "%s", path);
        cached_spine = 0;
        cached_page = *page;
        cache_valid = true;
    }
    return error;
}

esp_err_t save_page_for_book(const char* path, uint32_t page)
{
    if (path == nullptr)
        return ESP_ERR_INVALID_ARG;
    if (cache_valid && strcmp(path, cached_path) == 0 && cached_page == page)
        return ESP_OK;
    nvs_handle_t handle = 0;
    char key[16] = {};
    esp_err_t error = open_page(path, NVS_READWRITE, &handle, key);
    if (error != ESP_OK)
        return error;
    error = nvs_set_u32(handle, key, page);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    if (error == ESP_OK)
    {
        snprintf(cached_path, sizeof(cached_path), "%s", path);
        cached_spine = 0;
        cached_page = page;
        cache_valid = true;
    }
    return error;
}

esp_err_t load_position_for_book(const char* path, uint32_t* spine, uint32_t* page)
{
    if (path == nullptr || spine == nullptr || page == nullptr)
        return ESP_ERR_INVALID_ARG;
    if (cache_valid && strcmp(path, cached_path) == 0)
    {
        *spine = cached_spine;
        *page = cached_page;
        return ESP_OK;
    }
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (error != ESP_OK)
        return error;
    char page_key_for_book[16] = {};
    char spine_key_for_book[16] = {};
    position_key("p", path, page_key_for_book);
    position_key("s", path, spine_key_for_book);
    error = nvs_get_u32(handle, page_key_for_book, page);
    if (error == ESP_OK)
    {
        esp_err_t spine_error = nvs_get_u32(handle, spine_key_for_book, spine);
        if (spine_error == ESP_ERR_NVS_NOT_FOUND)
        {
            *spine = 0;
            spine_error = ESP_OK;
        }
        error = spine_error;
    }
    nvs_close(handle);
    if (error == ESP_OK)
    {
        snprintf(cached_path, sizeof(cached_path), "%s", path);
        cached_spine = *spine;
        cached_page = *page;
        cache_valid = true;
    }
    return error;
}

esp_err_t save_position_for_book(const char* path, uint32_t spine, uint32_t page)
{
    if (path == nullptr)
        return ESP_ERR_INVALID_ARG;
    if (cache_valid && strcmp(path, cached_path) == 0 && cached_spine == spine &&
        cached_page == page)
        return ESP_OK;
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (error != ESP_OK)
        return error;
    char page_key_for_book[16] = {};
    char spine_key_for_book[16] = {};
    position_key("p", path, page_key_for_book);
    position_key("s", path, spine_key_for_book);
    error = nvs_set_u32(handle, spine_key_for_book, spine);
    if (error == ESP_OK)
        error = nvs_set_u32(handle, page_key_for_book, page);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    if (error == ESP_OK)
    {
        snprintf(cached_path, sizeof(cached_path), "%s", path);
        cached_spine = spine;
        cached_page = page;
        cache_valid = true;
    }
    return error;
}

} // namespace persistence
} // namespace storage
} // namespace xreader
