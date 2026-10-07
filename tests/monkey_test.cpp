#include "app/cpp/init.hpp"
#include "boards/sim/runtime.hpp"

#include <cstdio>

namespace {
int checks = 0;
int failures = 0;

void expect(bool value, const char* name) {
  ++checks;
  if (!value) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", name);
  }
}

std::uint32_t next(std::uint32_t& seed) {
  seed = seed * 1664525U + 1013904223U;
  return seed;
}
}  // namespace

int main() {
  board::sim::runtime board{};
  board::sim::init(board);
  if (!board::sim::mount(board, "tests/fixtures/library")) return 2;
  state::store memory{}, persistent{};
  app::context application{};
  if (!app::init(application, board.capabilities, memory, persistent)) return 3;

  board::sim::advance(board, 1600);
  app::pump(application);
  std::uint32_t seed = 0x51a7c0deU;
  constexpr event::key_code keys[] = {
      event::key_code::up, event::key_code::down, event::key_code::left,
      event::key_code::right, event::key_code::ok, event::key_code::back,
      event::key_code::menu,
  };

  for (int step = 0; step < 1000; ++step) {
    const auto value = next(seed);
    if (value % 5 == 0) {
      board::sim::touch(board, static_cast<int>(value % board::sim::width),
                        static_cast<int>((value >> 8) % board::sim::height));
    } else {
      const auto key = keys[value % (sizeof(keys) / sizeof(keys[0]))];
      const bool long_press = value % 17 == 0;
      board::sim::inject(board, event::key(key, long_press ? 900 : 0, long_press));
    }
    board::sim::advance(board, 10);
    app::pump(application);

    const auto current = app::current_page(application);
    expect(static_cast<std::int64_t>(current) >= static_cast<std::int64_t>(app::page::splash) &&
               static_cast<std::int64_t>(current) <= static_cast<std::int64_t>(app::page::sleep),
           "monkey route remains valid");
    expect(application.reader.library.count <= application.reader.library.books.size(),
           "monkey library count remains bounded");
    expect(application.file_count >= 0 &&
               static_cast<std::size_t>(application.file_count) <= application.files.size(),
           "monkey file count remains bounded");
    expect(application.file_offset >= 0 && application.file_offset <= application.file_count,
           "monkey file offset remains bounded");
  }

  std::printf("monkey tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
