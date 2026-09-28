#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{
namespace chrome
{

uint16_t status_height(const gfx::framebuffer_t* framebuffer);
uint16_t indication_height(const gfx::framebuffer_t* framebuffer);
void draw_status_bar(gfx::framebuffer_t* framebuffer, const char* left, const char* right);
void draw_indication_bar(gfx::framebuffer_t* framebuffer, const char* left, const char* center,
                         const char* right);

} // namespace chrome
} // namespace ui
} // namespace xreader
