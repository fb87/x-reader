#pragma once

#include <cstddef>

/**
 * @brief Generic Wi-Fi and Bluetooth capability contracts.
 *
 * Neither M5Paper nor the simulator has real Bluetooth radio hardware
 * today, so `bluetooth::device` legitimately has no implementation on any
 * current board -- per the optional-capability rule, the app must treat
 * `capability::has(..., capability::id::bluetooth) == false` as correct
 * behavior, not a stub awaiting completion.
 */
namespace wifi {

/** @brief One Wi-Fi access point discovered by a board implementation. */
struct access_point {
  const char* ssid = nullptr;
  int rssi = 0;
  bool secured = false;
};

/** @brief Callback invoked for each scanned Wi-Fi access point; return false to stop early. */
using scan_fn = bool (*)(const access_point& value, void* user);

/** @brief Generic Wi-Fi capability supplied by a board implementation. */
struct device {
  void* context = nullptr;
  bool (*scan)(device& self, scan_fn callback, void* user) = nullptr;
  bool (*connect)(device& self, const char* ssid, const char* password) = nullptr;
  void (*disconnect)(device& self) = nullptr;
  bool (*connected)(device& self) = nullptr;
};

/** @brief Scans visible Wi-Fi networks. */
inline bool scan(device& self, scan_fn callback, void* user) {
  return self.scan != nullptr && self.scan(self, callback, user);
}

/** @brief Connects to an access point. Caller must source `password` from secret::store, never
 * from ordinary persisted state -- see core/secret.hpp. */
inline bool connect(device& self, const char* ssid, const char* password) {
  return self.connect != nullptr && self.connect(self, ssid, password);
}

/** @brief Disconnects the current Wi-Fi connection, if any. */
inline void disconnect(device& self) {
  if (self.disconnect != nullptr) self.disconnect(self);
}

/** @brief Returns true when the board reports an active Wi-Fi connection. */
inline bool connected(device& self) { return self.connected != nullptr && self.connected(self); }

}  // namespace wifi

namespace bluetooth {

/** @brief Generic Bluetooth capability placeholder. No board implements this today. */
struct device {
  void* context = nullptr;
  bool (*enabled)(device& self) = nullptr;
  bool (*set_enabled)(device& self, bool value) = nullptr;
};

}  // namespace bluetooth
