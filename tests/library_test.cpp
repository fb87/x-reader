#include "reader/library.hpp"

#include <cstdio>
#include <cstring>

/**
 * @brief Permanent regression test for reader::library's 17th transient-overflow slot
 * (ported from the old app's `app_open_storage_epub`/`app_delete_book`), exercising the
 * domain module directly -- unlike simulator_test.cpp, this isn't reachable end to end
 * without either 16 real fixture files on disk or a page-level test shortcut, neither of
 * which is worth it for a library-module invariant.
 */
namespace {
int checks = 0;
int failures = 0;

void expect(bool condition, const char* message) {
  ++checks;
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", message);
  }
}

void fill_to_cap(library::index& lib) {
  char path[32];
  for (int i = 0; i < library::max_books; ++i) {
    std::snprintf(path, sizeof(path), "book_%d.epub", i);
    expect(library::add(lib, path, path), "add succeeds while under the regular cap");
  }
  expect(lib.count == library::max_books, "library is at the regular 16-book cap");
}
}  // namespace

int main() {
  // --- add() still no-ops at the regular cap, as before this change ------------------
  {
    library::index lib{};
    fill_to_cap(lib);
    expect(!library::add(lib, "overflow.epub", "Overflow"),
           "add() still refuses once the regular cap is reached");
    expect(lib.count == library::max_books, "add()'s refusal doesn't touch count");
  }

  // --- open_transient opens a book past the cap instead of silently no-op'ing --------
  {
    library::index lib{};
    fill_to_cap(lib);
    const int idx = library::open_transient(lib, "overflow.epub", "Overflow");
    expect(idx == library::max_books, "open_transient places the overflow book in slot 16");
    expect(lib.count == library::max_books + 1, "count grows by one for the transient slot");
    expect(lib.books[idx].transient, "the overflow slot is marked transient");
    expect(std::strcmp(lib.books[idx].path.data(), "overflow.epub") == 0,
           "the overflow slot holds the requested path");

    // --- opening a second, different overflow book reuses slot 16, doesn't grow further
    const int idx2 = library::open_transient(lib, "overflow2.epub", "Overflow 2");
    expect(idx2 == library::max_books, "a second overflow book reuses slot 16");
    expect(lib.count == library::max_books + 1, "count does not grow past 17");
    expect(std::strcmp(lib.books[idx2].path.data(), "overflow2.epub") == 0,
           "slot 16 now holds the second overflow book's path");

    // --- re-opening the same overflow path again reuses the slot without changing count
    const int idx3 = library::open_transient(lib, "overflow2.epub", "Overflow 2");
    expect(idx3 == library::max_books, "re-opening the same overflow path reuses slot 16");
    expect(lib.count == library::max_books + 1, "re-opening doesn't grow count");

    // --- opening a path already in the regular library reuses that regular slot -------
    const int idx4 = library::open_transient(lib, "book_3.epub", "book_3.epub");
    expect(idx4 == 3, "opening an already-scanned regular path reuses its regular index");
    expect(!lib.books[3].transient, "reusing a regular entry doesn't mark it transient");
  }

  // --- delete while under the cap: unaffected, exactly as before this change ---------
  {
    library::index lib{};
    library::add(lib, "a.epub", "A");
    library::add(lib, "b.epub", "B");
    library::remove(lib, 0);
    expect(lib.count == 1, "remove under the cap compacts normally");
    expect(std::strcmp(lib.books[0].path.data(), "b.epub") == 0, "the remaining book shifted down");
  }

  // --- deleting a regular book while a transient overflow is present ------------------
  {
    library::index lib{};
    fill_to_cap(lib);
    const int overflow = library::open_transient(lib, "overflow.epub", "Overflow");
    expect(overflow == library::max_books, "overflow book opened at slot 16");

    library::remove(lib, 5);  // delete a regular book, not the transient one.

    // Two entries disappear: the deleted regular book, and the transient overflow slot
    // that gets evicted alongside it (count 17 -> 15), matching the old app_delete_book.
    expect(lib.count == library::max_books - 1,
           "deleting a regular book while full drops the transient slot too (count 17 -> 15)");
    for (int i = 0; i < lib.count; ++i) {
      expect(!lib.books[i].transient, "no transient entry survives among the regular slots");
    }
    char expected[32];
    // book_5 was removed; book_6..15 shift down to fill 5..14, book_0..4 untouched.
    for (int i = 0; i < 5; ++i) {
      std::snprintf(expected, sizeof(expected), "book_%d.epub", i);
      expect(std::strcmp(lib.books[i].path.data(), expected) == 0,
             "books before the removed index are untouched");
    }
    for (int i = 5; i < lib.count; ++i) {
      std::snprintf(expected, sizeof(expected), "book_%d.epub", i + 1);
      expect(std::strcmp(lib.books[i].path.data(), expected) == 0,
             "books after the removed index shift down by one, skipping the dropped slot");
    }
  }

  // --- deleting the transient book itself leaves the regular books untouched ---------
  {
    library::index lib{};
    fill_to_cap(lib);
    const int overflow = library::open_transient(lib, "overflow.epub", "Overflow");
    library::remove(lib, overflow);
    expect(lib.count == library::max_books,
           "removing the transient slot itself drops exactly one entry");
    for (int i = 0; i < lib.count; ++i) {
      char expected[32];
      std::snprintf(expected, sizeof(expected), "book_%d.epub", i);
      expect(std::strcmp(lib.books[i].path.data(), expected) == 0,
             "all 16 regular books are untouched after removing only the transient slot");
    }
  }

  std::printf("library tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
