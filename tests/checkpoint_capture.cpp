#include "app/cpp/init.hpp"
#include "boards/sim/runtime.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
  const char* directory = std::getenv("XREADER_CHECKPOINT_DIR");
  if (directory == nullptr || directory[0] == '\0') return 2;

  board::sim::runtime board{};
  board::sim::init(board);
  if (const char* root = std::getenv("XREADER_SDCARD")) board::sim::mount(board, root);
  state::store memory{}, persistent{};
  app::context application{};
  if (!app::init(application, board.capabilities, memory, persistent)) return 3;

  char path[512]{};
  std::snprintf(path, sizeof(path), "%s/native-splash.pgm", directory);
  if (!board::sim::dump_framebuffer(board, path)) return 4;

  board::sim::advance(board, 1600);
  app::pump(application);
  std::snprintf(path, sizeof(path), "%s/native-home.pgm", directory);
  if (!board::sim::dump_framebuffer(board, path)) return 5;
  return 0;
}
