#pragma once

#if defined(CONFIG_PLUGIN_WIFI)

#include "app/cpp/model.hpp"
#include "core/connectivity.hpp"

namespace plugins::wifi {

struct scan_context {
  app::context* app = nullptr;
  int count = 0;
};

inline bool scan_entry(const ::wifi::access_point& point, void* user) {
  auto& scan = *static_cast<scan_context*>(user);
  if (scan.count == 0) {
    state::set(*scan.app->memory, "network.wifi.scan.first_ssid", point.ssid);
    state::set(*scan.app->memory, "network.wifi.scan.first_rssi",
               static_cast<std::int64_t>(point.rssi));
  }
  ++scan.count;
  return true;
}

inline bool scan(app::context& self) {
  auto* device = capability::get<::wifi::device>(*self.capabilities);
  if (device == nullptr) {
    state::set(*self.memory, "network.wifi.available", false);
    return false;
  }
  state::set(*self.memory, "network.wifi.available", true);
  scan_context scan_state{.app = &self};
  const bool ok = ::wifi::scan(*device, scan_entry, &scan_state);
  state::set(*self.memory, "network.wifi.scan.count", static_cast<std::int64_t>(scan_state.count));
  ++self.invalidations;
  return ok;
}

inline bool connect_first(app::context& self) {
  auto* device = capability::get<::wifi::device>(*self.capabilities);
  if (device == nullptr) return false;
  const char* ssid = state::get(*self.memory, "network.wifi.scan.first_ssid", "");
  if (ssid[0] == '\0' || !::wifi::connect(*device, ssid, "test-password")) return false;
  state::set(*self.memory, "network.wifi.connected", true);
  state::set(*self.memory, "network.wifi.ssid", ssid);
  ++self.invalidations;
  return true;
}

}  // namespace plugins::wifi

#endif  // CONFIG_PLUGIN_WIFI
