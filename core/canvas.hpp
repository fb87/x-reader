#pragma once

#include "display.hpp"
#include "geometry.hpp"

#include <cstdint>
#include <cstring>

/**
 * @brief Drawing into a framebuffer of any supported pixel format, ported
 * from `xr_canvas_t`/`xr_canvas_*` (src/xr_canvas.c, include/xr/xr_canvas.h).
 *
 * The canvas does not own memory: a board hands it the framebuffer. All
 * drawing clips to `canvas.clip`, which the shell sets to the dirty
 * rectangle being recomposed. Packing is preserved bit-for-bit from the
 * old implementation (GRAY4 high-nibble-first, MONO1 MSB-first with a
 * 4x4 Bayer dither for mid-grays) since both current panel profiles
 * (540x960 GRAY4, 480x800 MONO1) depend on exactly this layout.
 */
namespace canvas {

/** @brief 4x4 Bayer thresholds scaled to 0..255, used for gray fills on 1 bpp. */
inline constexpr std::uint8_t bayer4[4][4] = {
    {8, 136, 40, 168},
    {200, 72, 232, 104},
    {56, 184, 24, 152},
    {248, 120, 216, 88},
};

/** @brief Common e-ink gray levels (0 black .. 255 white); canvas quantizes to panel format. */
namespace gray {
inline constexpr std::uint8_t black = 0x00;
inline constexpr std::uint8_t dark = 0x22;
inline constexpr std::uint8_t light = 0xAA;
inline constexpr std::uint8_t white = 0xFF;
}  // namespace gray

/** @brief A drawable surface: a framebuffer view plus its current clip rect. */
struct surface {
  std::uint8_t* buf = nullptr;
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::int32_t stride = 0;  ///< bytes per row.
  display::pixel_format fmt = display::pixel_format::gray4;
  geometry::rect clip{};
};

/** @brief Bytes per row for `width` pixels at `fmt`. */
constexpr std::int32_t stride_for(int width, display::pixel_format fmt) {
  switch (fmt) {
    case display::pixel_format::mono1:
      return (width + 7) / 8;
    case display::pixel_format::gray4:
      return (width + 1) / 2;
    default:
      return width;
  }
}

/** @brief Minimum framebuffer size in bytes for a `width` x `height` surface at `fmt`. */
constexpr std::size_t buffer_size(int width, int height, display::pixel_format fmt) {
  return static_cast<std::size_t>(stride_for(width, fmt)) * static_cast<std::size_t>(height);
}

/** @brief Initializes a surface over caller-owned memory; `stride` of 0 derives it from `fmt`. */
inline void init(surface& c, void* buf, int width, int height, std::int32_t stride,
                 display::pixel_format fmt) {
  c.buf = static_cast<std::uint8_t*>(buf);
  c.width = static_cast<std::int16_t>(width);
  c.height = static_cast<std::int16_t>(height);
  c.stride = stride != 0 ? stride : stride_for(width, fmt);
  c.fmt = fmt;
  c.clip = geometry::make(0, 0, width, height);
}

/** @brief Sets the clip (intersected with the surface bounds); returns the previous one. */
inline geometry::rect set_clip(surface& c, geometry::rect clip) {
  const geometry::rect old = c.clip;
  c.clip = geometry::intersect(clip, geometry::make(0, 0, c.width, c.height));
  return old;
}

namespace detail {

/** @brief Writes one pixel; `dither` enables the Bayer pattern for MONO1 mid-grays. */
inline void put(surface& c, int x, int y, std::uint8_t g, bool dither) {
  std::uint8_t* row = c.buf + static_cast<std::int32_t>(y) * c.stride;
  switch (c.fmt) {
    case display::pixel_format::gray8:
      row[x] = g;
      break;
    case display::pixel_format::gray4: {
      const std::uint8_t v = g >> 4;
      std::uint8_t* p = &row[x >> 1];
      *p = (x & 1) ? static_cast<std::uint8_t>((*p & 0xF0) | v)
                   : static_cast<std::uint8_t>((*p & 0x0F) | (v << 4));
      break;
    }
    case display::pixel_format::mono1: {
      const std::uint8_t thr = dither ? bayer4[y & 3][x & 3] : 128;
      const std::uint8_t m = static_cast<std::uint8_t>(0x80 >> (x & 7));
      if (g >= thr) row[x >> 3] |= m;
      else row[x >> 3] &= static_cast<std::uint8_t>(~m);
      break;
    }
  }
}

}  // namespace detail

/** @brief Reads back the gray level of one pixel (quantized per the surface's pixel format). */
inline std::uint8_t get_pixel(const surface& c, int x, int y) {
  const std::uint8_t* row = c.buf + static_cast<std::int32_t>(y) * c.stride;
  switch (c.fmt) {
    case display::pixel_format::gray8:
      return row[x];
    case display::pixel_format::gray4: {
      const std::uint8_t p = row[x >> 1];
      const std::uint8_t v = (x & 1) ? (p & 0x0F) : (p >> 4);
      return static_cast<std::uint8_t>(v * 17);
    }
    case display::pixel_format::mono1:
      return (row[x >> 3] & (0x80 >> (x & 7))) ? 255 : 0;
  }
  return 255;
}

/** @brief Solid fill, clipped to the surface's clip rect. Mid-grays are dithered on 1 bpp. */
inline void fill(surface& c, geometry::rect r, std::uint8_t g) {
  r = geometry::intersect(r, c.clip);
  if (geometry::empty(r)) return;
  if (c.fmt == display::pixel_format::gray8) {
    for (int y = r.y; y < r.y + r.h; ++y) {
      std::memset(c.buf + static_cast<std::int32_t>(y) * c.stride + r.x, g,
                  static_cast<std::size_t>(r.w));
    }
    return;
  }
  for (int y = r.y; y < r.y + r.h; ++y) {
    for (int x = r.x; x < r.x + r.w; ++x) detail::put(c, x, y, g, true);
  }
}

/** @brief Fills a 1-pixel-tall horizontal strip. */
inline void hline(surface& c, int x, int y, int w, std::uint8_t g) {
  fill(c, geometry::make(x, y, w, 1), g);
}

/** @brief Fills a 1-pixel-wide vertical strip. */
inline void vline(surface& c, int x, int y, int h, std::uint8_t g) {
  fill(c, geometry::make(x, y, 1, h), g);
}

/** @brief Draws an unfilled rect outline of the given thickness. */
inline void border(surface& c, geometry::rect r, int t, std::uint8_t g) {
  if (t <= 0) return;
  fill(c, geometry::make(r.x, r.y, r.w, t), g);
  fill(c, geometry::make(r.x, r.y + r.h - t, r.w, t), g);
  fill(c, geometry::make(r.x, r.y + t, t, r.h - 2 * t), g);
  fill(c, geometry::make(r.x + r.w - t, r.y + t, t, r.h - 2 * t), g);
}

/** @brief 50% checkerboard fill: a cheap "shadow"/"dim" that reads correctly on every panel. */
inline void stipple(surface& c, geometry::rect r, std::uint8_t g) {
  r = geometry::intersect(r, c.clip);
  for (int y = r.y; y < r.y + r.h; ++y) {
    for (int x = r.x; x < r.x + r.w; ++x) {
      if (((x + y) & 1) == 0) detail::put(c, x, y, g, false);
    }
  }
}

/** @brief Blends an 8-bit alpha mask (glyphs, icons) in the given gray. */
inline void draw_mask(surface& c, int x, int y, int w, int h, const std::uint8_t* alpha,
                      int alpha_stride, std::uint8_t g) {
  const geometry::rect r = geometry::intersect(geometry::make(x, y, w, h), c.clip);
  for (int py = r.y; py < r.y + r.h; ++py) {
    const std::uint8_t* arow = alpha + (py - y) * alpha_stride;
    for (int px = r.x; px < r.x + r.w; ++px) {
      const std::uint8_t a = arow[px - x];
      if (a == 0) continue;
      if (c.fmt == display::pixel_format::mono1) {
        // No gray to blend into: keep thin strokes by inking at ~30% coverage
        // instead of thresholding the blend at 50%.
        if (a >= 80) detail::put(c, px, py, g, false);
        continue;
      }
      if (a == 255) {
        detail::put(c, px, py, g, false);
      } else {
        const std::uint8_t bg = get_pixel(c, px, py);
        const std::uint8_t out = static_cast<std::uint8_t>((bg * (255 - a) + g * a) / 255);
        detail::put(c, px, py, out, false);
      }
    }
  }
}

}  // namespace canvas
