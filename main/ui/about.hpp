#pragma once
#include "gfx/framebuffer.hpp"
namespace xreader
{
namespace ui
{
void draw_about(gfx::framebuffer_t* framebuffer, const char* version, const char* board,
                const char* idf_version, const char* build_date);
}
} // namespace xreader
