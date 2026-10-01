#pragma once
#include "gfx/framebuffer.hpp"
#include <stdint.h>
namespace xreader
{
namespace ui
{
enum book_action_item_t : uint8_t
{
    book_action_open,
    book_action_rename,
    book_action_delete,
    book_action_back,
    book_action_item_count
};
void draw_book_actions(gfx::framebuffer_t* framebuffer, book_action_item_t focus,
                       const char* title, int8_t footer_focus);
bool book_actions_touch_item(uint16_t width, uint16_t height, uint16_t x, uint16_t y,
                             book_action_item_t* item);
} // namespace ui
} // namespace xreader
