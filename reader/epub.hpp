#pragma once

#include "../core/storage.hpp"

#include <cstdint>
#include <array>
#include <cstring>

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define EPUB_WORK_BSS EXT_RAM_BSS_ATTR
#else
#define EPUB_WORK_BSS
#endif

/**
 * @brief Bounded EPUB 2/3 container and plain-text extraction, ported
 * function-by-function from `xr_epub_t`/`xr_epub_*` (include/xr/xr_epub.h,
 * src/xr_epub.c). No dynamic allocation; all buffers are caller-owned.
 *
 * This is the highest-risk single file in the whole migration -- NOT
 * because the algorithm is hard to port, but because it is easy to port
 * correctly and then forget to wire it into reader::session (exactly the
 * mistake the architecture prototype this migration is based on made: a
 * fully-working, fully-tested EPUB parser that nothing in the app ever
 * called, so every book showed the same hardcoded placeholder text).
 * reader/session.hpp MUST call into this for real chapter text.
 *
 * `epub::file_view` bridges a layering mismatch: the old `xr_storage_t`
 * modeled a single already-open file's byte-range view (bound to one
 * EPUB's bytes via `context`+`size`), which is a different abstraction
 * level than `core/storage.hpp`'s board-level, path-addressed
 * `storage::device` (used for directory listing). `file_view` is that
 * same bound-to-one-file view, expressed in terms of the new generic
 * storage capability instead of a raw FatFS/FILE* handle.
 */
