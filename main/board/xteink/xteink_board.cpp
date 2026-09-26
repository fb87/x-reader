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
#if XREADER_XTEINK_CONFIGURED
    // Fill these only after the exact Xteink revision has been identified.
    capabilities->configured = true;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
    return ESP_OK;
}

} // namespace xteink
} // namespace board
} // namespace xreader
