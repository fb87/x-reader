#include <cstdio>

#include "core/text.hpp"

/**
 * @brief Guards against a font-coverage regression, the specific mistake
 * the architecture prototype this migration is based on made: replacing
 * real bitmap fonts with an ad hoc uppercase-only 5x7 placeholder with no
 * accent/Vietnamese support. core/text.hpp bridges the exact same
 * compiled fonts/xr_font_alegreya_*.c objects the old C app used (not
 * copies -- extern "C" references to the same translation units), so this
 * test is really asserting "whoever builds this tree in the future didn't
 * swap the font generator's output for something smaller."
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

bool has_codepoint(const xr_font_t& f, std::uint32_t codepoint) {
  if (f.codepoints == nullptr) return codepoint >= f.first && codepoint <= f.last;
  for (std::uint16_t i = 0; i < f.glyph_count; ++i) {
    if (f.codepoints[i] == codepoint) return true;
  }
  return false;
}

}  // namespace

int main() {
  const xr_font_t& body = text::xr_font_alegreya_18;

  expect(has_codepoint(body, 'a') && has_codepoint(body, 'z'), "lowercase a-z covered");
  expect(has_codepoint(body, 'A') && has_codepoint(body, 'Z'), "uppercase A-Z covered");

  // Vietnamese precomposed Latin Extended Additional range (U+1EA0-U+1EF9), the exact
  // block the old app's commit "Add EPUB reading and Vietnamese fonts" introduced.
  int vietnamese_covered = 0;
  for (std::uint32_t cp = 0x1EA0; cp <= 0x1EF9; ++cp) {
    if (has_codepoint(body, cp)) ++vietnamese_covered;
  }
  expect(vietnamese_covered == (0x1EF9 - 0x1EA0 + 1),
         "full Vietnamese precomposed Latin Extended Additional block (U+1EA0-U+1EF9) covered");

  // Spot-check a few actual letters used by real Vietnamese text (matching the
  // mojibake fixtures in reader/book.hpp's differential test): "Việt" needs U+1EC7 (ệ),
  // "Nguyễn" needs U+1EC5 (ễ), "Hà Nội" needs U+1EC1 (ề is not in it, but U+1ED9 (ộ) is).
  expect(has_codepoint(body, 0x1EC7), "U+1EC7 (ệ, as in Việt) covered");
  expect(has_codepoint(body, 0x1EC5), "U+1EC5 (ễ, as in Nguyễn) covered");
  expect(has_codepoint(body, 0x1ED9), "U+1ED9 (ộ, as in Nội) covered");

  // Combining marks (U+0300-U+036F): needed for NFD-composed text, e.g. the
  // book_title_test.c fixture "Cafe\xcc\x81" (e + combining acute accent).
  expect(has_codepoint(body, 0x0301), "U+0301 (combining acute accent) covered");

  // The bold/title fonts used for headings must carry the same coverage, not a
  // reduced ASCII-only variant.
  expect(has_codepoint(text::xr_font_alegreya_bold_26, 0x1EC7),
         "bold heading font also covers Vietnamese, not just the body font");

  std::printf("font coverage tests: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
