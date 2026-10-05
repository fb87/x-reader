#include "app/app.hpp"
#include "build/selected_board.hpp"

#include <cstdlib>
#include <thread>

/**
 * @brief Composition root: wires the build-selected board's capabilities to the
 * app and runs its event loop headlessly. `app/` and `reader/` contain no board
 * names; the board is chosen entirely by `BOARD=` at build time (see Makefile).
 */
int main() {
  selected_board::runtime board{};
  selected_board::init(board);

  const char* root = std::getenv("XREADER_SDCARD");
  char default_root[storage::path_max];
  if (root == nullptr) {
    const char* home = std::getenv("HOME");
    std::snprintf(default_root, sizeof(default_root), "%s/data/sdcard", home != nullptr ? home : ".");
    root = default_root;
  }
  selected_board::mount(board, root);

  app::context application{};
  app::pages nav{};
  state::store memory{};
  state::store persistent{};

  static constexpr shell::theme theme{
      &xr_font_alegreya_14, &xr_font_alegreya_18, &xr_font_alegreya_bold_18,
      &xr_font_alegreya_bold_26, &xr_font_alegreya_20, 44, 64, 16, 72,
  };

  if (!app::init(application, nav, board.capabilities, memory, persistent, theme, root)) return 1;

  bool running = true;
  while (running) {
    app::pump(application);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  app::checkpoint(application);
  return 0;
}
