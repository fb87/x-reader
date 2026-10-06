#pragma once

#include "runtime.hpp"

/**
 * @brief A second simulator logical profile (480x800 MONO1, the Xteink
 * panel shape), reusing `board::sim::runtime` as-is rather than
 * duplicating it: no Xteink hardware driver exists anywhere today (old
 * tree or new), so this exists purely to exercise reader/app code
 * against a different width/height/pixel-format combination and catch
 * accidental GRAY4-only or 540-width-only assumptions that crept in
 * while the simulator profile was always M5Paper's.
 *
 * Deliberately NOT a separate runtime type: `board::sim::runtime`'s
 * framebuffer array is sized for 540x960 GRAY4 (259200 bytes), which
 * comfortably fits 480x800 MONO1 (48000 bytes), so reusing the same
 * type and overriding the display profile fields after init is safe
 * and avoids maintaining a second copy of every capability function.
 */
namespace board::sim_xteink {

inline constexpr int width = 480;
inline constexpr int height = 800;
inline constexpr auto pixel_format = display::pixel_format::mono1;

/** @brief Initializes a sim runtime, then overrides its display profile to Xteink's shape. */
inline void init(board::sim::runtime& self) {
  board::sim::init(self);
  self.display.width = width;
  self.display.height = height;
  self.display.format = pixel_format;
  self.display.stride = canvas::stride_for(width, pixel_format);
  self.display.update_align = 8;  // 1bpp SPI panels partial-update on 8px boundaries.
}

}  // namespace board::sim_xteink
