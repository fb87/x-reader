#pragma once

#include "app/cpp/init.hpp"
#include "runtime/lua/runtime.hpp"

/** @brief Lua-owned GUI/application over native reader and board services. */
namespace lua_app {

struct context {
  app::context native{};
  lua::runtime lua{};
};

inline bool dispatch(const event::value& value, void* user) {
  auto& self = *static_cast<context*>(user);
  const bool handled = lua::on_event(self.lua, value);
  if (handled) lua::render(self.lua);
  return handled;
}

inline bool init(context& self, capability::registry& capabilities, state::store& memory,
                 state::store& persistent, const char* script = "app/lua/init.lua") {
  if (!app::init(self.native, capabilities, memory, persistent, false)) return false;
  if (!lua::init(self.lua, self.native, script)) return false;
  self.native.shell.on_event = dispatch;
  self.native.shell.user = &self;
  if (self.native.shell.platform != nullptr) {
    lua::tick(self.lua, platform::now_ms(*self.native.shell.platform));
  }
  return lua::render(self.lua);
}

inline bool reload(context& self) {
  if (!lua::reload(self.lua)) return false;
  if (self.native.shell.platform != nullptr) {
    lua::tick(self.lua, platform::now_ms(*self.native.shell.platform));
  }
  ++self.native.invalidations;
  return lua::render(self.lua);
}

inline void request_reload(context& self) { lua::request_reload(self.lua); }

inline void pump(context& self) {
  const int before = self.native.invalidations;
  if (self.native.shell.platform != nullptr) {
    lua::tick(self.lua, platform::now_ms(*self.native.shell.platform));
  }
  shell::pump(self.native.shell);
  if (lua::take_reload_request(self.lua)) {
    (void)reload(self);
    return;
  }
  if (self.native.invalidations != before) lua::render(self.lua);
}

inline void checkpoint(context& self) { app::checkpoint(self.native); }
inline void close(context& self) { lua::close(self.lua); }

}  // namespace lua_app
