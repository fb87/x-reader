#pragma once

#include "event.hpp"
#include "geometry.hpp"
#include "refresh.hpp"

#include <cstdint>

/**
 * @brief Generic framebuffer/display capability, ported from `xr_display_t`
 * (include/xr/xr_hal.h) and the pixel-format enum from `xr_pixfmt_t`
 * (include/xr/xr_canvas.h).
 *
 * A board owns the framebuffer memory and hands it to `core/canvas.hpp` for
 * drawing; this header only describes the panel shape and the "push this
 * rect with this waveform" operation. Mandatory capability: every board
 * must populate this (see core/capability.hpp).
 */
namespace display {

/** @brief Pixel formats supported by the generic framebuffer interface. */
enum class pixel_format {
  mono1,  ///< 1 bpp, MSB first, 1 = white (e.g. small SPI panels).
  gray4,  ///< 4 bpp, high nibble first (e.g. IT8951 / M5Paper).
  gray8,  ///< 8 bpp.
};

/** @brief Generic display interface supplied by a board implementation. */
struct device {
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::int32_t stride = 0;  ///< bytes per row.
  pixel_format format = pixel_format::gray4;
  std::uint8_t* framebuffer = nullptr;
  std::uint8_t update_align = 1;  ///< partial-update x alignment, e.g. 8 for 1bpp.
  void* context = nullptr;        ///< board-private.
  /** Pushes `area` of the framebuffer to the panel using `mode`. `refresh::mode::full` is
   * always called with the whole screen. */
  void (*update)(device& self, geometry::rect area, refresh::mode mode) = nullptr;
};

/** @brief Requests a display update, clamped to the panel bounds, when a backend is attached. */
inline void update(device& self, geometry::rect area, refresh::mode mode) {
  if (self.update == nullptr) return;
  self.update(self, geometry::clamp(area, self.width, self.height), mode);
}

}  // namespace display
