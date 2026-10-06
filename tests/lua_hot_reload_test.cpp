#include "boards/sim/runtime.hpp"
#include "runtime/lua/frontend.hpp"

#include <cstdio>
#include <cstring>

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

static bool write_script(const char* path, const char* source) {
  std::FILE* file = std::fopen(path, "wb");
  if (file == nullptr) return false;
  const std::size_t size = std::strlen(source);
  const bool ok = std::fwrite(source, 1, size, file) == size;
  return std::fclose(file) == 0 && ok;
}

int main() {
  const char* path = "build/hot_reload.lua";
  const char* v1 = R"lua(
function init(reload)
    if not reload then shell.state.set("hot.keep", 1) end
    shell.state.set("hot.version", 1)
    return true
end
function render() shell.api.clear(15); shell.api.present() end
function on_event(ev) return false end
function tick(now) end
)lua";
  const char* v2 = R"lua(
function init(reload)
    shell.state.set("hot.version", 2)
    return true
end
function render() shell.api.clear(0); shell.api.present() end
function on_event(ev)
    if ev.type == "key" and ev.key == "menu" then
        shell.api.reload()
        return true
    end
    return false
end
function tick(now) end
)lua";
  const char* broken = R"lua(
shell.state.set("hot.keep", 999)
error("intentional hot reload failure")
)lua";
  const char* v3 = R"lua(
function init(reload)
    shell.state.set("hot.version", 3)
    return true
end
function render() shell.api.clear(5); shell.api.present() end
function on_event(ev) return false end
function tick(now) end
)lua";

  test::expect(write_script(path, v1), "writes initial Lua source");

  board::sim::runtime sim{};
  board::sim::init(sim);
  state::store memory{};
  state::store persistent{};
  lua_app::context application{};
  test::expect(lua_app::init(application, sim.capabilities, memory, persistent, path),
               "hot reload frontend initializes");
  test::expect(application.lua.generation == 1, "initial Lua generation is one");
  test::expect(state::get(memory, "hot.version", std::int64_t{0}) == 1,
               "initial Lua source is active");

  state::set(memory, "app.page.current", std::int64_t{6});
  state::set(memory, "hot.keep", std::int64_t{77});
  const auto first_pixel = sim.framebuffer[0];

  test::expect(write_script(path, v2), "writes second Lua source");
  test::expect(lua_app::reload(application), "manual Lua VM reload succeeds");
  test::expect(application.lua.generation == 2, "Lua generation increments after reload");
  test::expect(state::get(memory, "app.page.current", std::int64_t{-1}) == 6,
               "current page survives Lua reload");
  test::expect(state::get(memory, "hot.keep", std::int64_t{0}) == 77,
               "shared state survives Lua reload");
  test::expect(state::get(memory, "hot.version", std::int64_t{0}) == 2,
               "new Lua source takes effect");
  test::expect(sim.framebuffer[0] != first_pixel, "new Lua renderer updates framebuffer");

  test::expect(write_script(path, broken), "writes broken Lua source");
  test::expect(!lua_app::reload(application), "broken Lua source is rejected");
  test::expect(application.lua.generation == 2, "failed reload keeps previous generation");
  test::expect(state::get(memory, "hot.keep", std::int64_t{0}) == 77,
               "failed reload rolls back shared state changes");
  test::expect(state::get(memory, "hot.version", std::int64_t{0}) == 2,
               "failed reload keeps old application state");

  test::expect(write_script(path, v3), "writes third Lua source");
  board::sim::inject(sim, event::key(event::key_code::menu));
  lua_app::pump(application);
  test::expect(application.lua.generation == 3,
               "shell.api.reload request restarts Lua after callback returns");
  test::expect(state::get(memory, "hot.version", std::int64_t{0}) == 3,
               "deferred Lua-requested reload activates latest source");
  test::expect(state::get(memory, "app.page.current", std::int64_t{-1}) == 6,
               "Lua-requested reload also preserves app state");

  lua_app::close(application);
  std::remove(path);
  std::printf("lua hot reload tests: %d checks, %d failures\n", test::checks, test::failures);
  return test::failures == 0 ? 0 : 1;
}
