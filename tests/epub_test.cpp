#include "core/storage.hpp"
#include "reader/epub.hpp"
#include "reader/session.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <zlib.h>

namespace test {

inline bool read(storage::device&, const char* path, std::uint32_t offset, void* destination,
                 std::uint32_t size) {
  std::FILE* file = std::fopen(path, "rb");
  if (file == nullptr) return false;
  const bool ok = std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0 &&
                  std::fread(destination, 1, size, file) == size;
  std::fclose(file);
  return ok;
}

inline bool size(storage::device&, const char* path, std::uint32_t& output) {
  std::FILE* file = std::fopen(path, "rb");
  if (file == nullptr || std::fseek(file, 0, SEEK_END) != 0) return false;
  output = static_cast<std::uint32_t>(std::ftell(file));
  std::fclose(file);
  return true;
}

inline bool inflate(storage::device&, const void* source, std::uint32_t source_size,
                    void* destination, std::uint32_t destination_size) {
  z_stream stream{};
  stream.next_in = const_cast<Bytef*>(static_cast<const Bytef*>(source));
  stream.avail_in = source_size;
  stream.next_out = static_cast<Bytef*>(destination);
  stream.avail_out = destination_size;
  if (::inflateInit2(&stream, -MAX_WBITS) != Z_OK) return false;
  const int result = ::inflate(&stream, Z_FINISH);
  ::inflateEnd(&stream);
  return result == Z_STREAM_END && stream.total_out == destination_size;
}

}  // namespace test

int main() {
  constexpr const char* path = "/home/dao/data/sample.epub";
  std::uint32_t file_size = 0;
  storage::device storage{.root = "build", .read = test::read, .size = test::size,
                          .inflate = test::inflate};
  if (!storage::size(storage, path, file_size)) {
    std::fprintf(stderr, "fixture missing: %s\n", path);
    return 1;
  }
  epub::file_view view{&storage, path, file_size};
  epub::context document{};
  std::array<epub::manifest_item, 640> manifest{};
  std::array<epub::spine_item, 640> spine{};
  std::array<std::uint8_t, 80U * 1024U> scratch{};
  std::array<char, 64U * 1024U> text{};
  const auto opened = epub::open(document, view, scratch.data(), scratch.size(), manifest.data(),
                                 static_cast<std::uint16_t>(manifest.size()), spine.data(),
                                 static_cast<std::uint16_t>(spine.size()));
  if (opened != epub::status::ok) {
    std::fprintf(stderr, "EPUB open failed: %s\n", epub::status_string(opened));
    return 2;
  }
  std::uint32_t text_size = 0;
  const auto text_status = epub::spine_text(document, 1, scratch.data(), scratch.size(), text.data(), text.size(), text_size);
  if (text_status != epub::status::ok || text[0] == '\0') {
    std::fprintf(stderr, "EPUB text failed: %s manifest=%u spine=%u text=%u\n",
                 epub::status_string(text_status), document.manifest_count, document.spine_count,
                 static_cast<unsigned>(static_cast<std::uint8_t>(text[0])));
    return 3;
  }
  static reader::session pagination{};
  std::snprintf(pagination.text.data(), pagination.text.size(),
                "A deliberately long paragraph used to verify cached pagination across page turns. "
                "The same layout must reuse the page map while a changed layout invalidates it.");
  pagination.current_chapter = 3;
  const geometry::rect area{28, 28, 484, 840};
  reader::paginate(pagination, area, 3);
  pagination.page_start[0] = 17;
  reader::paginate(pagination, area, 3);
  if (pagination.page_start[0] != 17) {
    std::fprintf(stderr, "pagination cache was recomputed for an unchanged layout\n");
    return 4;
  }
  reader::paginate(pagination, {28, 28, 480, 840}, 3);
  if (pagination.page_start[0] != 0) {
    std::fprintf(stderr, "pagination cache was not invalidated by a layout change\n");
    return 5;
  }
  std::printf("epub tests: passed (manifest=%u spine=%u)\n", document.manifest_count,
              document.spine_count);
  return 0;
}
