#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace display {
struct device;
}
namespace input {
struct device;
}
namespace platform {
struct device;
}
namespace storage {
struct device;
}
namespace wifi {
struct device;
}
namespace secret {
struct store;
}

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

/** @brief Compile-time mapping from capability interface type to id. */
template <typename T>
struct traits;

template <>
struct traits<display::device> {
  static constexpr id value = id::display;
};
template <>
struct traits<input::device> {
  static constexpr id value = id::input;
};
template <>
struct traits<platform::device> {
  static constexpr id value = id::platform;
};
template <>
struct traits<storage::device> {
  static constexpr id value = id::storage;
};
template <>
struct traits<wifi::device> {
  static constexpr id value = id::wifi;
};
template <>
struct traits<secret::store> {
  static constexpr id value = id::secret;
};

/** @brief Returns a capability implementation by interface type. */
template <typename T>
inline T* get(registry& self) {
  return static_cast<T*>(self.slots[static_cast<std::size_t>(traits<T>::value)]);
}

/** @brief Explicit-id lookup retained for unmapped capability families. */
template <typename T>
inline T* get(registry& self, id key) {
  return static_cast<T*>(self.slots[static_cast<std::size_t>(key)]);
}

/** @brief Returns true when a capability is available. */
inline bool has(const registry& self, id key) {
  return self.slots[static_cast<std::size_t>(key)] != nullptr;
}

}  // namespace capability
