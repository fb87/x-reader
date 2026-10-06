#pragma once

#if defined(CONFIG_PLUGIN_WIFI)

#include "app/cpp/pages/common.hpp"
#include "app/cpp/routes.hpp"
#include "plugins/wifi/cpp/service.hpp"

namespace plugins::wifi::pages::wifi {

inline void render(app::context& self) {
  auto& d = *self.shell.display;
  canvas::fill(d, {0, 0, d.width, d.height}, canvas::gray::white);
  app::pages::draw_status(self, "WI-FI");
  const char* labels[] = {"SCAN NETWORKS", "CONNECT FIRST", "BACK"};
  for (int i = 0; i < 3; ++i) app::pages::draw_row(self, i, 90 + i * 76, labels[i]);
  const char* ssid = state::get(*self.memory, "network.wifi.ssid", "");
  if (ssid[0] != '\0') {
    text::draw(d, 24, 350, "CONNECTED:", 2);
    text::draw(d, 160, 350, ssid, 2);
  }
}

inline bool event(app::context& self, const event::value& value) {
  auto activate = [&](int selected) {
    if (selected == 0) return plugins::wifi::scan(self);
    if (selected == 1) return plugins::wifi::connect_first(self);
    return app::routes::push(self, "/settings");
  };

  if (value.event_type == event::type::tap) {
    if (value.y < 90 || value.y >= 90 + 3 * 76) return false;
    const int selected = (value.y - 90) / 76;
    state::set(*self.memory, "app.menu.selected", static_cast<std::int64_t>(selected));
    return activate(selected);
  }
  if (value.event_type != event::type::key) return false;
  if (value.key == event::key_code::back) return app::routes::push(self, "/settings");
  if (value.key == event::key_code::up) {
    app::move_selection(self, -1, 3);
    return true;
  }
  if (value.key == event::key_code::down) {
    app::move_selection(self, 1, 3);
    return true;
  }
  if (value.key == event::key_code::ok)
    return activate(static_cast<int>(app::selection(self)));
  return false;
}

inline void tick(app::context&, std::uint32_t) {}

}  // namespace plugins::wifi::pages::wifi

#endif  // CONFIG_PLUGIN_WIFI
