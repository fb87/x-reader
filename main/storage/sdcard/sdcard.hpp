#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "sdmmc_cmd.h"

namespace xreader
{
namespace storage
{
namespace sdcard
{

struct config_t
{
    spi_host_device_t spi_host;
    gpio_num_t cs_pin;
    const char* mount_path;
    uint32_t max_files;
};

struct device_t
{
    sdmmc_card_t* card;
    bool mounted;
};

esp_err_t mount(device_t* device, const config_t* config);
esp_err_t unmount(device_t* device, const char* mount_path);

} // namespace sdcard
} // namespace storage
} // namespace xreader
