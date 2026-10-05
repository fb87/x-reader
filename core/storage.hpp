#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief Generic, path-addressed filesystem/storage capability.
 *
 * Ported from `xr_storage_t` (include/xr/xr_storage.h), extended with a
 * directory `list()` so reader/library code never touches `FILE*`/`DIR*`/
 * FatFS directly (the old `app_scan_library`/file-manager code called
 * FatFS `f_opendir`/`f_readdir` inline; the new reader/library.hpp must go
 * through `storage::list` exclusively). The simulator maps this to
 * host/in-memory storage; M5Paper maps it to SD/FatFS; reader code is
 * unchanged either way.
 *
 * `inflate` deliberately keeps the OLD contract's *streaming* shape
 * (`path` + `source_offset` + `source_size`, not an in-memory buffer)
 * rather than requiring the whole compressed entry to be buffered first.
 * This preserves real, hardware-motivated behavior: `xr_storage_inflate_raw_fn`'s
 * doc comment requires producing exactly `destination_size` bytes or
 * failing, which is how the EPUB parser detects truncated/corrupt ZIP
 * entries, and an ESP32-class board cannot assume a whole compressed
 * chapter fits in memory at once.
 */
namespace storage {

/** @brief Maximum portable path length used by the reader-facing storage API. */
inline constexpr std::size_t path_max = 256;

/** @brief Directory entry returned by a storage implementation. */
struct entry {
  const char* name = nullptr;
  bool directory = false;
  std::uint32_t size = 0;
};

/** @brief Callback invoked for each directory entry; return false to stop early. */
using entry_fn = bool (*)(const entry& value, void* user);

/** @brief Generic filesystem/storage capability implemented by a board. */
struct device {
  void* context = nullptr;
  const char* root = nullptr;
  bool (*list)(device& self, const char* path, entry_fn callback, void* user) = nullptr;
  bool (*read)(device& self, const char* path, std::uint32_t offset, void* destination,
               std::uint32_t size) = nullptr;
  bool (*file_size)(device& self, const char* path, std::uint32_t& out) = nullptr;
  /** Inflates a raw RFC 1951 DEFLATE stream of `source_size` bytes starting at
   * `source_offset` within `path`, streaming it through the board's own bounded
   * staging buffer. Must fail if the stream does not produce exactly
   * `destination_size` bytes. */
  bool (*inflate)(device& self, const char* path, std::uint32_t source_offset,
                  std::uint32_t source_size, void* destination,
                  std::uint32_t destination_size) = nullptr;
};

/** @brief Enumerates a directory through the board-provided storage implementation. */
inline bool list(device& self, const char* path, entry_fn callback, void* user) {
  return self.list != nullptr && self.list(self, path, callback, user);
}

/** @brief Reads a byte range from a file. */
inline bool read(device& self, const char* path, std::uint32_t offset, void* destination,
                 std::uint32_t size) {
  return self.read != nullptr && self.read(self, path, offset, destination, size);
}

/** @brief Queries a file's size in bytes. */
inline bool file_size(device& self, const char* path, std::uint32_t& out) {
  return self.file_size != nullptr && self.file_size(self, path, out);
}

/** @brief Expands a raw DEFLATE byte range from `path` when the board supports it. */
inline bool inflate(device& self, const char* path, std::uint32_t source_offset,
                    std::uint32_t source_size, void* destination,
                    std::uint32_t destination_size) {
  return self.inflate != nullptr &&
         self.inflate(self, path, source_offset, source_size, destination, destination_size);
}

}  // namespace storage
