#include "app/app.hpp"
#include "selected_board.hpp"

#include <cstdlib>
#include <thread>

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#include "esp_log.h"
#endif

/**
 * @brief Composition root: wires the build-selected board's capabilities to the
 * app and runs its event loop headlessly. `app/` and `reader/` contain no board
 * names; the board is chosen entirely by `BOARD=` at build time (see Makefile).
 */
static int run_application() {
#ifdef ESP_PLATFORM
  static EXT_RAM_BSS_ATTR selected_board::runtime board{};
  static EXT_RAM_BSS_ATTR app::context application{};
  static EXT_RAM_BSS_ATTR app::pages nav{};
  static state::store memory{};
  static state::store persistent{};
#else
  selected_board::runtime board{};
#endif
#ifdef ESP_PLATFORM
  static constexpr const char* tag = "xreader";
  ESP_LOGI(tag, "starting M5Paper board initialization");
  if (!selected_board::init(board)) {
    ESP_LOGE(tag, "M5Paper board initialization failed");
    return 1;
  }
  ESP_LOGI(tag, "M5Paper display and input initialized");
#else
  selected_board::init(board);
#endif

#ifdef ESP_PLATFORM
  const char* root = "/sdcard";
#else
  const char* root = std::getenv("XREADER_SDCARD");
  char default_root[storage::path_max];
  if (root == nullptr) {
    const char* home = std::getenv("HOME");
    std::snprintf(default_root, sizeof(default_root), "%s/data/sdcard", home != nullptr ? home : ".");
    root = default_root;
  }
#endif
#ifdef ESP_PLATFORM
  if (!selected_board::mount(board, root)) {
    ESP_LOGE(tag, "SD card mount failed");
    return 1;
  }
  ESP_LOGI(tag, "SD card mounted at %s", root);
#else
  selected_board::mount(board, root);
#endif

#ifndef ESP_PLATFORM
  app::context application{};
  app::pages nav{};
  state::store memory{};
  state::store persistent{};
#endif

  static constexpr shell::theme theme{
      &xr_font_alegreya_14, &xr_font_alegreya_18, &xr_font_alegreya_bold_18,
      &xr_font_alegreya_bold_26, &xr_font_alegreya_20, 44, 64, 16, 72,
  };

  if (!app::init(application, nav, board.capabilities, memory, persistent, theme, root)) {
#ifdef ESP_PLATFORM
    ESP_LOGE(tag, "application initialization failed");
#endif
    return 1;
  }

  // Show the boot screen before the potentially slow storage walk (real SD cards can
  // have many files), matching the old app_main.cpp's ordering.
  shell::tick(application.shell);
  shell::flush(application.shell);
  app::scan_library(application);

  bool running = true;
  while (running) {
    app::pump(application);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  app::checkpoint(application);
  return 0;
}

#ifdef ESP_PLATFORM
extern "C" void app_main() { (void)run_application(); }
#else
int main() { return run_application(); }
#endif
