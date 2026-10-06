#include "boards/sim/runtime.hpp"
#include "reader/epub.hpp"

#include <cstdio>
#include <cstring>

/**
 * @brief Real-fixture EPUB regression test, matching the old
 * tests/epub_import_test.c in spirit: open the real sample EPUB through a
 * real board storage capability (board::sim, dogfooding the same runtime
 * the app and tests/simulator_test.cpp use) and verify real manifest/spine
 * counts and non-empty chapter text -- not a synthetic fixture.
 */
int main(int argc, char** argv) {
  const char* path = argc == 2 ? argv[1] : "/home/dao/data/sample.epub";

  board::sim::runtime sim{};
  board::sim::init(sim);

  std::uint32_t size = 0;
  if (!storage::file_size(sim.storage, path, size)) {
    std::fprintf(stderr, "cannot stat %s\n", path);
    return 1;
  }
  epub::file_view view{&sim.storage, path, size};

  static epub::context doc;
  static std::array<epub::manifest_item, 640> manifest;
  static std::array<epub::spine_item, 640> spine;
  static std::array<std::uint8_t, 80 * 1024> scratch;
  static std::array<char, 64 * 1024> text;

  epub::status status =
      epub::open(doc, view, scratch.data(), scratch.size(), manifest.data(),
                static_cast<std::uint16_t>(manifest.size()), spine.data(),
                static_cast<std::uint16_t>(spine.size()));
  if (status != epub::status::ok) {
    std::fprintf(stderr, "EPUB open failed: %s\n", epub::status_string(status));
    return 1;
  }

  std::uint32_t text_size = 0;
  status = epub::spine_text(doc, 1, scratch.data(), scratch.size(), text.data(), text.size(),
                            text_size);
  if (status != epub::status::ok || doc.manifest_count != 602 || doc.spine_count != 596 ||
      text[0] == '\0') {
    std::fprintf(stderr, "EPUB import failed: %s (manifest=%u spine=%u text[0]=%u)\n",
                 epub::status_string(status), doc.manifest_count, doc.spine_count,
                 static_cast<unsigned>(static_cast<std::uint8_t>(text[0])));
    return 1;
  }
  std::printf("epub tests: passed (manifest=%u spine=%u)\n", doc.manifest_count, doc.spine_count);
  return 0;
}