namespace epub {

inline constexpr std::uint32_t path_max = 256;
inline constexpr std::uint32_t id_max = 64;
inline constexpr std::uint32_t title_max = 128;
inline constexpr std::uint16_t no_item = 0xFFFFu;

/** @brief Outcome of an EPUB operation; never silently succeeds on a short read. */
enum class status {
  ok = 0,
  argument,
  io,
  format,
  unsupported,
  not_found,
  buffer_too_small,
  capacity,
};

/** @brief A human-readable explanation for a status value; never returns nullptr. */
inline const char* status_string(status value) {
  static const char* const names[] = {
      "ok",       "invalid argument",  "storage I/O error",      "invalid EPUB format",
      "unsupported EPUB feature", "archive entry not found", "buffer too small",
      "caller capacity exceeded",
  };
  const auto index = static_cast<unsigned>(value);
  return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown EPUB error";
}

/** @brief A bound-to-one-file byte-range view over a board's storage capability. */
struct file_view {
  storage::device* device = nullptr;
  const char* path = nullptr;
  std::uint32_t size = 0;
};

/** @brief Reads exactly `size` bytes at `offset`; false on any I/O failure. */
inline bool read(const file_view& view, std::uint32_t offset, void* destination,
                 std::uint32_t size) {
  return view.device != nullptr &&
         storage::read(*view.device, view.path, offset, destination, size);
}

/** @brief Inflates a raw DEFLATE range, producing exactly `destination_size` bytes or failing. */
inline bool inflate_raw(const file_view& view, std::uint32_t source_offset,
                        std::uint32_t source_size, void* destination,
                        std::uint32_t destination_size) {
  if (view.device == nullptr || source_size > 128U * 1024U) return false;
  static EPUB_WORK_BSS std::array<std::uint8_t, 128U * 1024U> compressed{};
  if (!read(view, source_offset, compressed.data(), source_size)) return false;
  return storage::inflate(*view.device, compressed.data(), source_size, destination,
                          destination_size);
}

/** @brief Caller-owned manifest entry: an id plus its href, from the OPF `<item>`. */
struct manifest_item {
  char id[id_max] = {0};
  char href[path_max] = {0};
};

/** @brief One reading-order entry, from the OPF `<itemref>`, resolved to a manifest index. */
struct spine_item {
  std::uint16_t manifest_index = 0;
};

/** @brief An opened EPUB: resolved package path/title plus caller-owned manifest/spine arrays. */
struct context {
  const file_view* storage = nullptr;
  char package_path[path_max] = {0};
  char title[title_max] = {0};
  manifest_item* manifest = nullptr;
  std::uint16_t manifest_capacity = 0;
  std::uint16_t manifest_count = 0;
  spine_item* spine = nullptr;
  std::uint16_t spine_capacity = 0;
  std::uint16_t spine_count = 0;
  void* scratch = nullptr;
  std::uint32_t scratch_size = 0;
};

namespace detail {

inline std::uint16_t le16(const unsigned char* p) {
  return static_cast<std::uint16_t>(p[0]) | static_cast<std::uint16_t>(p[1] << 8);
}

inline std::uint32_t le32(const unsigned char* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

inline bool range_ok(std::uint32_t start, std::uint32_t length, std::uint32_t total) {
  return start <= total && length <= total - start;
}

inline status read_at(const file_view& view, std::uint32_t offset, void* destination,
                      std::uint32_t size) {
  if (view.device == nullptr || !range_ok(offset, size, view.size)) return status::io;
  return read(view, offset, destination, size) ? status::ok : status::io;
}

inline status read_u32(const file_view& view, std::uint32_t offset, std::uint32_t& value) {
  unsigned char bytes[4];
  const status result = read_at(view, offset, bytes, sizeof(bytes));
  if (result == status::ok) value = le32(bytes);
  return result;
}

inline bool string_equal_at(const file_view& view, std::uint32_t offset, std::uint16_t length,
                            const char* text) {
  for (std::uint16_t i = 0; i < length; ++i) {
    unsigned char c = 0;
    if (text[i] == '\0' || read_at(view, offset + i, &c, 1) != status::ok ||
        c != static_cast<unsigned char>(text[i])) {
      return false;
    }
  }
  return text[length] == '\0';
}

inline constexpr std::uint32_t zip_local_signature = 0x04034b50UL;
inline constexpr std::uint32_t zip_central_signature = 0x02014b50UL;
inline constexpr std::uint32_t zip_end_signature = 0x06054b50UL;
inline constexpr std::uint32_t zip_central_header_size = 46u;

/** @brief Finds a central-directory entry's 46-byte header and its file offset. */
inline status zip_find(const file_view& view, const char* path, unsigned char header[46],
                       std::uint32_t& header_at) {
  constexpr std::uint32_t tail_capacity = 65557u;
  static EPUB_WORK_BSS std::array<unsigned char, tail_capacity> tail{};
  if (view.device == nullptr || path == nullptr || view.size < 22U) return status::format;
  const std::uint32_t tail_size = view.size > tail_capacity ? tail_capacity : view.size;
  const std::uint32_t tail_start = view.size - tail_size;
  if (read_at(view, tail_start, tail.data(), tail_size) != status::ok) return status::io;
  int end_offset = -1;
  for (std::int64_t offset = static_cast<std::int64_t>(tail_size) - 22; offset >= 0; --offset) {
    if (le32(tail.data() + offset) == zip_end_signature) {
      end_offset = static_cast<int>(offset);
      break;
    }
  }
  if (end_offset < 0) return status::format;
  const unsigned char* end = tail.data() + end_offset;
  if (le16(end + 4) != 0 || le16(end + 6) != 0 || le16(end + 8) != le16(end + 10) ||
      le16(end + 8) == 0xFFFFu || le32(end + 12) == 0xFFFFFFFFu ||
      le32(end + 16) == 0xFFFFFFFFu) {
    return status::unsupported;
  }
  const std::uint16_t entries = le16(end + 10);
  const std::uint32_t directory_size = le32(end + 12);
  const std::uint32_t directory_at = le32(end + 16);
  if (!range_ok(directory_at, directory_size, view.size)) return status::format;
  const std::uint32_t directory_end = directory_at + directory_size;
  std::uint32_t pos = directory_at;
  for (std::uint16_t i = 0; i < entries; ++i) {
    if (!range_ok(pos, zip_central_header_size, directory_end) ||
        read_at(view, pos, header, zip_central_header_size) != status::ok) {
      return status::format;
    }
    if (le32(header) != zip_central_signature) return status::format;
    const std::uint16_t name_size = le16(header + 28);
    const std::uint16_t extra_size = le16(header + 30);
    const std::uint16_t comment_size = le16(header + 32);
    const std::uint32_t next = pos + zip_central_header_size;
    if (!range_ok(next, static_cast<std::uint32_t>(name_size) + extra_size + comment_size,
                  directory_end)) {
      return status::format;
    }
    if (string_equal_at(view, next, name_size, path)) {
      header_at = pos;
      return status::ok;
    }
    pos = next + name_size + extra_size + comment_size;
  }
  return status::not_found;
}

}  // namespace detail

/** @brief Copies one archive entry into `destination`; entries are not NUL-terminated. */
inline status extract(const context& doc, const char* path, void* destination,
                      std::uint32_t destination_size, std::uint32_t& extracted_size) {
  unsigned char central[46];
  unsigned char local[30];
  std::uint32_t central_at = 0;
  if (doc.storage == nullptr || path == nullptr || destination == nullptr) return status::argument;
  status result = detail::zip_find(*doc.storage, path, central, central_at);
  if (result != status::ok) return result;
  const std::uint16_t flags = detail::le16(central + 8);
  const std::uint16_t method = detail::le16(central + 10);
  const std::uint32_t compressed = detail::le32(central + 20);
  const std::uint32_t uncompressed = detail::le32(central + 24);
  const std::uint32_t local_at = detail::le32(central + 42);
  if ((flags & 1u) != 0) return status::unsupported;
  if (uncompressed > destination_size) return status::buffer_too_small;
  if (!detail::range_ok(local_at, sizeof(local), doc.storage->size) ||
      detail::read_at(*doc.storage, local_at, local, sizeof(local)) != status::ok ||
      detail::le32(local) != detail::zip_local_signature) {
    return status::format;
  }
  const std::uint16_t name_size = detail::le16(local + 26);
  const std::uint16_t extra_size = detail::le16(local + 28);
  std::uint32_t data_at = local_at + 30u;
  if (data_at < local_at ||
      !detail::range_ok(data_at, static_cast<std::uint32_t>(name_size) + extra_size,
                        doc.storage->size)) {
    return status::format;
  }
  data_at += name_size + extra_size;
  if (!detail::range_ok(data_at, compressed, doc.storage->size)) return status::format;
  if (method == 0) {
    if (compressed != uncompressed) return status::format;
    result = detail::read_at(*doc.storage, data_at, destination, uncompressed);
  } else if (method == 8) {
    result = inflate_raw(*doc.storage, data_at, compressed, destination, uncompressed)
                 ? status::ok
                 : status::io;
  } else {
    return status::unsupported;
  }
  if (result == status::ok) extracted_size = uncompressed;
  return result;
}

namespace detail {

inline bool name_is(const char* start, const char* end, const char* name) {
  const char* local = start;
  for (const char* p = start; p < end; ++p) {
    if (*p == ':') local = p + 1;
  }
  while (local < end && *name != '\0' && *local == *name) {
    ++local;
    ++name;
  }
  return local == end && *name == '\0';
}

inline const char* tag_next(const char* p, const char* limit, const char*& name,
                            const char*& name_end, const char*& tag_end) {
  while (p < limit) {
    while (p < limit && *p != '<') ++p;
    if (p == limit || ++p == limit) return nullptr;
    if (*p == '/' || *p == '!' || *p == '?') {
      while (p < limit && *p != '>') ++p;
      if (p == limit) return nullptr;
      ++p;
      continue;  // never return stale name pointers for closing tags.
    }
    name = p;
    while (p < limit && *p != '>' && *p != '/' && *p != ' ' && *p != '\t' && *p != '\r' &&
           *p != '\n') {
      ++p;
    }
    name_end = p;
    while (p < limit && *p != '>') ++p;
    if (p == limit) return nullptr;
    tag_end = p;
    return p + 1;
  }
  return nullptr;
}

inline bool copy_value(char* destination, std::uint32_t size, const char* value,
                       const char* end) {
  const auto n = static_cast<std::uint32_t>(end - value);
  if (n >= size) return false;
  std::memcpy(destination, value, n);
  destination[n] = '\0';
  return true;
}

inline bool attribute(const char* p, const char* end, const char* wanted, char* destination,
                      std::uint32_t destination_size) {
  while (p < end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '/')) ++p;
    const char* key = p;
    while (p < end && *p != '=' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
    const char* key_end = p;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
    if (p == end || *p != '=') {
      while (p < end && *p != ' ') ++p;
      continue;
    }
    ++p;
    while (p < end && (*p == ' ' || *p == '\t')) ++p;
    if (p == end || (*p != '\'' && *p != '\"')) return false;
    const char quote = *p++;
    const char* value = p;
    while (p < end && *p != quote) ++p;
    if (p == end) return false;
    if (name_is(key, key_end, wanted)) return copy_value(destination, destination_size, value, p);
    ++p;
  }
  return false;
}

inline status parse_container(context& doc, const char* xml, std::uint32_t size) {
  const char* p = xml;
  const char* limit = xml + size;
  const char *name = nullptr, *name_end = nullptr, *tag_end = nullptr;
  while ((p = tag_next(p, limit, name, name_end, tag_end)) != nullptr) {
    if (name_is(name, name_end, "rootfile")) {
      if (!attribute(name_end, tag_end, "full-path", doc.package_path,
                     sizeof(doc.package_path))) {
        return status::format;
      }
      return status::ok;
    }
  }
  return status::format;
}

inline std::uint16_t manifest_index(const context& doc, const char* id) {
  for (std::uint16_t i = 0; i < doc.manifest_count; ++i) {
    if (std::strcmp(doc.manifest[i].id, id) == 0) return i;
  }
  return no_item;
}

inline status parse_opf(context& doc, const char* xml, std::uint32_t size) {
  const char* p = xml;
  const char* limit = xml + size;
  const char *name = nullptr, *name_end = nullptr, *tag_end = nullptr;
  char id[id_max] = {0};
  while ((p = tag_next(p, limit, name, name_end, tag_end)) != nullptr) {
    if (name_is(name, name_end, "item")) {
      if (doc.manifest_count == doc.manifest_capacity) return status::capacity;
      manifest_item& item = doc.manifest[doc.manifest_count];
      if (!attribute(name_end, tag_end, "id", item.id, sizeof(item.id)) ||
          !attribute(name_end, tag_end, "href", item.href, sizeof(item.href))) {
        return status::format;
      }
      ++doc.manifest_count;
    } else if (name_is(name, name_end, "itemref")) {
      if (!attribute(name_end, tag_end, "idref", id, sizeof(id))) return status::format;
      const std::uint16_t index = manifest_index(doc, id);
      if (index == no_item) return status::format;
      if (doc.spine_count == doc.spine_capacity) return status::capacity;
      doc.spine[doc.spine_count++].manifest_index = index;
    }
  }
  if (doc.title[0] == '\0') {
    const char* start = std::strstr(xml, "<dc:title");
    if (start != nullptr) {
      start = std::strchr(start, '>');
      if (start != nullptr) {
        ++start;
        const char* end = std::strstr(start, "</dc:title>");
        if (end != nullptr) copy_value(doc.title, sizeof(doc.title), start, end);
      }
    }
  }
  return doc.spine_count != 0 ? status::ok : status::format;
}

}  // namespace detail

/**
 * @brief Parses META-INF/container.xml and the package OPF. `scratch` is reused for
 * archive entries and must hold both files individually. No storage is owned by the
 * EPUB object and no dynamic allocation is performed.
 */
inline status open(context& doc, const file_view& storage, void* scratch,
                   std::uint32_t scratch_size, manifest_item* manifest,
                   std::uint16_t manifest_capacity, spine_item* spine,
                   std::uint16_t spine_capacity) {
  if (storage.device == nullptr || scratch == nullptr || scratch_size < 2 || manifest == nullptr ||
      manifest_capacity == 0 || spine == nullptr || spine_capacity == 0) {
    return status::argument;
  }
  doc = context{};
  doc.storage = &storage;
  doc.scratch = scratch;
  doc.scratch_size = scratch_size;
  doc.manifest = manifest;
  doc.manifest_capacity = manifest_capacity;
  doc.spine = spine;
  doc.spine_capacity = spine_capacity;

  std::uint32_t size = 0;
  status result = extract(doc, "META-INF/container.xml", scratch, scratch_size - 1, size);
  if (result != status::ok) return result;
  static_cast<char*>(scratch)[size] = '\0';
  result = detail::parse_container(doc, static_cast<const char*>(scratch), size);
  if (result != status::ok) return result;

  result = extract(doc, doc.package_path, scratch, scratch_size - 1, size);
  if (result != status::ok) return result;
  static_cast<char*>(scratch)[size] = '\0';
  return detail::parse_opf(doc, static_cast<const char*>(scratch), size);
}

/** @brief Resolves a spine item's href relative to the OPF path. */
inline status spine_path(const context& doc, std::uint16_t spine_index, char* path,
                         std::uint32_t path_size) {
  if (path == nullptr || spine_index >= doc.spine_count) return status::argument;
  const std::uint16_t item = doc.spine[spine_index].manifest_index;
  if (item >= doc.manifest_count) return status::format;
  const char* href = doc.manifest[item].href;
  std::uint32_t prefix = 0;
  if (href[0] == '/') {
    ++href;
  } else {
    const char* slash = std::strrchr(doc.package_path, '/');
    prefix = slash != nullptr ? static_cast<std::uint32_t>(slash - doc.package_path + 1) : 0;
  }
  std::uint32_t href_size = 0;
  while (href[href_size] != '\0' && href[href_size] != '#') ++href_size;
  if (prefix > path_size || href_size >= path_size - prefix) return status::buffer_too_small;
  std::memcpy(path, doc.package_path, prefix);
  std::memcpy(path + prefix, href, href_size);
  path[prefix + href_size] = '\0';

  // Normalize EPUB's relative href segments without allocating a second path.
  std::uint32_t read_pos = 0;
  std::uint32_t written = 0;
  while (read_pos < prefix + href_size) {
    while (read_pos < prefix + href_size && path[read_pos] == '/') ++read_pos;
    const std::uint32_t segment_start = read_pos;
    while (read_pos < prefix + href_size && path[read_pos] != '/') ++read_pos;
    const std::uint32_t segment_size = read_pos - segment_start;
    if (segment_size == 0 || (segment_size == 1 && path[segment_start] == '.')) continue;
    if (segment_size == 2 && path[segment_start] == '.' && path[segment_start + 1] == '.') {
      if (written == 0) return status::format;
      while (written != 0 && path[written - 1] != '/') --written;
      if (written != 0) --written;
      continue;
    }
    if (written != 0) path[written++] = '/';
    std::memmove(path + written, path + segment_start, segment_size);
    written += segment_size;
  }
  path[written] = '\0';
  return status::ok;
}

namespace detail {

inline bool append_char(char* text, std::uint32_t size, std::uint32_t& used, char c) {
  if (used + 1 >= size) return false;
  text[used++] = c;
  return true;
}

inline bool append_entity(const char*& source, const char* limit, char* text, std::uint32_t size,
                          std::uint32_t& used) {
  const char* p = source;
  char c = 0;
  if (limit - p >= 4 && std::memcmp(p, "amp;", 4) == 0) {
    c = '&';
    p += 4;
  } else if (limit - p >= 3 && std::memcmp(p, "lt;", 3) == 0) {
    c = '<';
    p += 3;
  } else if (limit - p >= 3 && std::memcmp(p, "gt;", 3) == 0) {
    c = '>';
    p += 3;
  } else if (limit - p >= 5 && std::memcmp(p, "quot;", 5) == 0) {
    c = '\"';
    p += 5;
  } else if (limit - p >= 5 && std::memcmp(p, "apos;", 5) == 0) {
    c = '\'';
    p += 5;
  } else {
    return append_char(text, size, used, '&');
  }
  source = p;
  return append_char(text, size, used, c);
}

inline bool text_break_tag(const char* tag, const char* end) {
  const char* name = tag;
  if (name < end && *name == '/') ++name;
  const char* name_end = name;
  while (name_end < end && *name_end != ' ' && *name_end != '\t' && *name_end != '\r' &&
         *name_end != '\n' && *name_end != '/') {
    ++name_end;
  }
  return name_is(name, name_end, "br") || name_is(name, name_end, "p") ||
         name_is(name, name_end, "div") || name_is(name, name_end, "li") ||
         name_is(name, name_end, "h1") || name_is(name, name_end, "h2") ||
         name_is(name, name_end, "h3") || name_is(name, name_end, "tr");
}

inline status xhtml_text(const char* source, std::uint32_t source_size, char* text,
                         std::uint32_t text_size, std::uint32_t* out) {
  const char* p = source;
  const char* limit = source + source_size;
  std::uint32_t used = 0;
  bool space = true;
  const char* ignored = nullptr;
  if (text == nullptr || text_size == 0) return status::argument;
  while (p < limit) {
    if (ignored != nullptr && *p != '<') {
      ++p;
      continue;
    }
    if (*p == '<') {
      const char* tag = ++p;
      while (p < limit && *p != '>') ++p;
      if (p == limit) return status::format;
      const char* tag_name = tag;
      if (*tag_name == '/') ++tag_name;
      const char* tag_name_end = tag_name;
      while (tag_name_end < p && *tag_name_end != ' ' && *tag_name_end != '\t' &&
             *tag_name_end != '\r' && *tag_name_end != '\n' && *tag_name_end != '/') {
        ++tag_name_end;
      }
      const bool closing = tag != tag_name;
      if (ignored != nullptr) {
        if (closing && name_is(tag_name, tag_name_end, ignored)) ignored = nullptr;
        ++p;
        continue;
      }
      if (!closing && (name_is(tag_name, tag_name_end, "head") ||
                       name_is(tag_name, tag_name_end, "style") ||
                       name_is(tag_name, tag_name_end, "script"))) {
        ignored = name_is(tag_name, tag_name_end, "head")
                      ? "head"
                      : (name_is(tag_name, tag_name_end, "style") ? "style" : "script");
        ++p;
        continue;
      }
      const bool break_tag = text_break_tag(tag, p);
      ++p;
      if (break_tag && !space) {
        if (!append_char(text, text_size, used, '\n')) return status::buffer_too_small;
        space = true;
      }
    } else if (*p == '&') {
      ++p;
      if (!append_entity(p, limit, text, text_size, used)) return status::buffer_too_small;
      space = false;
    } else if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
      ++p;
      if (!space) {
        if (!append_char(text, text_size, used, ' ')) return status::buffer_too_small;
        space = true;
      }
    } else {
      if (!append_char(text, text_size, used, *p++)) return status::buffer_too_small;
      space = false;
    }
  }
  while (used != 0 && (text[used - 1] == ' ' || text[used - 1] == '\n')) --used;
  text[used] = '\0';
  if (out != nullptr) *out = used;
  return status::ok;
}

}  // namespace detail

/**
 * @brief Extracts one spine XHTML document and strips markup into plain text.
 * `scratch` holds the XHTML source; `text` receives a NUL-terminated result.
 */
inline status spine_text(const context& doc, std::uint16_t spine_index, void* scratch,
                         std::uint32_t scratch_size, char* text, std::uint32_t text_size,
                         std::uint32_t& text_size_out) {
  char path[path_max] = {0};
  if (scratch == nullptr || scratch_size < 2) return status::argument;
  status result = spine_path(doc, spine_index, path, sizeof(path));
  if (result != status::ok) return result;
  std::uint32_t size = 0;
  result = extract(doc, path, scratch, scratch_size - 1, size);
  if (result != status::ok) return result;
  static_cast<char*>(scratch)[size] = '\0';
  return detail::xhtml_text(static_cast<const char*>(scratch), size, text, text_size,
                            &text_size_out);
}

}  // namespace epub
