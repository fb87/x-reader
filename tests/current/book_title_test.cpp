#include <cstdio>
#include <cstring>

#include "reader/book.hpp"

/**
 * @brief Permanent regression test for reader::book::make_title's Vietnamese
 * mojibake/NFD repair, ported from the fixtures in the old tests/book_title_test.c.
 * Written during Phase 15 (old-tree retirement) after noticing the Phase 5
 * differential verification of this logic was only ever run from a throwaway
 * script -- retiring the old .c test without this would have been a real
 * regression in checked-in coverage, not just a file move.
 */
namespace {
int failed = 0;

int check(const char* input, const char* expected) {
  char output[128];
  book::make_title(output, sizeof(output), input);
  if (std::strcmp(output, expected) == 0) return 0;
  std::fprintf(stderr, "title mismatch: got '%s', expected '%s'\n", output, expected);
  return 1;
}
}  // namespace

int main() {
  failed |= check(
      "K\xc3\xa1\xc2\xbb\xe2\x80\xb9"
      "ch.epub",
      "K\xe1\xbb\x8b"
      "ch");
  failed |= check(
      "K\xc3\xa1\xc2\xbb\xc2\x8b"
      "ch.epub",
      "K\xe1\xbb\x8b"
      "ch");
  failed |= check("Ti\xe1\xba\xbfng Vi\xe1\xbb\x87t.epub", "Ti\xe1\xba\xbfng Vi\xe1\xbb\x87t");
  failed |= check("Cafe\xcc\x81.epu", "Cafe\xcc\x81");
  if (failed == 0) std::printf("book title tests: passed\n");
  return failed;
}
