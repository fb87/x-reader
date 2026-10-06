#include "core/state.hpp"
#include "runtime/lua/frontend.hpp"
#include "selected_board.hpp"

#include <cstdio>

/** @brief Composition root for the Lua application frontend. */
int main() {
  selected_board::runtime board{};
  selected_board::init(board);

  state::store memory{};
  state::store persistent{};
  (void)state::load(persistent, "build/state-lua.db");

  lua_app::context application{};
  if (!lua_app::init(application, board.capabilities, memory, persistent)) {
    std::fprintf(stderr, "Lua application initialization failed\n");
    return 1;
  }

  lua_app::pump(application);
  lua_app::checkpoint(application);
  (void)state::save(persistent, "build/state-lua.db");
  lua_app::close(application);
  return 0;
}
