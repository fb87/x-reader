#pragma once

#include <cstddef>
#include <cstdint>

#include "core/geometry.hpp"
#include "core/refresh.hpp"

namespace display {

/** @brief Pixel formats supported by the generic framebuffer interface. */
enum class pixel_format {
  mono1,
  gray4,
  gray8,
};

/** @brief Generic display interface supplied by a board implementation. */
struct device {
  int width;
  int height;
  int stride;
  pixel_format format;
  std::uint8_t* framebuffer;
  void* context;
  void (*update)(device& self, geometry::rect area, refresh::mode mode);
};

/** @brief Requests a display update when an update callback is available. */
inline void update(device& self, geometry::rect area, refresh::mode mode) {
  if (self.update == nullptr) {
    return;
  }
  self.update(self, geometry::clamp(area, self.width, self.height), mode);
}

}  // namespace display
