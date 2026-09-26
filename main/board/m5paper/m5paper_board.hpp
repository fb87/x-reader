#pragma once

#include "esp_err.h"

namespace xreader
{
namespace board
{
namespace m5paper
{

esp_err_t power_on();
esp_err_t power_off();
esp_err_t enter_deep_sleep(uint64_t wakeup_us);

} // namespace m5paper
} // namespace board
} // namespace xreader
