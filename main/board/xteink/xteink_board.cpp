#include "xteink_board.hpp"

#include "xteink_pins.hpp"

namespace xreader
{
namespace board
{
namespace xteink
{

esp_err_t get_capabilities(capabilities_t* capabilities)
{
    if (capabilities == nullptr)
        return ESP_ERR_INVALID_ARG;
    *capabilities = {
        .configured = true,
        .display_width = display_width,
        .display_height = display_height,
        .display_controller = display_controller_ssd1677,
        .storage_bus = storage_bus_spi,
        .has_touch = false,
        .has_rotary = false,
        .has_sd = true,
        .supports_power_control = true,
        .supports_deep_sleep = true,
    };
    return ESP_OK;
}

} // namespace xteink
} // namespace board
} // namespace xreader
