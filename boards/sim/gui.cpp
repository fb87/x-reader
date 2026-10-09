#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "app/cpp/init.hpp"
#include "boards/sim/runtime.hpp"
#include "boards/sim/wayland_backend.hpp"

/** @brief Runs the interactive Wayland simulator. */
int main() {
  board::sim::runtime sim{};
  board::sim::init(sim);
  const char* root = std::getenv("XREADER_SDCARD");
  char default_root[storage::path_max]{};
  if (root == nullptr) {
    const char* home = std::getenv("HOME");
    std::snprintf(default_root, sizeof(default_root), "%s/data/sdcard",
                  home != nullptr ? home : ".");
    root = default_root;
  }
  board::sim::mount(sim, root);

  board::sim::wayland::backend window{};
  if (!board::sim::wayland::open(window, sim, "Ebook Reader - M5Paper Simulator")) {
    board::sim::wayland::close(window);
    return 2;
  }

  state::store memory{};
  state::store persistent{};
  app::context application{};
  if (!app::init(application, sim.capabilities, memory, persistent)) {
    board::sim::wayland::close(window);
    return 3;
  }

  auto last = std::chrono::steady_clock::now();
  bool dirty = true;
  while (window.running) {
    board::sim::wayland::pump(window);

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
    if (elapsed > 0) {
      board::sim::advance(sim, static_cast<std::uint32_t>(elapsed));
      last = now;
    }

    const int refresh_before = sim.refresh_count;
    app::pump(application);
    dirty = dirty || sim.refresh_count != refresh_before;
    if (dirty && board::sim::wayland::present(window, sim)) dirty = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  app::checkpoint(application);
  board::sim::wayland::close(window);
  return 0;
}
