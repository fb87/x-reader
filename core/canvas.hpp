#pragma once

#include <algorithm>
#include <cstdint>

#include "core/display.hpp"
#include "core/geometry.hpp"

namespace canvas {

/** @brief Logical grayscale values used by simple application rendering. */
enum class gray : std::uint8_t {
  black = 0x0,
  dark = 0x5,
  light = 0xa,
  white = 0xf,
};

/** @brief Writes one pixel to a supported framebuffer. */
inline void pixel(display::device& display, int x, int y, gray value) {
  if (x < 0 || y < 0 || x >= display.width || y >= display.height ||
      display.framebuffer == nullptr) {
    return;
  }
  const auto level = static_cast<std::uint8_t>(value);
  if (display.format == display::pixel_format::gray4) {
    auto& byte = display.framebuffer[static_cast<std::size_t>(y) * display.stride + x / 2];
    if ((x & 1) == 0) {
      byte = static_cast<std::uint8_t>((byte & 0x0fU) | (level << 4U));
    } else {
      byte = static_cast<std::uint8_t>((byte & 0xf0U) | level);
    }
  } else if (display.format == display::pixel_format::gray8) {
    display.framebuffer[static_cast<std::size_t>(y) * display.stride + x] =
        static_cast<std::uint8_t>(level * 17U);
  }
}

inline std::uint8_t get_pixel(display::device& display, int x, int y) {
  if (x < 0 || y < 0 || x >= display.width || y >= display.height || display.framebuffer == nullptr)
    return 0x0f;
  if (display.format == display::pixel_format::gray4) {
    const auto byte = display.framebuffer[static_cast<std::size_t>(y) * display.stride + x / 2];
    return (x & 1) == 0 ? static_cast<std::uint8_t>(byte >> 4U)
                        : static_cast<std::uint8_t>(byte & 0x0fU);
  }
  return static_cast<std::uint8_t>(
      display.framebuffer[static_cast<std::size_t>(y) * display.stride + x] / 17U);
}

inline void draw_mask(display::device& display, int x, int y, int width, int height,
                      const std::uint8_t* alpha, int alpha_stride, gray value) {
  const auto area = geometry::clamp({x, y, width, height}, display.width, display.height);
  const auto color = static_cast<std::uint8_t>(value);
  for (int py = area.y; py < area.y + area.h; ++py) {
    for (int px = area.x; px < area.x + area.w; ++px) {
      const auto coverage = alpha[(py - y) * alpha_stride + (px - x)];
      if (coverage == 0) continue;
      if (coverage == 255) {
        pixel(display, px, py, value);
      } else {
        const auto background = get_pixel(display, px, py);
        pixel(display, px, py,
              static_cast<gray>(static_cast<std::uint8_t>(
                  (background * (255 - coverage) + color * coverage) / 255)));
      }
    }
  }
}

/** @brief Fills a rectangle with a grayscale value. */
inline void fill(display::device& display, geometry::rect area, gray value) {
  area = geometry::clamp(area, display.width, display.height);
  for (int y = area.y; y < area.y + area.h; ++y) {
    for (int x = area.x; x < area.x + area.w; ++x) {
      pixel(display, x, y, value);
    }
  }
}

/** @brief Draws a rectangle border. */
inline void border(display::device& display, geometry::rect area, int thickness, gray value) {
  fill(display, {area.x, area.y, area.w, thickness}, value);
  fill(display, {area.x, area.y + area.h - thickness, area.w, thickness}, value);
  fill(display, {area.x, area.y, thickness, area.h}, value);
  fill(display, {area.x + area.w - thickness, area.y, thickness, area.h}, value);
}

/** @brief Draws a horizontal line. */
inline void hline(display::device& display, int x, int y, int width, gray value) {
  fill(display, {x, y, width, 1}, value);
}

/** @brief Draws a vertical line. */
inline void vline(display::device& display, int x, int y, int height, gray value) {
  fill(display, {x, y, 1, height}, value);
}

/** @brief Draws a 50 percent checkerboard fill useful for e-ink shadows. */
inline void stipple(display::device& display, geometry::rect area, gray value) {
  area = geometry::clamp(area, display.width, display.height);
  for (int y = area.y; y < area.y + area.h; ++y) {
    for (int x = area.x; x < area.x + area.w; ++x) {
      if (((x + y) & 1) == 0) pixel(display, x, y, value);
    }
  }
}

}  // namespace canvas
