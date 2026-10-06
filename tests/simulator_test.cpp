#include "app/app.hpp"
#include "boards/sim/runtime.hpp"

#include <cstdio>
#include <cstring>

/**
 * @brief Autonomous input-injection test suite, following docs/DESIGN.md s17/s18:
 * every assertion is reached by injecting simulated events through
 * board::sim::inject/touch -> app::pump -> shell::dispatch -> page/dialog ->
 * reader/domain operation -> state change -> render/display update. No test
 * calls a page's render/event function directly.
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

namespace {

void settle(board::sim::runtime& sim, app::context& app, std::uint32_t ms = 20) {
  board::sim::advance(sim, ms);
  app::pump(app);
}

// Home's focus persists across on_enter (only on_create resets it, matching the old
// app's `if (!p->scope.focus) xr_scope_focus_first(...)`), so returning to Home doesn't
// always land back on the book card. Send more Ups than there are focusable rows to
// deterministically land back on the card (move_focus clamps at the top, it doesn't wrap).
void home_reset_focus(board::sim::runtime& sim, app::context& app) {
  for (int i = 0; i < 8; ++i) board::sim::rotary_left(sim);
  settle(sim, app);
}

}  // namespace

int main() {
  board::sim::runtime sim{};
  board::sim::init(sim);
  board::sim::mount(sim, "tests/fixtures/library");

  app::pages nav{};
  app::context application{};
  state::store memory{};
  state::store persistent{};
  static constexpr shell::theme theme{
      &xr_font_alegreya_14, &xr_font_alegreya_18,      &xr_font_alegreya_bold_18,
      &xr_font_alegreya_bold_26, &xr_font_alegreya_20, 44,
      64,                   16,                        72,
  };

  const bool initialized =
      app::init(application, nav, sim.capabilities, memory, persistent, theme,
               "tests/fixtures/library");
  test::expect(initialized, "app::init should succeed with all mandatory capabilities present");

  // --- secret store: real on the sim board, structurally separate from state ---
  test::expect(capability::has(sim.capabilities, capability::id::secret),
              "sim board registers a real secret-store capability");
  char secret_buf[64] = {0};
  test::expect(!secret::get(sim.secret, "network.wifi.password", secret_buf, sizeof(secret_buf)),
              "an unset secret key reports absent, not a default/empty success");
  test::expect(secret::set(sim.secret, "network.wifi.password", "s3cr3t"),
              "setting a secret succeeds");
  test::expect(secret::get(sim.secret, "network.wifi.password", secret_buf, sizeof(secret_buf)) &&
                  std::strcmp(secret_buf, "s3cr3t") == 0,
              "the secret round-trips exactly");
  test::expect(secret::remove(sim.secret, "network.wifi.password"),
              "removing an existing secret succeeds");
  test::expect(!secret::get(sim.secret, "network.wifi.password", secret_buf, sizeof(secret_buf)),
              "the secret is gone after removal");

  // --- startup/splash --------------------------------------------------------
  test::expect(shell::top(application.shell) == &nav.splash.base, "splash is the initial page");
  test::expect(nav.splash.base.chrome == page::chrome::none, "splash has no chrome");

  // scan_library is deliberately separate from init() (see its doc comment): the
  // real composition root shows the splash before walking storage.
  app::scan_library(application);

  settle(sim, application, 100);
  test::expect(shell::top(application.shell) == &nav.splash.base,
              "splash should not time out before 1500ms");
  settle(sim, application, 1500);
  test::expect(shell::top(application.shell) == &nav.home.base, "splash times out to home");

  // --- library discovery -------------------------------------------------------
  test::expect(application.library.count == 2, "scan discovers both fixture books");

  // --- home navigation: Down from the book card reaches the Library button ----
  test::expect(application.shell.pages[0]->scope.focus == &nav.home.card.base_widget,
              "home starts focused on the book card");
  board::sim::rotary_right(sim);  // Down
  settle(sim, application);
  test::expect(application.shell.pages[0]->scope.focus == &nav.home.library.base_widget,
              "Down moves focus from the card to the Library button");
  board::sim::rotary_push(sim);  // OK
  settle(sim, application);
  test::expect(shell::top(application.shell) == &nav.library.base, "OK on Library pushes it");
  test::expect(nav.library.list.count == 2, "library list shows both fixture books");

  // --- open a book via Book Info, exercising the REAL EPUB pipeline -----------
  board::sim::rotary_push(sim);  // OK on the first (focused) row -> show_book_info.
  settle(sim, application);
  test::expect(application.shell.dialog_count == 1, "selecting a row shows the book-info dialog");
  test::expect(nav.book_info.base.scope.focus == &nav.book_info.read.base_widget,
              "book info starts focused on Read");
  board::sim::rotary_push(sim);  // OK -> Read -> open_book.
  settle(sim, application);
  test::expect(application.shell.dialog_count == 0, "book info closes after Read");
  test::expect(shell::top(application.shell) == &nav.reader.base, "Read pushes the reader");
  test::expect(application.session.epub_open, "reader session actually opened the EPUB");
  test::expect(application.session.doc.manifest_count == 602,
              "real manifest count from the fixture EPUB (not a placeholder)");
  test::expect(application.session.doc.spine_count == 596,
              "real spine count from the fixture EPUB (not a placeholder)");
  test::expect(std::strcmp(reader::current_text(application.session),
                           reader::sample_text()) != 0,
              "reader shows real chapter text, never the Pride and Prejudice fallback, "
              "while a real EPUB is open");

  // --- reader: touch page turns and chrome toggle -----------------------------
  const int page_before = application.session.page;
  board::sim::touch(sim, nav.reader.base.area.x + nav.reader.base.area.w - 10,
                    nav.reader.base.area.y + 10);  // right third -> next page.
  settle(sim, application);
  test::expect(application.session.page != page_before || application.session.current_chapter != 0,
              "tapping the right third turns the page (or crosses a chapter)");

  // Keep tapping until a chapter boundary is actually crossed (the first tap above may
  // just advance within a long chapter 0) -- this is the only way to exercise the
  // chapter-crossing branch of turn_page end to end, through the real input path.
  const int chapter_before_crossing = application.session.current_chapter;
  bool crossed_chapter = false;
  for (int i = 0; i < 500 && !crossed_chapter; ++i) {
    board::sim::touch(sim, nav.reader.base.area.x + nav.reader.base.area.w - 10,
                      nav.reader.base.area.y + 10);
    settle(sim, application);
    crossed_chapter = application.session.current_chapter != chapter_before_crossing;
  }
  test::expect(crossed_chapter, "repeated right-third taps eventually cross a chapter boundary");
  test::expect(application.session.total_pages == application.session.page_count,
              "total_pages is refreshed immediately on a chapter-crossing page turn, not "
              "left stale for one render (fixed tech debt inherited from the old app's turn())");
  test::expect(nav.reader.base.chrome == page::chrome::none, "reader chrome still hidden");
  board::sim::touch(sim, nav.reader.base.area.x + nav.reader.base.area.w / 2,
                    nav.reader.base.area.y + 10);  // center third -> toggle chrome.
  settle(sim, application);
  test::expect(nav.reader.base.chrome == page::chrome::all, "center tap reveals reader chrome");

  // --- back navigation: reader -> library -> home -----------------------------
  board::sim::inject(sim, event::key(event::key_code::back));
  settle(sim, application);
  test::expect(nav.reader.base.chrome == page::chrome::none,
              "back with chrome visible hides it first, instead of popping");
  board::sim::inject(sim, event::key(event::key_code::back));
  settle(sim, application);
  test::expect(shell::top(application.shell) == &nav.library.base,
              "back with no chrome pops to the library");

  // --- favorites: mark a book favorite from the library dock action -----------
  // The list has 2 rows starting at selected=0: it takes `count` Downs to leave the
  // list into the dock (count-1 to walk the rows, +1 more for list_event to return
  // false and fall through to shell's dock-focus-entry), landing on dock index 0.
  // Walking the rows this way lands selection on the LAST row (book_index[1]), not
  // row 0 -- so that's the book that ends up favorited, not library.books[0].
  board::sim::inject(sim, event::key(event::key_code::down));
  board::sim::inject(sim, event::key(event::key_code::down));
  settle(sim, application);
  const int favorited_book = nav.library.book_index[nav.library.list.selected];
  board::sim::rotary_push(sim);  // Favorite is dock action 0.
  settle(sim, application);
  test::expect(application.library.books[favorited_book].favorite,
              "dock Favorite action marks the selected book");

  board::sim::inject(sim, event::key(event::key_code::back));
  settle(sim, application);
  test::expect(shell::top(application.shell) == &nav.home.base, "back from library reaches home");

  // --- favorites page shows exactly the favorited book ------------------------
  home_reset_focus(sim, application);
  board::sim::rotary_right(sim);  // card -> Library.
  board::sim::rotary_right(sim);  // Library -> Favorites.
  settle(sim, application);
  board::sim::rotary_push(sim);
  settle(sim, application);
  test::expect(shell::top(application.shell) == &nav.favorites.base, "Favorites button pushes it");
  test::expect(nav.favorites.list.count == 1, "favorites shows exactly the one favorited book");
  board::sim::inject(sim, event::key(event::key_code::back));
  settle(sim, application);

  // --- settings: real state mutation, not a no-op ----------------------------
  home_reset_focus(sim, application);
  board::sim::rotary_right(sim);  // card -> Library.
  board::sim::rotary_right(sim);  // Library -> Favorites.
  board::sim::rotary_right(sim);  // Favorites -> File Manager.
  board::sim::rotary_right(sim);  // File Manager -> Settings.
  settle(sim, application);
  board::sim::rotary_push(sim);
  settle(sim, application);
  test::expect(shell::top(application.shell) == &nav.settings.base, "Settings button pushes it");
  test::expect(nav.settings.list.count == app::row_count, "settings lists all seven rows");

  const bool wifi_before = state::get(memory, app::key::wifi_connected, false);
  board::sim::rotary_push(sim);  // OK on row 0 (Wi-Fi).
  settle(sim, application);
  test::expect(state::get(memory, app::key::wifi_connected, false) != wifi_before,
              "selecting the Wi-Fi row flips network.wifi.connected");

  board::sim::inject(sim, event::key(event::key_code::down));  // -> Bluetooth.
  board::sim::inject(sim, event::key(event::key_code::down));  // -> Font size.
  settle(sim, application);
  const std::int64_t font_before = state::get(memory, app::key::font_size, std::int64_t{1});
  board::sim::rotary_push(sim);
  settle(sim, application);
  test::expect(state::get(memory, app::key::font_size, std::int64_t{1}) ==
                  (font_before + 1) % 3,
              "selecting the Font size row cycles reader.settings.font_size");

  for (int i = 0; i < 4; ++i) board::sim::inject(sim, event::key(event::key_code::down));
  settle(sim, application);  // row 6: About.
  board::sim::rotary_push(sim);
  settle(sim, application);
  test::expect(application.shell.dialog_count == 1, "About row shows a confirm dialog");
  test::expect(nav.about_confirm.base.scope.focus == &nav.about_confirm.no.base_widget,
              "confirm dialogs default focus to No -- a stray press must not be destructive");
  board::sim::inject(sim, event::key(event::key_code::back));
  settle(sim, application);
  test::expect(application.shell.dialog_count == 0, "Back cancels the About dialog");

  board::sim::inject(sim, event::key(event::key_code::back));
  settle(sim, application);
  test::expect(shell::top(application.shell) == &nav.home.base, "back from settings reaches home");

  // --- delete confirm: Yes actually removes the book --------------------------
  home_reset_focus(sim, application);
  board::sim::rotary_right(sim);  // card -> Library.
  settle(sim, application);
  board::sim::rotary_push(sim);
  settle(sim, application);
  const int count_before_delete = application.library.count;
  // Same two-rows-starting-at-0 list as the favorites test: 2 Downs to leave the list
  // and land on dock index 0 (Favorite), one more to reach index 1 (Delete).
  board::sim::inject(sim, event::key(event::key_code::down));
  board::sim::inject(sim, event::key(event::key_code::down));
  board::sim::inject(sim, event::key(event::key_code::down));
  settle(sim, application);
  board::sim::rotary_push(sim);  // Delete dock action -> show_delete_confirm.
  settle(sim, application);
  test::expect(application.shell.dialog_count == 1, "Delete action shows a confirm dialog");
  board::sim::inject(sim, event::key(event::key_code::left));  // No -> Yes.
  settle(sim, application);
  board::sim::rotary_push(sim);
  settle(sim, application);
  test::expect(application.library.count == count_before_delete - 1,
              "confirming delete actually removes the book from the library");
  board::sim::inject(sim, event::key(event::key_code::back));
  settle(sim, application);

  // --- sleep / wake -------------------------------------------------------------
  home_reset_focus(sim, application);
  board::sim::rotary_right(sim);  // card -> Library.
  board::sim::rotary_right(sim);  // Library -> Favorites.
  board::sim::rotary_right(sim);  // Favorites -> File Manager.
  board::sim::rotary_right(sim);  // File Manager -> Settings.
  board::sim::rotary_right(sim);  // Settings -> Sleep.
  settle(sim, application);
  board::sim::rotary_push(sim);
  settle(sim, application);
  test::expect(shell::top(application.shell) == &nav.sleep.base, "Sleep button replaces with sleep");
  board::sim::touch(sim, 10, 10);
  settle(sim, application);
  test::expect(shell::top(application.shell) == &nav.home.base, "any input wakes back to home");

  // --- persistent state round-trip ---------------------------------------------
  app::checkpoint(application);
  state::store restored{};
  app::context restored_app{};
  restored_app.memory = &restored;
  restored_app.persistent = &persistent;
  app::restore_persistent(restored_app);
  test::expect(state::get(restored, app::key::font_size, std::int64_t{-1}) ==
                  state::get(memory, app::key::font_size, std::int64_t{-1}),
              "font size survives a checkpoint/restore round trip");
  test::expect(state::get(restored, app::key::wifi_connected, false) ==
                  state::get(memory, app::key::wifi_connected, false),
              "wifi connected flag survives a checkpoint/restore round trip");

  std::printf("simulator tests: %d checks, %d failures\n", test::checks, test::failures);
  return test::failures == 0 ? 0 : 1;
}
