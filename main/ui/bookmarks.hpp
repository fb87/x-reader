#pragma once
#include "gfx/framebuffer.hpp"
namespace xreader::ui {
struct bookmark_view_t { bool valid; uint8_t spine; uint8_t page; };
void draw_bookmarks(gfx::framebuffer_t* framebuffer, const bookmark_view_t* items, uint8_t count, uint8_t focus);
bool bookmarks_touch_index(uint16_t width, uint16_t height, uint16_t x, uint16_t y, uint8_t count, uint8_t* index);
}
