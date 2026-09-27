#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{
namespace chrome
{

static constexpr uint16_t status_height = 36;
static constexpr uint16_t indication_height = 36;

void draw_status_bar(gfx::framebuffer_t* framebuffer, const char* left, const char* right);
void draw_indication_bar(gfx::framebuffer_t* framebuffer, const char* left, const char* center,
                         const char* right);

} // namespace chrome
} // namespace ui
} // namespace xreader
