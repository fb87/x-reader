#include "persistence.hpp"

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
}

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

} // namespace persistence
} // namespace storage
} // namespace xreader
