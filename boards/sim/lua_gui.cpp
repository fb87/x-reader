#include "runtime/lua/frontend.hpp"
#include "boards/sim/runtime.hpp"
#include "boards/sim/wayland_backend.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

namespace {

namespace fs = std::filesystem;

/** @brief Computes a cheap fingerprint of reloadable files below `root`. */
std::size_t source_fingerprint(const char* root, const char* extension) {
  std::size_t fingerprint = 1469598103934665603ULL;
  std::error_code ec;
  if (!fs::exists(root, ec)) return fingerprint;
  for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec),
       end;
       it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (!it->is_regular_file(ec) || it->path().extension() != extension) continue;
    const auto path_hash = std::hash<std::string>{}(it->path().string());
    const auto size = static_cast<std::size_t>(it->file_size(ec));
    const auto stamp = static_cast<std::size_t>(it->last_write_time(ec).time_since_epoch().count());
    fingerprint ^= path_hash + 0x9e3779b97f4a7c15ULL + (fingerprint << 6U) + (fingerprint >> 2U);
    fingerprint ^= size + stamp + 0x9e3779b97f4a7c15ULL + (fingerprint << 6U) +
                   (fingerprint >> 2U);
  }
  return fingerprint;
}


}  // namespace

/** @brief Runs the interactive Wayland Lua simulator. */
int main() {
  board::sim::runtime sim{};
  board::sim::init(sim);
  if (const char* root = std::getenv("XREADER_SDCARD")) board::sim::mount(sim, root);

  board::sim::wayland::backend window{};
  if (!board::sim::wayland::open(window, sim, "Ebook Reader - Lua / M5Paper Simulator")) {
    board::sim::wayland::close(window);
    return 2;
  }

  state::store memory{};
  state::store persistent{};
  lua_app::context application{};
  if (!lua_app::init(application, sim.capabilities, memory, persistent)) {
    board::sim::wayland::close(window);
    return 3;
  }

  auto last = std::chrono::steady_clock::now();
  auto last_watch = last;
  std::size_t lua_fingerprint = source_fingerprint("app/lua", ".lua") ^
                                source_fingerprint("runtime/lua", ".lua") ^
                                source_fingerprint("plugins", ".lua");
  std::size_t language_fingerprint = source_fingerprint("lang", ".txt");
  bool dirty = true;

  while (window.running) {
    board::sim::wayland::pump(window);
    if (board::sim::wayland::take_manual_reload(window)) {
      std::fprintf(stderr, "lua: manual reload requested (F5)\n");
      lua_app::request_reload(application);
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
    if (elapsed > 0) {
      board::sim::advance(sim, static_cast<std::uint32_t>(elapsed));
      last = now;
    }
    if (now - last_watch >= std::chrono::milliseconds(250)) {
      const std::size_t fingerprint = source_fingerprint("app/lua", ".lua") ^
                                      source_fingerprint("runtime/lua", ".lua") ^
                                      source_fingerprint("plugins", ".lua");
      const std::size_t translations = source_fingerprint("lang", ".txt");
      if (fingerprint != lua_fingerprint || translations != language_fingerprint) {
        lua_fingerprint = fingerprint;
        language_fingerprint = translations;
        std::fprintf(stderr, "lua: source/translation change detected, reloading VM\n");
        lua_app::request_reload(application);
      }
      last_watch = now;
    }

    const int refresh_before = sim.refresh_count;
    lua_app::pump(application);
    dirty = dirty || sim.refresh_count != refresh_before;
    if (dirty && board::sim::wayland::present(window, sim)) dirty = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  lua_app::checkpoint(application);
  lua_app::close(application);
  board::sim::wayland::close(window);
  return 0;
}
