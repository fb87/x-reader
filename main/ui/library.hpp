#pragma once

#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{

void draw_library(gfx::framebuffer_t* framebuffer, bool storage_mounted, const char* mount_path);
bool find_first_book(const char* mount_path, char* path, size_t capacity);

} // namespace ui
} // namespace xreader
