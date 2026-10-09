#include "boards/sim/xteink.hpp"

#include <cstdio>
#include <cstring>

#include "app/app.hpp"

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
}  // namespace test

int main() {
  board::sim::runtime sim{};
  board::sim_xteink::init(sim);
  board::sim::mount(sim, "tests/fixtures/library");

  test::expect(sim.display.width == 480 && sim.display.height == 800,
               "display profile is actually 480x800, not the M5Paper default");
  test::expect(sim.display.format == display::pixel_format::mono1,
               "display profile is actually MONO1, not GRAY4");

  app::pages nav{};
  app::context application{};
  state::store memory{};
  state::store persistent{};
  static constexpr shell::theme theme{
      &xr_font_alegreya_14,
      &xr_font_alegreya_18,
      &xr_font_alegreya_bold_18,
      &xr_font_alegreya_bold_26,
      &xr_font_alegreya_20,
      44,
      64,
      16,
      72,
  };

  test::expect(app::init(application, nav, sim.capabilities, memory, persistent, theme,
                         "tests/fixtures/library"),
               "app::init succeeds on the narrower MONO1 profile");
  app::scan_library(application);
  board::sim::advance(sim, 1600);
  app::pump(application);
  test::expect(shell::top(application.shell) == &nav.home.base,
               "splash still times out to home at this geometry");
  test::expect(application.library.count == 2, "library scan is unaffected by display profile");

  // Open the real fixture EPUB and paginate at the narrower 480px width.
  board::sim::rotary_right(sim);
  board::sim::advance(sim, 20);
  app::pump(application);
  board::sim::rotary_push(sim);
  board::sim::advance(sim, 20);
  app::pump(application);
  test::expect(shell::top(application.shell) == &nav.library.base, "library pushed correctly");
  board::sim::rotary_push(sim);
  board::sim::advance(sim, 20);
  app::pump(application);
  board::sim::rotary_push(sim);  // Read.
  board::sim::advance(sim, 20);
  app::pump(application);
  test::expect(shell::top(application.shell) == &nav.reader.base, "reader pushed correctly");
  test::expect(application.session.epub_open, "the real EPUB opened at this geometry too");
  test::expect(nav.reader.text_rect.w <= 480 - 2 * 28,
               "the text rect is clipped to the narrower 480px width, not 540px");
  test::expect(application.session.page_count > 0, "pagination produced at least one page");

  const int page_before = application.session.page;
  const int chapter_before = application.session.current_chapter;
  board::sim::touch(sim, nav.reader.base.area.x + nav.reader.base.area.w - 5,
                    nav.reader.base.area.y + 5);
  board::sim::advance(sim, 20);
  app::pump(application);
  test::expect(application.session.page != page_before ||
                   application.session.current_chapter != chapter_before,
               "touch page-turn still works at the narrower width/MONO1 format");

  std::printf("xteink profile tests: %d checks, %d failures\n", test::checks, test::failures);
  return test::failures == 0 ? 0 : 1;
}
