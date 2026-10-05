#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <strings.h>

/**
 * @brief Ebook-reader book metadata and display-title repair, ported from
 * `app_book_t` (app/app_internal.h) and `app_book_display_title`
 * (app/book_title.c, app/book_title.h).
 *
 * Some library sources hand back a filename that was UTF-8 encoded
 * twice (common with tools that assumed Windows-1252/Latin-1 somewhere
 * in the pipeline). `make_title` detects and undoes exactly one such
 * extra encoding layer -- it does not touch already-correct UTF-8 -- and
 * strips a trailing .epub/.epu extension.
 */
namespace book {

inline constexpr std::size_t title_max = 128;
inline constexpr std::size_t path_max = 256;

/** @brief One library entry's metadata. */
struct item {
  std::array<char, title_max> title{};
  const char* author = nullptr;
  const char* format = nullptr;
  std::array<char, path_max> path{};
  std::uint16_t pages = 0;
  std::uint8_t progress = 0;  ///< percent.
  std::uint16_t size_kb = 0;
  bool favorite = false;
  bool epub_source = false;
  bool transient = false;
};

namespace detail {

inline bool utf8_codepoint(const unsigned char* text, std::size_t available, unsigned& value,
                           std::size_t& length) {
  const unsigned first = text[0];
  if (first < 0x80) {
    value = first;
    length = 1;
    return true;
  }

  std::size_t count = 0;
  unsigned codepoint = 0;
  unsigned minimum = 0;
  if ((first & 0xe0) == 0xc0) {
    count = 2;
    codepoint = first & 0x1f;
    minimum = 0x80;
  } else if ((first & 0xf0) == 0xe0) {
    count = 3;
    codepoint = first & 0x0f;
    minimum = 0x800;
  } else if ((first & 0xf8) == 0xf0) {
    count = 4;
    codepoint = first & 0x07;
    minimum = 0x10000;
  } else {
    return false;
  }
  if (count > available) return false;
  for (std::size_t i = 1; i < count; ++i) {
    if ((text[i] & 0xc0) != 0x80) return false;
    codepoint = (codepoint << 6) | (text[i] & 0x3f);
  }
  if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
    return false;
  }
  value = codepoint;
  length = count;
  return true;
}

inline bool valid_utf8(const unsigned char* text, std::size_t length) {
  for (std::size_t offset = 0; offset < length;) {
    unsigned value = 0;
    std::size_t count = 0;
    if (!utf8_codepoint(text + offset, length - offset, value, count)) return false;
    offset += count;
  }
  return true;
}

inline bool windows_1252_byte(unsigned codepoint, unsigned char& value) {
  static constexpr unsigned mapping[32] = {
      0x20ac, 0,      0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
      0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017d, 0,
      0,      0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
      0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0,      0x017e, 0x0178,
  };
  if (codepoint <= 0xff) {
    value = static_cast<unsigned char>(codepoint);
    return true;
  }
  for (unsigned i = 0; i < 32; ++i) {
    if (mapping[i] == codepoint) {
      value = static_cast<unsigned char>(0x80 + i);
      return true;
    }
  }
  return false;
}

}  // namespace detail

/**
 * @brief Repairs a title that was accidentally UTF-8-encoded twice, and strips a
 * trailing .epub/.epu extension. Leaves already-correct text unchanged.
 */
inline void make_title(char* output, std::size_t capacity, const char* input) {
  if (output == nullptr || capacity == 0) return;
  if (input == nullptr) input = "";

  const std::size_t input_length = std::strlen(input);
  std::size_t used = 0;
  bool changed = false;
  bool repairable = true;
  for (std::size_t offset = 0; offset < input_length;) {
    unsigned codepoint = 0;
    unsigned char byte = 0;
    std::size_t count = 0;
    if (!detail::utf8_codepoint(reinterpret_cast<const unsigned char*>(input) + offset,
                                input_length - offset, codepoint, count) ||
        !detail::windows_1252_byte(codepoint, byte) || used + 1 >= capacity) {
      repairable = false;
      break;
    }
    output[used++] = static_cast<char>(byte);
    changed = changed || count > 1;
    offset += count;
  }
  if (repairable && changed &&
      detail::valid_utf8(reinterpret_cast<const unsigned char*>(output), used)) {
    output[used] = '\0';
  } else {
    std::snprintf(output, capacity, "%s", input);
  }

  char* extension = std::strrchr(output, '.');
  if (extension != nullptr) {
    if (strcasecmp(extension, ".epub") == 0 || strcasecmp(extension, ".epu") == 0) {
      *extension = '\0';
    }
  }
}

}  // namespace book
