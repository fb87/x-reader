#include "boards/sim/xteink.hpp"

#include <cstdio>
#include <cstring>

#include "app/cpp/init.hpp"

/**
 * @brief Exercises the app under a second logical profile (480x800 MONO1,
 * not M5Paper's 540x960 GRAY4) to catch accidental width/pixel-format
 * assumptions, per docs/DESIGN.md's "stays simulator-only profile for
 * now" plan for Xteink. A reduced flow, not the full 47-check suite:
 * splash -> home -> library -> open the real fixture EPUB -> paginate
 * and page-turn at the narrower width -- the parts most likely to
 * assume 540px/4bpp if something regresses.
 */
namespace test {
int checks = 0;
int failures = 0;
inline void expect(bool condition, const char* message) {
  ++checks;
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", message);
  }
}

inline void key(board::sim::runtime& board, app::context& application, event::key_code code) {
  board::sim::inject(board, event::key(code));
  app::pump(application);
}
}  // namespace test

int main() {
  board::sim::runtime sim{};
  board::sim_xteink::init(sim);
  board::sim::mount(sim, "tests/fixtures/library");

  test::expect(sim.display.width == 480 && sim.display.height == 800,
               "display profile is actually 480x800, not the M5Paper default");
  test::expect(sim.display.format == display::pixel_format::mono1,
               "display profile is actually MONO1, not GRAY4");

  app::context application{};
  state::store memory{};
  state::store persistent{};

  test::expect(app::init(application, sim.capabilities, memory, persistent),
               "app::init succeeds on the narrower MONO1 profile");
  board::sim::advance(sim, 1600);
  app::pump(application);
  test::expect(app::current_page(application) == app::page::home,
               "splash still times out to home at this geometry");
  test::expect(application.reader.library.count == 2,
               "library scan is unaffected by display profile");

  // Open the real fixture EPUB and paginate at the narrower 480px width.
  state::set(memory, "app.home.card.focused", false);
  test::key(sim, application, event::key_code::ok);
  test::expect(app::current_page(application) == app::page::library, "library pushed correctly");
  test::key(sim, application, event::key_code::ok);
  test::key(sim, application, event::key_code::ok);
  test::expect(app::current_page(application) == app::page::reader, "reader pushed correctly");
  test::expect(application.reader.current.epub_open, "the real EPUB opened at this geometry too");
  const auto text_rect = app::pages::reader::text_rect(application);
  test::expect(text_rect.w <= 480 - 2 * 28,
               "the text rect is clipped to the narrower 480px width, not 540px");
  test::expect(application.reader.current.page_count > 0, "pagination produced at least one page");

  const int page_before = application.reader.current.page;
  const int chapter_before = application.reader.current.current_chapter;
  board::sim::touch(sim, sim.display.width - 5, 200);
  app::pump(application);
  test::expect(application.reader.current.page != page_before ||
                   application.reader.current.current_chapter != chapter_before,
               "touch page-turn still works at the narrower width/MONO1 format");

  std::printf("xteink profile tests: %d checks, %d failures\n", test::checks, test::failures);
  return test::failures == 0 ? 0 : 1;
}
