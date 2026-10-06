#include "core/storage.hpp"
#include "reader/epub.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace test {

/** @brief ZIP entry metadata used to build a deterministic stored EPUB fixture. */
struct fixture_entry {
  const char* name;
  const char* data;
  std::uint32_t offset = 0;
};

/** @brief Writes one little-endian 16-bit integer. */
void write_u16(std::FILE* file, std::uint16_t value) {
  const std::uint8_t bytes[] = {static_cast<std::uint8_t>(value),
                                static_cast<std::uint8_t>(value >> 8U)};
  std::fwrite(bytes, 1, sizeof(bytes), file);
}

/** @brief Writes one little-endian 32-bit integer. */
void write_u32(std::FILE* file, std::uint32_t value) {
  const std::uint8_t bytes[] = {static_cast<std::uint8_t>(value),
                                static_cast<std::uint8_t>(value >> 8U),
                                static_cast<std::uint8_t>(value >> 16U),
                                static_cast<std::uint8_t>(value >> 24U)};
  std::fwrite(bytes, 1, sizeof(bytes), file);
}

/** @brief Builds a tiny uncompressed EPUB ZIP for parser validation. */
bool make_fixture(const char* path) {
  fixture_entry entries[] = {
      {"META-INF/container.xml",
       "<?xml version=\"1.0\"?><container><rootfiles><rootfile full-path=\"OEBPS/content.opf\"/></rootfiles></container>"},
      {"OEBPS/content.opf",
       "<package><metadata><dc:title>Fixture Book</dc:title></metadata><manifest>"
       "<item id=\"chap1\" href=\"chapter1.xhtml\" media-type=\"application/xhtml+xml\"/>"
       "</manifest><spine><itemref idref=\"chap1\"/></spine></package>"},
      {"OEBPS/chapter1.xhtml", "<html><body><h1>Chapter One</h1><p>Hello reader.</p></body></html>"},
  };
  std::FILE* file = std::fopen(path, "wb");
  if (file == nullptr) return false;

  for (auto& entry : entries) {
    entry.offset = static_cast<std::uint32_t>(std::ftell(file));
    const auto name_size = static_cast<std::uint16_t>(std::strlen(entry.name));
    const auto data_size = static_cast<std::uint32_t>(std::strlen(entry.data));
    write_u32(file, 0x04034b50U);
    write_u16(file, 20);
    write_u16(file, 0);
    write_u16(file, 0);
    write_u16(file, 0);
    write_u16(file, 0);
    write_u32(file, 0);
    write_u32(file, data_size);
    write_u32(file, data_size);
    write_u16(file, name_size);
    write_u16(file, 0);
    std::fwrite(entry.name, 1, name_size, file);
    std::fwrite(entry.data, 1, data_size, file);
  }

  const auto central_offset = static_cast<std::uint32_t>(std::ftell(file));
  for (const auto& entry : entries) {
    const auto name_size = static_cast<std::uint16_t>(std::strlen(entry.name));
    const auto data_size = static_cast<std::uint32_t>(std::strlen(entry.data));
    write_u32(file, 0x02014b50U);
    write_u16(file, 20);
    write_u16(file, 20);
    write_u16(file, 0);
    write_u16(file, 0);
    write_u16(file, 0);
    write_u16(file, 0);
    write_u32(file, 0);
    write_u32(file, data_size);
    write_u32(file, data_size);
    write_u16(file, name_size);
    write_u16(file, 0);
    write_u16(file, 0);
    write_u16(file, 0);
    write_u16(file, 0);
    write_u32(file, 0);
    write_u32(file, entry.offset);
    std::fwrite(entry.name, 1, name_size, file);
  }
  const auto central_end = static_cast<std::uint32_t>(std::ftell(file));
  write_u32(file, 0x06054b50U);
  write_u16(file, 0);
  write_u16(file, 0);
  write_u16(file, 3);
  write_u16(file, 3);
  write_u32(file, central_end - central_offset);
  write_u32(file, central_offset);
  write_u16(file, 0);
  return std::fclose(file) == 0;
}

/** @brief Reads bytes from a host file through the generic storage interface. */
bool read(storage::device&, const char* path, std::uint32_t offset, void* destination,
          std::uint32_t size) {
  std::FILE* file = std::fopen(path, "rb");
  if (file == nullptr) return false;
  const bool ok = std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0 &&
                  std::fread(destination, 1, size, file) == size;
  std::fclose(file);
  return ok;
}

/** @brief Queries host-file size through the generic storage interface. */
bool size(storage::device&, const char* path, std::uint32_t& out) {
  std::FILE* file = std::fopen(path, "rb");
  if (file == nullptr || std::fseek(file, 0, SEEK_END) != 0) return false;
  out = static_cast<std::uint32_t>(std::ftell(file));
  std::fclose(file);
  return true;
}

}  // namespace test

/** @brief Validates ZIP/container/OPF/spine/text extraction through generic storage. */
int main() {
  constexpr const char* path = "build/fixture.epub";
  if (!test::make_fixture(path)) return 1;
  storage::device storage{.root = "build", .read = test::read, .size = test::size};
  epub::document document{};
  auto open_status = epub::open(document, storage, path); if (open_status != epub::status::ok) { std::fprintf(stderr, "open status=%d\n", static_cast<int>(open_status)); return 2; }
  if (std::strcmp(document.title.data(), "Fixture Book") != 0 || document.spine_count != 1) return 3;
  char text[512]{};
  if (epub::spine_text(document, 0, text, sizeof(text)) != epub::status::ok) return 4;
  if (std::strstr(text, "Chapter One") == nullptr || std::strstr(text, "Hello reader.") == nullptr)
    return 5;
  std::printf("epub tests: passed\n");
  return 0;
}
