#pragma once

#include <cstdint>

namespace platform {

/** @brief Generic platform services required by the core runtime. */
struct device {
    void* context;
    std::uint32_t (*now_ms)(device& self);
    int (*battery_percent)(device& self);
};

/** @brief Returns monotonic platform time in milliseconds. */
inline std::uint32_t now_ms(device& self)
{
    return self.now_ms == nullptr ? 0U : self.now_ms(self);
}

/** @brief Returns battery level or -1 when unavailable. */
inline int battery_percent(device& self)
{
    return self.battery_percent == nullptr ? -1 : self.battery_percent(self);
}

} // namespace platform
