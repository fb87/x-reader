#pragma once

#include <cstddef>

namespace wifi {

/** @brief One Wi-Fi access point discovered by a board implementation. */
struct access_point {
    const char* ssid = nullptr;
    int rssi = 0;
    bool secured = false;
};

/** @brief Callback invoked for each scanned Wi-Fi access point. */
using scan_fn = bool (*)(const access_point& value, void* user);

/** @brief Generic Wi-Fi capability. */
struct device {
    void* context = nullptr;
    bool (*scan)(device& self, scan_fn callback, void* user) = nullptr;
    bool (*connect)(device& self, const char* ssid, const char* password) = nullptr;
    void (*disconnect)(device& self) = nullptr;
    bool (*connected)(device& self) = nullptr;
};

/** @brief Scans visible Wi-Fi networks. */
inline bool scan(device& self, scan_fn callback, void* user)
{
    return self.scan != nullptr && self.scan(self, callback, user);
}

/** @brief Connects to an access point. */
inline bool connect(device& self, const char* ssid, const char* password)
{
    return self.connect != nullptr && self.connect(self, ssid, password);
}

/** @brief Disconnects the current Wi-Fi connection. */
inline void disconnect(device& self)
{
    if (self.disconnect != nullptr) {
        self.disconnect(self);
    }
}

/** @brief Returns true when the board reports an active Wi-Fi connection. */
inline bool connected(device& self)
{
    return self.connected != nullptr && self.connected(self);
}

} // namespace wifi

namespace bluetooth {

/** @brief Minimal generic Bluetooth capability placeholder for future reader features. */
struct device {
    void* context = nullptr;
    bool (*enabled)(device& self) = nullptr;
    bool (*set_enabled)(device& self, bool value) = nullptr;
};

} // namespace bluetooth
