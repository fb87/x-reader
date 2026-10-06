#pragma once

#if defined(CONFIG_PLUGIN_WIFI)

#include "app/cpp/model.hpp"
#include "core/connectivity.hpp"
#include "core/router.hpp"
#include "plugins/wifi/cpp/pages/wifi.hpp"

namespace plugins::wifi::routes {

inline bool open_page(const router::request& request, void* user) {
  auto& self = *static_cast<app::context*>(user);
  app::apply_page(self, app::page::connectivity);
  app::bind_page(self,
                 app::page_binding{
                     .render = pages::wifi::render,
                     .event = pages::wifi::event,
                     .tick = pages::wifi::tick,
                 });
  state::set(*self.memory, "app.route.current", request.uri);
  return true;
}

inline const char* menu_value(void* user) {
  auto& self = *static_cast<app::context*>(user);
  return state::get(*self.memory, "network.wifi.connected", false) ? "CONNECTED" : "OFF";
}

inline bool register_all(app::context& self) {
  if (capability::get<::wifi::device>(*self.capabilities) == nullptr) return true;
  return router::register_route(self.router,
                                {.path = "/settings/network",
                                 .title_key = "wifi",
                                 .menu_section = "settings",
                                 .menu_order = 10,
                                 .open = open_page,
                                 .user = &self,
                                 .menu_value = menu_value});
}

}  // namespace plugins::wifi::routes

#endif  // CONFIG_PLUGIN_WIFI
