#include "sdcard.hpp"

#include <stdio.h>

#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

namespace xreader
{
namespace storage
{
namespace sdcard
{

namespace
{

static const char* const tag = "sdcard";

} // namespace

esp_err_t mount(device_t* device, const config_t* config)
{
    if (device == nullptr || config == nullptr || config->mount_path == nullptr ||
        config->max_files == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = config->spi_host;

    sdspi_device_config_t device_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    device_config.host_id = config->spi_host;
    device_config.gpio_cs = config->cs_pin;

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = static_cast<int>(config->max_files),
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    esp_err_t error = esp_vfs_fat_sdspi_mount(config->mount_path, &host, &device_config,
                                              &mount_config, &device->card);
    if (error != ESP_OK)
    {
        ESP_LOGW(tag, "SD card mount failed: %s", esp_err_to_name(error));
        return error;
    }

    device->mounted = true;
    ESP_LOGI(tag, "SD card mounted at %s", config->mount_path);
    sdmmc_card_print_info(stdout, device->card);
    return ESP_OK;
}

esp_err_t unmount(device_t* device, const char* mount_path)
{
    if (device == nullptr || mount_path == nullptr || !device->mounted)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t error = esp_vfs_fat_sdcard_unmount(mount_path, device->card);
    if (error == ESP_OK)
    {
        device->mounted = false;
    }
    return error;
}

} // namespace sdcard
} // namespace storage
} // namespace xreader
