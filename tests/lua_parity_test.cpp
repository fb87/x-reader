#include <algorithm>
#include <cstdio>

#include "app/cpp/init.hpp"
#include "boards/sim/runtime.hpp"
#include "runtime/lua/frontend.hpp"

namespace test {
int checks = 0;
int failures = 0;
void expect(bool condition, const char* name) {
  ++checks;
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", name);
  }
}
}  // namespace test

static bool same_frame(const board::sim::runtime& a, const board::sim::runtime& b) {
  return std::equal(a.framebuffer.begin(), a.framebuffer.end(), b.framebuffer.begin());
}

int main() {
  board::sim::runtime cpp_sim{};
  board::sim::runtime lua_sim{};
  board::sim::init(cpp_sim);
  board::sim::init(lua_sim);
  state::store cpp_memory{}, cpp_persistent{};
  state::store lua_memory{}, lua_persistent{};
  app::context cpp_app{};
  lua_app::context lua_app_context{};

  test::expect(app::init(cpp_app, cpp_sim.capabilities, cpp_memory, cpp_persistent),
               "native frontend initializes");
  test::expect(lua_app::init(lua_app_context, lua_sim.capabilities, lua_memory, lua_persistent,
                             "app/lua/init.lua"),
               "Lua frontend initializes");
  test::expect(same_frame(cpp_sim, lua_sim), "default Lua splash matches C++ framebuffer");

  board::sim::advance(cpp_sim, 1600);
  board::sim::advance(lua_sim, 1600);
  app::pump(cpp_app);
  lua_app::pump(lua_app_context);
  test::expect(app::current_page(cpp_app) == app::page::home &&
                   app::current_page(lua_app_context.native) == app::page::home,
               "both frontends reach Home");
  test::expect(same_frame(cpp_sim, lua_sim), "default Lua Home matches C++ framebuffer");
  lua_app::close(lua_app_context);

  board::sim::runtime styled_sim{};
  board::sim::init(styled_sim);
  state::store styled_memory{}, styled_persistent{};
  lua_app::context styled{};
  test::expect(lua_app::init(styled, styled_sim.capabilities, styled_memory, styled_persistent,
                             "tests/lua_styled_app.lua"),
               "styled Lua frontend initializes");
  board::sim::advance(styled_sim, 1600);
  lua_app::pump(styled);
  test::expect(!same_frame(cpp_sim, styled_sim),
               "Lua style override intentionally changes framebuffer");
  lua_app::close(styled);

  std::printf("lua parity tests: %d checks, %d failures\n", test::checks, test::failures);
  return test::failures == 0 ? 0 : 1;
}
