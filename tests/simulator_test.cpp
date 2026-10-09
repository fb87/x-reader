#include "app/cpp/init.hpp"
#include "boards/sim/runtime.hpp"
#include "core/state.hpp"

#include <cstdio>
#include <cstring>

namespace test {

/** @brief Assertion accumulator for autonomous simulator validation. */
struct result {
  int checks = 0;
  int failures = 0;
};

/** @brief Records one assertion with a readable failure description. */
inline void expect(result& self, bool condition, const char* message) {
  ++self.checks;
  if (!condition) {
    ++self.failures;
    std::fprintf(stderr, "FAIL: %s\n", message);
  }
}

/** @brief Counts hierarchical state notifications. */
inline void count_notification(const char*, void* user) { ++*static_cast<int*>(user); }

/** @brief Sends one key through the simulator input queue and pumps the application. */
inline void key(board::sim::runtime& board, app::context& application, event::key_code code) {
  board::sim::inject(board, event::key(code));
  app::pump(application);
}

/** @brief Sends one touch event through the simulator input queue. */
inline void tap(board::sim::runtime& board, app::context& application, int x, int y) {
  board::sim::touch(board, x, y);
  app::pump(application);
}

/** @brief Returns to Home through the public input path. */
inline void back_home(board::sim::runtime& board, app::context& application) {
  while (app::current_page(application) != app::page::home) key(board, application, event::key_code::back);
}

}  // namespace test

