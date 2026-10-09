#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "core/canvas.hpp"
#include "core/unicode_glyphs.hpp"
#include "xr/xr_text.h"

namespace text {

extern "C" {
extern const xr_font_t xr_font_alegreya_14;
extern const xr_font_t xr_font_alegreya_18;
extern const xr_font_t xr_font_alegreya_20;
extern const xr_font_t xr_font_alegreya_24;
extern const xr_font_t xr_font_alegreya_bold_18;
extern const xr_font_t xr_font_alegreya_bold_26;
}

inline const xr_font_t& rich_font(int scale) {
  if (scale <= 1) return xr_font_alegreya_14;
  if (scale == 2) return xr_font_alegreya_18;
  if (scale == 3) return xr_font_alegreya_20;
  return xr_font_alegreya_24;
}

inline std::uint32_t next_codepoint(const char*& cursor);

inline const xr_glyph_t* rich_glyph(const xr_font_t& font, std::uint32_t codepoint) {
  if (font.codepoints != nullptr) {
    for (std::uint16_t i = 0; i < font.glyph_count; ++i)
      if (font.codepoints[i] == codepoint) return &font.glyphs[i];
    return nullptr;
  }
  if (codepoint < font.first || codepoint > font.last) return nullptr;
  return &font.glyphs[codepoint - font.first];
}

inline int advance(std::uint32_t codepoint, int scale) {
  const auto& font = rich_font(scale);
  const auto* glyph = rich_glyph(font, codepoint);
  return glyph != nullptr ? glyph->advance : font.ascent / 2;
}

inline int font_width(const xr_font_t& font, const char* value) {
  if (value == nullptr) return 0;
  int result = 0;
  const char* cursor = value;
  while (*cursor != '\0') {
    const auto* glyph = rich_glyph(font, next_codepoint(cursor));
    result += glyph != nullptr ? glyph->advance : font.ascent / 2;
  }
  return result;
}

inline void fit_font(const xr_font_t& font, const char* value, int max_width, char* output,
                     std::size_t capacity) {
  if (output == nullptr || capacity == 0) return;
  if (value == nullptr) {
    output[0] = '\0';
    return;
  }
  if (font_width(font, value) <= max_width) {
    std::snprintf(output, capacity, "%s", value);
    return;
  }
  const int ellipsis_width = font_width(font, "...");
  const char* cursor = value;
  std::size_t used = 0;
  while (*cursor != '\0') {
    const char* next = cursor;
    (void)next_codepoint(next);
    const std::size_t bytes = static_cast<std::size_t>(next - cursor);
    if (used + bytes + 4 >= capacity) break;
    char candidate[256]{};
    std::memcpy(candidate, value, used + bytes);
    candidate[used + bytes] = '\0';
    if (font_width(font, candidate) + ellipsis_width > max_width) break;
    std::memcpy(output + used, cursor, bytes);
    used += bytes;
    cursor = next;
  }
  if (used + 4 < capacity)
    std::memcpy(output + used, "...", 4);
  else
    output[capacity - 1] = '\0';
}

inline void draw_font(display::device& display, int x, int y, const char* value,
                      const xr_font_t& font, canvas::gray color) {
  if (value == nullptr) return;
  const int baseline = y + font.ascent;
  const char* cursor = value;
  while (*cursor != '\0') {
    const auto* glyph = rich_glyph(font, next_codepoint(cursor));
    if (glyph == nullptr) {
      x += font.ascent / 2;
      continue;
    }
    canvas::draw_mask(display, x + glyph->x_off, baseline + glyph->y_off, glyph->w, glyph->h,
                      font.bitmap + glyph->offset, glyph->w, color);
    x += glyph->advance;
  }
}

inline void draw_bold(display::device& display, int x, int y, const char* value,
                      canvas::gray color = canvas::gray::black) {
  draw_font(display, x, y, value, xr_font_alegreya_bold_18, color);
}

inline int bold_width(const char* value) { return font_width(xr_font_alegreya_bold_18, value); }

inline void draw_title(display::device& display, int x, int y, const char* value,
                       canvas::gray color = canvas::gray::black) {
  draw_font(display, x, y, value, xr_font_alegreya_bold_26, color);
}

inline int rich_width(const char* value, int scale) { return font_width(rich_font(scale), value); }

inline void draw_rich(display::device& display, int x, int y, const char* value, int scale,
                      canvas::gray color) {
  draw_font(display, x, y, value, rich_font(scale), color);
}

/** @brief Compact 5x7 bitmap glyph for printable ASCII characters. */
struct glyph {
  char character;
  std::uint8_t rows[7];
};

/** @brief Returns a compact uppercase-style glyph for basic UI text. */
inline const std::uint8_t* bitmap(char character) {
  static constexpr glyph glyphs[] = {
      {' ', {0, 0, 0, 0, 0, 0, 0}},        {'-', {0, 0, 0, 31, 0, 0, 0}},
      {'.', {0, 0, 0, 0, 0, 6, 6}},        {'/', {1, 2, 4, 8, 16, 0, 0}},
      {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
      {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
      {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
      {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
      {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
      {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
      {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
      {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
      {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
      {'I', {14, 4, 4, 4, 4, 4, 14}},      {'J', {7, 2, 2, 2, 2, 18, 12}},
      {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
      {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
      {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
      {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
      {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
      {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
      {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
      {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
      {':', {0, 6, 6, 0, 6, 6, 0}},        {'%', {17, 2, 4, 8, 17, 0, 0}},
      {'+', {0, 4, 4, 31, 4, 4, 0}},       {'?', {14, 17, 1, 2, 4, 0, 4}},
      {'(', {2, 4, 8, 8, 8, 4, 2}},        {')', {8, 4, 2, 2, 2, 4, 8}},
      {'[', {14, 8, 8, 8, 8, 8, 14}},      {']', {14, 2, 2, 2, 2, 2, 14}},
  };
  if (character >= 'a' && character <= 'z') character = static_cast<char>(character - 'a' + 'A');
  for (const auto& item : glyphs)
    if (item.character == character) return item.rows;
  static constexpr std::uint8_t unknown[7] = {14, 17, 1, 2, 4, 0, 4};
  return unknown;
}

/** @brief Draws one bitmap character with integer scaling. */
inline void draw_char(display::device& display, int x, int y, char character, int scale,
                      canvas::gray color) {
  const auto* rows = bitmap(character);
  for (int row = 0; row < 7; ++row) {
    for (int column = 0; column < 5; ++column) {
      if ((rows[row] & (1U << (4 - column))) != 0) {
        canvas::fill(display, {x + column * scale, y + row * scale, scale, scale}, color);
      }
    }
  }
}

/** @brief Decodes one UTF-8 code point and advances `cursor`. Invalid bytes map to '?'. */
inline std::uint32_t next_codepoint(const char*& cursor) {
  const auto first = static_cast<std::uint8_t>(*cursor++);
  if (first < 0x80U) return first;
  auto continuation = [](std::uint8_t value) { return (value & 0xC0U) == 0x80U; };
  if ((first & 0xE0U) == 0xC0U) {
    const auto b1 = static_cast<std::uint8_t>(*cursor);
    if (!continuation(b1)) return '?';
    ++cursor;
    return ((first & 0x1FU) << 6U) | (b1 & 0x3FU);
  }
  if ((first & 0xF0U) == 0xE0U) {
    const auto b1 = static_cast<std::uint8_t>(cursor[0]);
    const auto b2 = static_cast<std::uint8_t>(cursor[1]);
    if (!continuation(b1) || !continuation(b2)) return '?';
    cursor += 2;
    return ((first & 0x0FU) << 12U) | ((b1 & 0x3FU) << 6U) | (b2 & 0x3FU);
  }
  if ((first & 0xF8U) == 0xF0U) {
    const auto b1 = static_cast<std::uint8_t>(cursor[0]);
    const auto b2 = static_cast<std::uint8_t>(cursor[1]);
    const auto b3 = static_cast<std::uint8_t>(cursor[2]);
    if (!continuation(b1) || !continuation(b2) || !continuation(b3)) return '?';
    cursor += 3;
    return ((first & 0x07U) << 18U) | ((b1 & 0x3FU) << 12U) | ((b2 & 0x3FU) << 6U) | (b3 & 0x3FU);
  }
  return '?';
}

/** @brief Draws one UTF-8 code point in the common 6x7 UI cell. */
inline void draw_codepoint(display::device& display, int x, int y, std::uint32_t codepoint,
                           int scale, canvas::gray color) {
  if (codepoint < 0x80U) {
    draw_char(display, x, y, static_cast<char>(codepoint), scale, color);
    return;
  }

  const auto* glyph = unicode::find(codepoint);
  if (glyph == nullptr) {
    draw_char(display, x, y, '?', scale, color);
    return;
  }

  // Translation glyphs are generated as 12x12 bitmaps. Downsample each 2x2
  // source block into one pixel so ASCII, Vietnamese and CJK all share the
  // same 6x7 metrics/baseline used by the stock UI font.
  for (int row = 0; row < 6; ++row) {
    for (int column = 0; column < 6; ++column) {
      bool set = false;
      for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
          const int source_row = row * 2 + dy;
          const int source_column = column * 2 + dx;
          set = set || ((glyph->rows[source_row] & (1U << (11 - source_column))) != 0);
        }
      }
      if (set) {
        canvas::fill(display, {x + column * scale, y + row * scale, scale, scale}, color);
      }
    }
  }
}

/** @brief Returns the rendered width of a UTF-8 string at the requested scale. */
inline int width(const char* value, int scale = 2) {
  if (value == nullptr) return 0;
  return rich_width(value, scale);
  /*
  int count = 0;
  const char* cursor = value;
  while (*cursor != '\0') {
    (void)next_codepoint(cursor);
    ++count;
  }
  return count == 0 ? 0 : (count * 6 - 1) * scale; */
}

/** @brief Copies a string into a bounded single-line buffer, adding an ellipsis when needed. */
inline int fit(const char* value, int max_width, int scale, char* output, std::size_t capacity) {
  if (output == nullptr || capacity == 0) return 0;
  if (value == nullptr) {
    output[0] = '\0';
    return 0;
  }
  const int total = width(value, scale);
  if (total <= max_width) {
    std::snprintf(output, capacity, "%s", value);
    return total;
  }
  const char* cursor = value;
  std::size_t used = 0;
  const int ellipsis_width = width("...", scale);
  while (*cursor != '\0') {
    const char* next = cursor;
    (void)next_codepoint(next);
    const std::size_t bytes = static_cast<std::size_t>(next - cursor);
    if (used + bytes + 4 >= capacity) break;
    char candidate[256] = {0};
    std::memcpy(candidate, value, used + bytes);
    candidate[used + bytes] = '\0';
    if (width(candidate, scale) + ellipsis_width > max_width) break;
    std::memcpy(output + used, cursor, bytes);
    used += bytes;
    cursor = next;
  }
  if (used + 4 <= capacity) {
    std::memcpy(output + used, "...", 4);
    return width(output, scale);
  }
  output[used < capacity ? used : capacity - 1] = '\0';
  return width(output, scale);
}

/** @brief Draws a NUL-terminated UTF-8 bitmap string. */
inline void draw(display::device& display, int x, int y, const char* value, int scale = 2,
                 canvas::gray color = canvas::gray::black) {
  draw_rich(display, x, y, value, scale, color);
  return;
  /*
  if (value == nullptr) return;
  const char* cursor = value;
  while (*cursor != '\0') {
    draw_codepoint(display, x, y, next_codepoint(cursor), scale, color);
    x += 6 * scale;
  } */
}

/** @brief Draws one line clipped and ellipsized to a horizontal width. */
inline void draw_in(display::device& display, int x, int y, int max_width, const char* value,
                    int scale = 2, canvas::gray color = canvas::gray::black) {
  char fitted[256] = {0};
  fit(value, max_width, scale, fitted, sizeof(fitted));
  draw(display, x, y, fitted, scale, color);
}

inline void draw_bold_in(display::device& display, int x, int y, int max_width, const char* value,
                         canvas::gray color = canvas::gray::black) {
  char fitted[256]{};
  fit_font(xr_font_alegreya_bold_18, value, max_width, fitted, sizeof(fitted));
  draw_bold(display, x, y, fitted, color);
}

/** @brief Returns the common bitmap height used by every UTF-8 glyph. */
inline int height(const char* value, int scale = 2) {
  return value == nullptr || value[0] == '\0' ? 0 : rich_font(scale).line_height;
}

/** @brief Draws text centered in a rectangle. */
inline void center(display::device& display, geometry::rect area, const char* value, int scale = 2,
                   canvas::gray color = canvas::gray::black) {
  draw(display, area.x + (area.w - width(value, scale)) / 2,
       area.y + (area.h - height(value, scale)) / 2, value, scale, color);
}

}  // namespace text
