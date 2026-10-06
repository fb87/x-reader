#pragma once

#include "core/canvas.hpp"

namespace widget {

/** @brief Shared visual style for the native stock widgets. */
struct style {
  canvas::gray background = canvas::gray::white;
  canvas::gray foreground = canvas::gray::black;
  canvas::gray secondary = canvas::gray::dark;
  canvas::gray focus_background = canvas::gray::black;
  canvas::gray focus_foreground = canvas::gray::white;
  canvas::gray divider = canvas::gray::light;
  int border_width = 2;
  int text_scale = 2;
};

}  // namespace widget
