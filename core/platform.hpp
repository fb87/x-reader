#pragma once

#include <cstdint>

/**
 * @brief Generic platform services, ported from `xr_platform_ops_t`
 * (include/xr/xr_hal.h).
 *
 * Mandatory capability alongside display/input/storage: every board must
 * populate `now_ms` at minimum. `wall_time` is part of the real hardware
 * contract (the M5Paper board reads an RTC for it) but is currently not
 * consumed by any app page -- kept so the board-level capability isn't
 * lost during the port, not because app/ needs it yet.
 */
namespace platform {

/** @brief Generic platform/time/power capability supplied by a board implementation. */
struct device {
  void* context = nullptr;
  std::uint32_t (*now_ms)(device& self) = nullptr;
  void (*wall_time)(device& self, int& hour, int& minute) = nullptr;
  int (*battery_percent)(device& self) = nullptr;  ///< < 0 = unknown/unavailable.
  /** Enters a board-defined low-power sleep, waking after at most `wake_after_ms` (0 = no
   * timer bound, rely on the board's other wake sources only). Optional: boards without a
   * real low-power state (e.g. the simulator) may leave this null. */
  void (*enter_deep_sleep)(device& self, std::uint32_t wake_after_ms) = nullptr;
};

/** @brief Returns monotonic platform time in milliseconds, or 0 when unavailable. */
inline std::uint32_t now_ms(device& self) { return self.now_ms == nullptr ? 0U : self.now_ms(self); }

/** @brief Reads the wall-clock hour/minute; returns false when unavailable. */
inline bool wall_time(device& self, int& hour, int& minute) {
  if (self.wall_time == nullptr) return false;
  self.wall_time(self, hour, minute);
  return true;
}

/** @brief Returns battery level as a percentage, or -1 when unavailable. */
inline int battery_percent(device& self) {
  return self.battery_percent == nullptr ? -1 : self.battery_percent(self);
}

/** @brief Enters deep sleep when the board supports it; returns false when unsupported. */
inline bool enter_deep_sleep(device& self, std::uint32_t wake_after_ms) {
  if (self.enter_deep_sleep == nullptr) return false;
  self.enter_deep_sleep(self, wake_after_ms);
  return true;
}

}  // namespace platform
