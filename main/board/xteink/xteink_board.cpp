#include "xteink_board.hpp"

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
    *capabilities = {};
    return ESP_ERR_NOT_SUPPORTED;
}

} // namespace xteink
} // namespace board
} // namespace xreader
