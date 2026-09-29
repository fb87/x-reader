#pragma once
#include "gfx/framebuffer.hpp"
#include <stdint.h>
namespace xreader
{
namespace ui
{
enum book_manager_item_t : uint8_t
{
    book_manager_library,
    book_manager_import,
    book_manager_storage,
    book_manager_cleanup,
    book_manager_back,
    book_manager_item_count
};
void draw_book_manager(gfx::framebuffer_t* framebuffer, book_manager_item_t focus,
                       uint16_t book_count, bool storage_mounted);
bool book_manager_touch_item(uint16_t width, uint16_t height, uint16_t x, uint16_t y,
                             book_manager_item_t* item);
} // namespace ui
} // namespace xreader
