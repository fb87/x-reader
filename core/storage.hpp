#pragma once

#include <cstddef>
#include <cstdint>

namespace storage {

/** @brief Maximum portable path length used by the reader-facing storage API. */
inline constexpr std::size_t path_max = 256;

/** @brief Directory entry returned by a storage implementation. */
struct entry {
    const char* name = nullptr;
    bool directory = false;
    std::uint32_t size = 0;
};

/** @brief Callback invoked for each directory entry. */
using entry_fn = bool (*)(const entry& value, void* user);

/** @brief Generic filesystem/storage capability implemented by a board. */
struct device {
    void* context = nullptr;
    const char* root = nullptr;
    bool (*list)(device& self, const char* path, entry_fn callback, void* user) = nullptr;
    bool (*read)(device& self, const char* path, std::uint32_t offset, void* destination,
                 std::uint32_t size) = nullptr;
    bool (*size)(device& self, const char* path, std::uint32_t& out) = nullptr;
    bool (*inflate)(device& self, const void* source, std::uint32_t source_size,
                    void* destination, std::uint32_t destination_size) = nullptr;
};

/** @brief Enumerates a directory through the board-provided storage implementation. */
inline bool list(device& self, const char* path, entry_fn callback, void* user)
{
    return self.list != nullptr && self.list(self, path, callback, user);
}

/** @brief Reads a byte range from a file. */
inline bool read(device& self, const char* path, std::uint32_t offset, void* destination,
                 std::uint32_t size)
{
    return self.read != nullptr && self.read(self, path, offset, destination, size);
}

/** @brief Queries a file size. */
inline bool size(device& self, const char* path, std::uint32_t& out)
{
    return self.size != nullptr && self.size(self, path, out);
}

/** @brief Expands a raw DEFLATE stream when the board/storage backend supports it. */
inline bool inflate(device& self, const void* source, std::uint32_t source_size, void* destination,
                    std::uint32_t destination_size) {
  return self.inflate != nullptr &&
         self.inflate(self, source, source_size, destination, destination_size);
}

}  // namespace storage
