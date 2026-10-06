#pragma once

#include <cstdint>

namespace event {

/** @brief Logical input keys independent from concrete hardware. */
enum class key_code {
    none,
    up,
    down,
    left,
    right,
    ok,
    back,
    menu,
};

/** @brief Input-event type. */
enum class type {
    none,
    key,
    tap,
    press,
    release,
};

/** @brief Generic input event delivered to the application. */
struct value {
    type event_type = type::none;
    key_code key = key_code::none;
    int x = 0;
    int y = 0;
    std::uint32_t duration_ms = 0;
    bool long_press = false;
};

/** @brief Creates a key event. */
constexpr value key(key_code code, std::uint32_t duration_ms = 0, bool long_press = false)
{
    return {.event_type = type::key, .key = code, .duration_ms = duration_ms, .long_press = long_press};
}

inline constexpr std::uint32_t long_press_threshold_ms = 700;

/** @brief Creates a touchscreen tap event. */
constexpr value tap(int x, int y)
{
    return {.event_type = type::tap, .x = x, .y = y};
}

} // namespace event
