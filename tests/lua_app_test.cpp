#include <cstdio>
#include <cstring>

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

static void settle(board::sim::runtime& sim, lua_app::context& app) {
  board::sim::advance(sim, 20);
  lua_app::pump(app);
}

int main() {
  board::sim::runtime sim{};
  board::sim::init(sim);
  state::store memory{};
  state::store persistent{};
  lua_app::context application{};

  test::expect(lua_app::init(application, sim.capabilities, memory, persistent, "app/lua/init.lua"),
               "Lua frontend initializes");
  test::expect(app::current_page(application.native) == app::page::splash,
               "Lua app starts on splash");
  test::expect(sim.refresh_count > 0, "Lua GUI performs an initial display refresh");
  bool has_ink = false;
  for (const auto value : sim.framebuffer) {
    if (value != 0xffU) {
      has_ink = true;
      break;
    }
  }
  test::expect(has_ink, "Lua-defined splash renders into the framebuffer");

  board::sim::advance(sim, 1600);
  lua_app::pump(application);
  test::expect(app::current_page(application.native) == app::page::home,
               "Lua tick leaves splash after timeout");
  test::expect(sim.refresh_count >= 2, "Lua page transition renders the Home GUI");

  const auto selection_before_long = state::get(memory, "app.menu.selected", std::int64_t{0});
  board::sim::inject(sim, event::key(event::key_code::down, 900, true));
  settle(sim, application);
  test::expect(state::get(memory, "app.menu.selected", std::int64_t{-1}) == selection_before_long,
               "shell consumes long press before Lua page handler");
  test::expect(state::get(memory, "app.input.long_press_duration_ms", std::int64_t{0}) == 900,
               "Lua frontend shares native shell long-press handling");

  board::sim::rotary_push(sim);
  settle(sim, application);
  test::expect(app::current_page(application.native) == app::page::library,
               "Lua home activation opens Library");

  board::sim::inject(sim, event::key(event::key_code::back));
  settle(sim, application);
  test::expect(app::current_page(application.native) == app::page::home, "Lua back returns Home");

  board::sim::rotary_right(sim);
  board::sim::rotary_right(sim);
  board::sim::rotary_right(sim);
  settle(sim, application);
  board::sim::rotary_push(sim);
  settle(sim, application);
  test::expect(app::current_page(application.native) == app::page::settings,
               "Lua navigation opens Settings");

  const auto font_before = state::get(memory, "reader.settings.font_size", std::int64_t{1});
  board::sim::rotary_right(sim);
  board::sim::rotary_right(sim);
  settle(sim, application);
  board::sim::rotary_push(sim);
  settle(sim, application);
  test::expect(state::get(memory, "reader.settings.font_size", std::int64_t{1}) != font_before,
               "Lua settings changes reader font state");

  const auto frame_before_language = sim.framebuffer;
  for (int i = 0; i < 4; ++i) board::sim::rotary_right(sim);
  settle(sim, application);
  board::sim::rotary_push(sim);
  settle(sim, application);
  test::expect(std::strcmp(state::get(memory, "system.language", "en"), "vi") == 0,
               "Lua settings cycles language to Vietnamese");
  test::expect(frame_before_language != sim.framebuffer, "Language change redraws translated GUI");

  lua_app::close(application);
  std::printf("lua app tests: %d checks, %d failures\n", test::checks, test::failures);
  return test::failures == 0 ? 0 : 1;
}
