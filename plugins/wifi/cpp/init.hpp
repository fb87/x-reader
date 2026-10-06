#pragma once

#if defined(CONFIG_PLUGIN_WIFI)

#include "app/cpp/model.hpp"
#include "plugins/wifi/cpp/routes.hpp"

namespace plugins::wifi {

inline bool init(app::context& self) { return routes::register_all(self); }

}  // namespace plugins::wifi

#endif  // CONFIG_PLUGIN_WIFI
