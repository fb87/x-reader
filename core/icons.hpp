#pragma once

#include "core/canvas.hpp"
#include "core/geometry.hpp"

#include "xr_icons.h"

#include <cstdint>
#include <cstring>

namespace icon {

inline void draw(display::device& display, geometry::rect rect, int value,
                 canvas::gray color = canvas::gray::black) {
  if (value <= XR_ICON_NONE || value >= XR_ICON_COUNT) return;
  const auto& glyph = xr_icons[value];
  const int width = glyph.width > 24 ? 24 : glyph.width;
  for (int y = 0; y < 24; ++y) {
    for (int x = 0; x < width; ++x) {
      const auto byte = glyph.bitmap[y * 3 + x / 8];
      if ((byte & static_cast<std::uint8_t>(0x80U >> (x % 8))) != 0)
        canvas::fill(display, {rect.x + x, rect.y + y, 1, 1}, color);
    }
  }
}

inline int for_label(const char* value) {
  if (value == nullptr) return XR_ICON_NONE;
  if (std::strcmp(value, "LIBRARY") == 0 || std::strcmp(value, "FAVORITES") == 0)
    return XR_ICON_BOOK;
  if (std::strcmp(value, "FILE MANAGER") == 0) return XR_ICON_FOLDER;
  if (std::strcmp(value, "SETTINGS") == 0) return XR_ICON_SETTINGS;
  if (std::strcmp(value, "SLEEP") == 0) return XR_ICON_CLOSE;
  if (std::strcmp(value, "WIFI") == 0) return XR_ICON_SD_CARD;
  return XR_ICON_NONE;
}

}  // namespace icon
