#pragma once

#include "core/storage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace epub {

inline constexpr std::size_t path_max = 256;
inline constexpr std::size_t title_max = 128;
inline constexpr std::size_t manifest_max = 128;
inline constexpr std::size_t spine_max = 128;

/** @brief EPUB parsing status. */
enum class status {
  ok,
  argument,
  io,
  format,
  unsupported,
  not_found,
  buffer_too_small,
  capacity,
};

/** @brief One manifest entry required to resolve the EPUB spine. */
struct manifest_item {
  std::array<char, 64> id{};
  std::array<char, path_max> href{};
};

/** @brief Bounded EPUB document metadata and spine. */
struct document {
  storage::device* storage = nullptr;
  std::array<char, path_max> file{};
  std::array<char, path_max> package_path{};
  std::array<char, title_max> title{};
  std::array<manifest_item, manifest_max> manifest{};
  std::array<std::uint16_t, spine_max> spine{};
  std::size_t manifest_count = 0;
  std::size_t spine_count = 0;
};

/** @brief Reads one little-endian 16-bit integer. */
inline std::uint16_t u16(const std::uint8_t* data) {
  return static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8U));
}

/** @brief Reads one little-endian 32-bit integer. */
inline std::uint32_t u32(const std::uint8_t* data) {
  return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8U) |
         (static_cast<std::uint32_t>(data[2]) << 16U) |
         (static_cast<std::uint32_t>(data[3]) << 24U);
}

/** @brief ZIP central-directory metadata needed to extract an entry. */
struct zip_entry {
  std::uint16_t method = 0;
  std::uint32_t compressed_size = 0;
  std::uint32_t uncompressed_size = 0;
  std::uint32_t local_offset = 0;
};

/** @brief Locates a ZIP entry by path using the central directory. */
inline status find_zip_entry(document& self, const char* name, zip_entry& out) {
  if (self.storage == nullptr || name == nullptr) return status::argument;
  std::uint32_t file_size = 0;
  if (!storage::size(*self.storage, self.file.data(), file_size) || file_size < 22) return status::io;
  const std::uint32_t tail_size = file_size > 65557U ? 65557U : file_size;
  std::array<std::uint8_t, 65557> tail{};
  if (!storage::read(*self.storage, self.file.data(), file_size - tail_size, tail.data(), tail_size))
    return status::io;
  int eocd = -1;
  for (int i = static_cast<int>(tail_size) - 22; i >= 0; --i) {
    if (u32(tail.data() + i) == 0x06054b50U) {
      eocd = i;
      break;
    }
  }
  if (eocd < 0) return status::format;
  const std::uint16_t entries = u16(tail.data() + eocd + 10);
  std::uint32_t offset = u32(tail.data() + eocd + 16);
  for (std::uint16_t index = 0; index < entries; ++index) {
    std::uint8_t header[46]{};
    if (!storage::read(*self.storage, self.file.data(), offset, header, sizeof(header))) return status::io;
    if (u32(header) != 0x02014b50U) return status::format;
    const std::uint16_t name_length = u16(header + 28);
    const std::uint16_t extra_length = u16(header + 30);
    const std::uint16_t comment_length = u16(header + 32);
    if (name_length >= path_max) return status::format;
    std::array<char, path_max> entry_name{};
    if (!storage::read(*self.storage, self.file.data(), offset + 46, entry_name.data(), name_length))
      return status::io;
    entry_name[name_length] = '\0';
    if (std::strcmp(entry_name.data(), name) == 0) {
      out.method = u16(header + 10);
      out.compressed_size = u32(header + 20);
      out.uncompressed_size = u32(header + 24);
      out.local_offset = u32(header + 42);
      return status::ok;
    }
    offset += 46U + name_length + extra_length + comment_length;
  }
  return status::not_found;
}

/** @brief Extracts one ZIP entry into caller-owned memory. */
inline status extract(document& self, const char* name, void* destination,
                      std::uint32_t destination_size, std::uint32_t* extracted = nullptr) {
  zip_entry entry{};
  const auto found = find_zip_entry(self, name, entry);
  if (found != status::ok) return found;
  if (entry.uncompressed_size > destination_size) return status::buffer_too_small;
  std::uint8_t local[30]{};
  if (!storage::read(*self.storage, self.file.data(), entry.local_offset, local, sizeof(local)))
    return status::io;
  if (u32(local) != 0x04034b50U) return status::format;
  const std::uint32_t data_offset = entry.local_offset + 30U + u16(local + 26) + u16(local + 28);
  if (entry.method == 0) {
    if (!storage::read(*self.storage, self.file.data(), data_offset, destination, entry.uncompressed_size))
      return status::io;
  } else if (entry.method == 8) {
    if (entry.compressed_size > 128U * 1024U) return status::capacity;
    static std::array<std::uint8_t, 128U * 1024U> compressed{};
    if (!storage::read(*self.storage, self.file.data(), data_offset, compressed.data(), entry.compressed_size))
      return status::io;
    if (!storage::inflate(*self.storage, compressed.data(), entry.compressed_size, destination,
                          entry.uncompressed_size))
      return status::unsupported;
  } else {
    return status::unsupported;
  }
  if (extracted != nullptr) *extracted = entry.uncompressed_size;
  return status::ok;
}

