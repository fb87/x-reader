#pragma once
#include "gfx/framebuffer.hpp"
#include <stdint.h>
namespace xreader
{
namespace ui
{
void draw_storage(gfx::framebuffer_t* framebuffer, bool mounted, uint64_t total_bytes,
                  uint64_t free_bytes, uint16_t books, const char* mount_path, int8_t footer_focus);
}
} // namespace xreader