/** @brief Exercises the migrated reader flows through simulated M5Paper input. */
int main() {
  test::result result{};
  board::sim::runtime board{};
  board::sim::init(board);
  test::expect(result, board::sim::mount(board, "tests/fixtures/library"),
               "simulator mounts the real EPUB fixture directory");
  state::store memory{};
  state::store persistent{};
  app::context application{};

  test::expect(result, app::init(application, board.capabilities, memory, persistent),
               "application initializes against generic simulator capabilities");
  test::expect(result, board.display.width == 540 && board.display.height == 960,
               "simulator clones M5Paper 540x960 logical size");
  test::expect(result, board.display.format == display::pixel_format::gray4,
               "simulator clones M5Paper 4bpp format");
  test::expect(result, capability::has(board.capabilities, capability::id::storage),
               "simulator publishes storage capability");
  test::expect(result, capability::has(board.capabilities, capability::id::wifi),
               "simulator publishes optional Wi-Fi capability");
  test::expect(result, state::get(memory, "reader.library.count", std::int64_t{-1}) == 2,
               "library scan discovers EPUB files and ignores unsupported files");
  test::expect(result, app::current_page(application) == app::page::splash,
               "application starts on splash page");
  test::expect(result, board.refresh_count >= 1, "initial render reaches display backend");

  int notifications = 0;
  test::expect(result,
               state::observe(memory, "reader.book", test::count_notification, &notifications),
               "reader.book prefix observer registers");

  board::sim::advance(board, 1499);
  app::pump(application);
  test::expect(result, app::current_page(application) == app::page::splash,
               "splash remains visible before timeout");
  board::sim::advance(board, 1);
  app::pump(application);
  test::expect(result, app::current_page(application) == app::page::home,
               "splash automatically advances after 1500 ms");

  const auto home_selection_before = state::get(memory, "app.menu.selected", std::int64_t{0});
  board::sim::inject(board, event::key(event::key_code::down, 900, true));
  app::pump(application);
  test::expect(result, state::get(memory, "app.menu.selected", std::int64_t{-1}) == home_selection_before,
               "shell consumes long-press before active page navigation");
  test::expect(result, state::get(memory, "app.input.long_press_duration_ms", std::int64_t{0}) == 900,
               "shell records classified long-press duration");

  // Home -> Library.
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::current_page(application) == app::page::library,
               "home opens library through rotary push");
  test::key(board, application, event::key_code::down);
  test::expect(result, state::get(memory, "reader.library.selected", std::int64_t{-1}) == 1,
               "rotary moves shared library selection");
  state::set(memory, "reader.library.selected", std::int64_t{0});

  // Book info modal is part of the original minimal flow.
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::book_info_visible(application),
               "library activation opens book-info dialog");
  test::expect(result, app::current_page(application) == app::page::library,
               "book-info dialog does not replace library page");
  test::key(board, application, event::key_code::right);
  test::expect(result, state::get(memory, "app.dialog.selected", std::int64_t{-1}) == 1,
               "book-info focus moves to Close");
  test::key(board, application, event::key_code::left);
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::current_page(application) == app::page::reader,
               "Read/Continue opens selected book");
  test::expect(result, state::get(memory, "reader.book.current", std::int64_t{-1}) == 0,
               "reader.book.current stores selected book index");
  test::expect(result, std::strlen(state::get(memory, "reader.book.title", "")) > 0,
               "reader publishes current title in shared state");
  test::expect(result, notifications >= 1, "reader.book observers receive open-position changes");

  // Reader input/chrome.
  const auto page_before_reader = application.reader.current.page;
  const auto chapter_before_reader = application.reader.current.current_chapter;
  test::key(board, application, event::key_code::down);
  test::key(board, application, event::key_code::down);
  test::expect(result, application.reader.current.page != page_before_reader ||
                           application.reader.current.current_chapter != chapter_before_reader,
               "rotary page input advances reading position");
  const auto page_before_left = application.reader.current.page;
  const auto chapter_before_left = application.reader.current.current_chapter;
  test::tap(board, application, 10, 400);
  test::expect(result, application.reader.current.page != page_before_left ||
                           application.reader.current.current_chapter != chapter_before_left,
               "left-side touch moves to previous page");
  const auto page_before_right = application.reader.current.page;
  const auto chapter_before_right = application.reader.current.current_chapter;
  test::tap(board, application, 530, 400);
  test::expect(result, application.reader.current.page != page_before_right ||
                           application.reader.current.current_chapter != chapter_before_right,
               "right-side touch moves to next page");
  test::tap(board, application, 270, 400);
  test::expect(result, state::get(memory, "app.reader.chrome", false),
               "center touch toggles reader chrome");
  test::key(board, application, event::key_code::menu);
  test::expect(result, !state::get(memory, "app.reader.chrome", true),
               "reader MENU toggles chrome like dev/minimal");
  test::key(board, application, event::key_code::menu);
  test::key(board, application, event::key_code::down);
  test::expect(result,
                static_cast<app::focus_area>(state::get(memory, "app.focus.area", std::int64_t{0})) ==
                    app::focus_area::dock,
                "reader Down enters dock when chrome is visible");
  test::key(board, application, event::key_code::down);
  test::key(board, application, event::key_code::down);
  test::expect(result, state::get(memory, "app.dock.selected", std::int64_t{-1}) == 2,
               "reader dock Down traverses all actions");
  test::key(board, application, event::key_code::up);
  test::expect(result, state::get(memory, "app.dock.selected", std::int64_t{-1}) == 1,
               "reader dock Up moves to previous action");
  const auto font_before = state::get(memory, "reader.settings.font_size", std::int64_t{-1});
  test::key(board, application, event::key_code::down);
  test::key(board, application, event::key_code::ok);
  test::expect(result, state::get(memory, "reader.settings.font_size", std::int64_t{-1}) >= font_before,
               "reader A+ dock action adjusts font setting");
  test::key(board, application, event::key_code::back);
  test::expect(result, app::current_page(application) == app::page::library,
               "reader BACK returns library");
  test::key(board, application, event::key_code::back);
  test::expect(result, app::current_page(application) == app::page::home,
               "library BACK returns home");

  // Dock touch hit testing.
  state::set(memory, "app.menu.selected", std::int64_t{0});
  test::key(board, application, event::key_code::ok);
  test::tap(board, application, 450, board::sim::height - 20);
  test::expect(result, app::current_page(application) == app::page::home,
               "library dock touch activates Back");

  // Favorites action from library dock.
  state::set(memory, "app.menu.selected", std::int64_t{0});
  test::key(board, application, event::key_code::ok);
  state::set(memory, "reader.library.selected", std::int64_t{0});
  test::key(board, application, event::key_code::down);
  test::key(board, application, event::key_code::down);  // bottom boundary -> dock
  test::expect(result,
                static_cast<app::focus_area>(state::get(memory, "app.focus.area", std::int64_t{0})) ==
                    app::focus_area::dock,
                "library rotary reaches bottom action bar");
  test::key(board, application, event::key_code::down);
  test::expect(result, state::get(memory, "app.dock.selected", std::int64_t{-1}) == 1,
               "library dock Down advances action selection");
  test::key(board, application, event::key_code::up);
  test::expect(result, state::get(memory, "app.dock.selected", std::int64_t{-1}) == 0,
               "library dock Up returns to first action");
  test::key(board, application, event::key_code::ok);
  test::expect(result, application.reader.library.books[1].favorite,
               "Favorite dock action updates cached book state");
  test::key(board, application, event::key_code::back);

  state::set(memory, "app.menu.selected", std::int64_t{1});
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::current_page(application) == app::page::favorites,
               "home opens favorites page");
  test::expect(result, app::visible_book_count(application) == 1,
               "favorites page filters library cache");
  test::key(board, application, event::key_code::back);

  // File manager remains a storage-backed book-opening path.
  state::set(memory, "app.menu.selected", std::int64_t{2});
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::current_page(application) == app::page::files,
               "home opens file manager");
  state::set(memory, "reader.library.selected", std::int64_t{0});
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::current_page(application) == app::page::reader,
               "file manager opens EPUB directly");
  const auto file_reader_page = application.reader.current.page;
  const auto file_reader_chapter = application.reader.current.current_chapter;
  test::key(board, application, event::key_code::down);
  test::key(board, application, event::key_code::down);
  test::expect(result, application.reader.current.page != file_reader_page ||
                           application.reader.current.current_chapter != file_reader_chapter,
               "File Manager reader responds to Down page turn");
  test::key(board, application, event::key_code::back);
  test::expect(result, app::current_page(application) == app::page::files,
               "reader BACK returns file manager");
  state::set(memory, "app.files.selected", std::int64_t{1});
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::current_page(application) == app::page::files,
               "file manager enters nested folder after reader close");
  test::key(board, application, event::key_code::back);
  test::expect(result, app::current_page(application) == app::page::files,
               "file manager BACK returns parent folder");
  state::set(memory, "app.files.selected", std::int64_t{0});
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::current_page(application) == app::page::reader,
               "file manager reopens EPUB after reader close");
  test::key(board, application, event::key_code::back);
  test::expect(result, app::current_page(application) == app::page::files,
               "reopened reader BACK returns file manager");
  test::key(board, application, event::key_code::back);

  // Settings: all seven original rows are present and mutable.
  state::set(memory, "app.menu.selected", std::int64_t{3});
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::current_page(application) == app::page::settings,
               "home opens settings");
  state::set(memory, "app.menu.selected", std::int64_t{1});
  test::key(board, application, event::key_code::ok);
  test::expect(result, state::get(memory, "network.bluetooth.enabled", false),
               "Bluetooth settings row changes shared state");
  state::set(memory, "app.menu.selected", std::int64_t{2});
  const auto settings_font_before = state::get(memory, "reader.settings.font_size", std::int64_t{0});
  test::key(board, application, event::key_code::ok);
  test::expect(result, state::get(memory, "reader.settings.font_size", std::int64_t{-1}) >= settings_font_before,
               "Font size row cycles reader font");
  state::set(memory, "app.menu.selected", std::int64_t{3});
  const auto refresh_before = state::get(memory, "reader.settings.full_refresh_every", std::int64_t{-1});
  test::key(board, application, event::key_code::ok);
  test::expect(result, state::get(memory, "reader.settings.full_refresh_every", std::int64_t{-1}) != refresh_before,
               "Full refresh row cycles refresh policy");
  state::set(memory, "app.menu.selected", std::int64_t{4});
  const bool progress_before = state::get(memory, "reader.settings.show_progress", true);
  test::key(board, application, event::key_code::ok);
  test::expect(result, state::get(memory, "reader.settings.show_progress", true) != progress_before,
               "Progress bar row toggles setting");
  state::set(memory, "app.menu.selected", std::int64_t{5});
  test::key(board, application, event::key_code::ok);
  test::expect(result, state::get(memory, "reader.settings.sleep_timeout_minutes", std::int64_t{0}) == 30,
               "Sleep timeout row cycles setting");
  state::set(memory, "app.menu.selected", std::int64_t{6});
  test::key(board, application, event::key_code::ok);
  test::expect(result, state::get(memory, "app.dialog.about", false),
               "About row opens modal dialog");
  test::key(board, application, event::key_code::ok);
  test::expect(result, !state::get(memory, "app.dialog.about", true), "About dialog closes on input");

  // Connectivity uses generic capability behind settings Wi-Fi row.
  state::set(memory, "app.menu.selected", std::int64_t{0});
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::current_page(application) == app::page::connectivity,
               "Wi-Fi settings row opens connectivity page");
  test::key(board, application, event::key_code::ok);
  test::expect(result, state::get(memory, "network.wifi.scan.count", std::int64_t{-1}) == 2,
               "connectivity scan uses generic Wi-Fi capability");
  test::key(board, application, event::key_code::down);
  test::key(board, application, event::key_code::ok);
  test::expect(result, state::get(memory, "network.wifi.connected", false),
               "connectivity connects through generic Wi-Fi capability");
  test::expect(result, std::strcmp(state::get(memory, "network.wifi.ssid", ""), "Reader-Lab") == 0,
               "connected SSID is shared in hierarchical state");
  test::key(board, application, event::key_code::back);
  test::key(board, application, event::key_code::back);
  test::expect(result, app::current_page(application) == app::page::home,
               "back navigation returns settings/connectivity to home");
  test::expect(result, state::get(memory, "app.menu.selected", std::int64_t{-1}) == 3,
               "settings back restores Settings focus on Home");

  // Sleep/wake.
  state::set(memory, "app.menu.selected", std::int64_t{4});
  test::key(board, application, event::key_code::ok);
  test::expect(result, app::current_page(application) == app::page::sleep,
               "home enters sleep screen");
  test::tap(board, application, 100, 100);
  test::expect(result, app::current_page(application) == app::page::home,
               "touch wakes sleep screen");

  state::set(memory, "reader.settings.sleep_timeout_minutes", std::int64_t{1});
  board.simulated_ms += 60000U;
  app::pump(application);
  test::expect(result, app::current_page(application) == app::page::sleep,
               "inactivity enters sleep screen after configured timeout");
  test::tap(board, application, 100, 100);
  test::expect(result, app::current_page(application) == app::page::home,
               "input wakes automatic sleep");
  board::sim::inject(board, event::key(event::key_code::ok, 5000, true));
  app::pump(application);
  test::expect(result, app::current_page(application) == app::page::sleep,
               "five-second push enters sleep screen");
  test::tap(board, application, 100, 100);

  // Persistence.
  app::checkpoint(application);
  test::expect(result, state::save(persistent, "build/test_state.db"),
               "persistent state saves to simulator disk backend");
  state::store restored{};
  test::expect(result, state::load(restored, "build/test_state.db"),
               "persistent state reloads from disk");
  test::expect(result,
               state::get(restored, "reader.book.page", std::int64_t{-1}) ==
                       state::get(memory, "reader.book.page", std::int64_t{-2}) &&
                   std::strcmp(state::get(restored, "network.wifi.ssid", ""), "Reader-Lab") == 0,
               "reader position and Wi-Fi metadata survive persistence");

  test::expect(result, board::sim::dump_framebuffer(board, "build/simulator-final.pgm"),
               "simulator dumps framebuffer for visual regression/debugging");
  test::expect(result, board.refresh_count > 20,
               "feature flows produce refreshes through common display path");

  std::printf("simulator tests: %d checks, %d failures\n", result.checks, result.failures);
  return result.failures == 0 ? 0 : 1;
}