/** @brief Copies an XML attribute value from a tag beginning at start. */
inline bool attribute(const char* start, const char* attribute_name, char* destination,
                      std::size_t capacity) {
  if (start == nullptr || attribute_name == nullptr || capacity == 0) return false;
  const char* end = std::strchr(start, '>');
  if (end == nullptr) return false;
  const char* found = std::strstr(start, attribute_name);
  if (found == nullptr || found >= end) return false;
  found += std::strlen(attribute_name);
  while (found < end && (*found == ' ' || *found == '=')) ++found;
  if (found >= end || (*found != '\'' && *found != '"')) return false;
  const char quote = *found++;
  const char* close = std::strchr(found, quote);
  if (close == nullptr || close > end || static_cast<std::size_t>(close - found) >= capacity) return false;
  std::memcpy(destination, found, static_cast<std::size_t>(close - found));
  destination[close - found] = '\0';
  return true;
}

/** @brief Resolves a relative EPUB package path. */
inline void resolve(const char* base, const char* relative, char* output, std::size_t capacity) {
  const char* slash = std::strrchr(base, '/');
  if (slash == nullptr) {
    std::snprintf(output, capacity, "%s", relative);
  } else {
    const std::size_t prefix = static_cast<std::size_t>(slash - base + 1);
    std::snprintf(output, capacity, "%.*s%s", static_cast<int>(prefix), base, relative);
  }
}

/** @brief Parses container.xml and package metadata using bounded caller-owned storage. */
inline status open(document& self, storage::device& source, const char* path) {
  self = {};
  self.storage = &source;
  if (path == nullptr || std::strlen(path) >= self.file.size()) return status::argument;
  std::snprintf(self.file.data(), self.file.size(), "%s", path);

  std::array<char, 32U * 1024U> scratch{};
  std::uint32_t size = 0;
  auto result = extract(self, "META-INF/container.xml", scratch.data(), scratch.size() - 1, &size);
  if (result != status::ok) return result;
  scratch[size] = '\0';
  const char* rootfile = std::strstr(scratch.data(), "<rootfile ");
  if (rootfile == nullptr || !attribute(rootfile, "full-path", self.package_path.data(), self.package_path.size()))
    return status::format;

  result = extract(self, self.package_path.data(), scratch.data(), scratch.size() - 1, &size);
  if (result != status::ok) return result;
  scratch[size] = '\0';
  const char* title_start = std::strstr(scratch.data(), "<dc:title");
  if (title_start != nullptr) {
    title_start = std::strchr(title_start, '>');
    const char* title_end = title_start == nullptr ? nullptr : std::strstr(title_start + 1, "</dc:title>");
    if (title_start != nullptr && title_end != nullptr) {
      const std::size_t count = static_cast<std::size_t>(title_end - title_start - 1);
      std::snprintf(self.title.data(), self.title.size(), "%.*s", static_cast<int>(count), title_start + 1);
    }
  }

  const char* cursor = scratch.data();
  while ((cursor = std::strstr(cursor, "<item ")) != nullptr) {
    if (self.manifest_count >= self.manifest.size()) return status::capacity;
    auto& item = self.manifest[self.manifest_count];
    if (attribute(cursor, "id", item.id.data(), item.id.size()) &&
        attribute(cursor, "href", item.href.data(), item.href.size())) ++self.manifest_count;
    ++cursor;
  }
  cursor = scratch.data();
  while ((cursor = std::strstr(cursor, "<itemref ")) != nullptr) {
    if (self.spine_count >= self.spine.size()) return status::capacity;
    char idref[64]{};
    if (attribute(cursor, "idref", idref, sizeof(idref))) {
      for (std::size_t i = 0; i < self.manifest_count; ++i) {
        if (std::strcmp(self.manifest[i].id.data(), idref) == 0) {
          self.spine[self.spine_count++] = static_cast<std::uint16_t>(i);
          break;
        }
      }
    }
    ++cursor;
  }
  return self.spine_count == 0 ? status::format : status::ok;
}

/** @brief Extracts one spine XHTML document and strips tags into plain text. */
inline status spine_text(document& self, std::size_t spine_index, char* output,
                         std::size_t capacity) {
  if (spine_index >= self.spine_count || capacity == 0) return status::argument;
  char entry_path[path_max]{};
  resolve(self.package_path.data(), self.manifest[self.spine[spine_index]].href.data(), entry_path,
          sizeof(entry_path));
  static std::array<char, 64U * 1024U> source{};
  std::uint32_t size = 0;
  const auto result = extract(self, entry_path, source.data(), source.size() - 1, &size);
  if (result != status::ok) return result;
  source[size] = '\0';
  std::size_t out = 0;
  bool tag = false;
  for (std::size_t i = 0; i < size && out + 1 < capacity; ++i) {
    const char ch = source[i];
    if (ch == '<') {
      tag = true;
      if (out > 0 && output[out - 1] != '\n') output[out++] = '\n';
    } else if (ch == '>') {
      tag = false;
    } else if (!tag) {
      output[out++] = ch;
    }
  }
  output[out] = '\0';
  return status::ok;
}

}  // namespace epub
