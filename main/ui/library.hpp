#pragma once

#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{

static constexpr size_t book_path_length = 512;

void draw_library(gfx::framebuffer_t* framebuffer, bool storage_mounted, const char* mount_path);
size_t find_books(const char* mount_path, char paths[][book_path_length], size_t capacity);
bool find_first_book(const char* mount_path, char* path, size_t capacity);

} // namespace ui
} // namespace xreader
