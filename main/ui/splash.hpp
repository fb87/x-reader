#pragma once

#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{

// Mockup 1.  Shown while the display/touch/SD stack comes up at boot, in place
// of whatever the panel happened to show before power-on (usually a stale
// frame from the previous session, since e-paper holds its image with the
// controller off).  `status` is a short line describing the current step
// ("Mounting SD card", "Loading library"); pass nullptr to omit it.
void draw_splash(gfx::framebuffer_t* framebuffer, const char* status);

} // namespace ui
} // namespace xreader
