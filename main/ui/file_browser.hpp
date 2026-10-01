#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"
#include "services/file_browser.hpp"

namespace xreader
{
namespace ui
{

void draw_file_browser(gfx::framebuffer_t* framebuffer,
                       const services::file_browser::listing_t* listing, uint8_t focus,
                       int8_t footer_focus);
bool file_browser_touch_index(uint16_t width, uint16_t height, uint16_t x, uint16_t y,
                              uint8_t count, uint8_t focus, uint8_t* index);

} // namespace ui
} // namespace xreader
