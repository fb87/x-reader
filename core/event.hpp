#pragma once

#include <cstdint>

/**
 * @brief Input events, ported from `xr_event_t`/`xr_key_t` (include/xr/xr_hal.h).
 *
 * A board's main loop builds one `event::value` per hardware input (rotary
 * turn/push, touch tap) and feeds it into `shell::dispatch`. No driver
 * interface is imposed beyond producing this value; everything above this
 * header is hardware-independent.
 */
namespace event {

/** @brief Logical key identities a board's input can produce. */
enum class key_code {
  none = 0,
  up,
  down,
  left,
  right,
  ok,
  back,
  menu,
  next,  ///< dedicated page-turn-forward button, where present.
  prev,  ///< dedicated page-turn-back button, where present.
  power,
};

/** @brief Discriminates which fields of `value` are meaningful. */
enum class type { none = 0, key, tap };

/** @brief One input event: either a key press or a touch tap, never both. */
struct value {
  type kind = type::none;
  key_code key = key_code::none;
  bool long_press = false;
  std::int16_t x = 0;  ///< meaningful only when kind == type::tap.
  std::int16_t y = 0;  ///< meaningful only when kind == type::tap.
};

/** @brief Builds a key event, optionally flagged as a long press. */
constexpr value key(key_code code, bool long_press = false) {
  return value{type::key, code, long_press, 0, 0};
}

/** @brief Builds a touch-tap event at the given screen coordinates. */
constexpr value tap(int x, int y) {
  return value{type::tap, key_code::none, false, static_cast<std::int16_t>(x),
               static_cast<std::int16_t>(y)};
}

}  // namespace event
