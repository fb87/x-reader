#include <cstdio>

#include "app/cpp/init.hpp"
#include "boards/sim/runtime.hpp"

int main() {
  board::sim::runtime board{};
  board::sim::init(board);
  state::store memory{}, persistent{};
  state::set(persistent, "app.route.current", "/library");
  state::set(persistent, "app.menu.selected", std::int64_t{2});
  state::set(persistent, "reader.library.selected", std::int64_t{1});
  state::set(persistent, "reader.book.current", std::int64_t{1});
  state::set(persistent, "reader.book.chapter", std::int64_t{2});
  state::set(persistent, "reader.book.page", std::int64_t{7});
  state::set(persistent, "reader.book.progress", std::int64_t{42});
  app::context application{};
  if (!app::init(application, board.capabilities, memory, persistent)) return 1;
  if (app::current_page(application) != app::page::splash) return 2;
  if (state::get(memory, "reader.library.selected", std::int64_t{-1}) != 1) return 3;
  if (!state::save(persistent, "build/persistence_test.db")) return 4;
  state::store restored{};
  if (!state::load(restored, "build/persistence_test.db")) return 5;
  if (std::strcmp(state::get(restored, "app.route.current", ""), "/library") != 0) return 6;
  if (state::get(restored, "reader.book.page", std::int64_t{-1}) != 7) return 7;
  if (state::get(restored, "reader.book.progress", std::int64_t{-1}) != 42) return 8;
  std::printf("persistence tests: passed\n");
  return 0;
}
