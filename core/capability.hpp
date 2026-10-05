#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * @brief Fixed-size capability registry a board populates and `app::init`
 * reads from. Replaces per-board ad hoc wiring (the old C tree passed
 * display/platform pointers around individually) with one lookup keyed by
 * a stable enum, so adding an optional capability never forces unrelated
 * code to change its function signatures.
 *
 * Mandatory at startup: display, input, platform, storage. Everything
 * else (wifi, bluetooth, power, rtc, front_light, usb, secret) is
 * optional -- `has()` returning false for one of these is a board
 * correctly reporting "no such hardware", not a bug.
 */
namespace capability {

/** @brief Stable identifier for a board capability. */
enum class id : std::uint8_t {
  display,
  input,
  platform,
  storage,
  wifi,
  bluetooth,
  power,
  rtc,
  front_light,
  usb,
  secret,
  count,
};

/** @brief Fixed-size capability registry used by the composition root. */
struct registry {
  std::array<void*, static_cast<std::size_t>(id::count)> slots{};
};

/** @brief Registers one capability implementation. */
template <typename T>
inline void set(registry& self, id key, T* value) {
  self.slots[static_cast<std::size_t>(key)] = value;
}

/** @brief Returns a capability implementation, or nullptr when unsupported. */
template <typename T>
inline T* get(registry& self, id key) {
  return static_cast<T*>(self.slots[static_cast<std::size_t>(key)]);
}

/** @brief Returns true when a capability is available. */
inline bool has(const registry& self, id key) {
  return self.slots[static_cast<std::size_t>(key)] != nullptr;
}

}  // namespace capability
