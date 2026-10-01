#pragma once
#include "gfx/framebuffer.hpp"
#include <stdint.h>
namespace xreader
{
namespace ui
{
enum book_sync_item_t : uint8_t
{
    book_sync_now,
    book_sync_books,
    book_sync_progress,
    book_sync_server,
    book_sync_history,
    book_sync_back,
    book_sync_item_count
};
void draw_book_sync(gfx::framebuffer_t* framebuffer, book_sync_item_t focus, bool connected,
                    bool sync_books, bool sync_progress, bool server_configured,
                    const char* last_sync, const char* activity, uint16_t completed,
                    uint16_t total, bool pending_retry, uint8_t retry_count,
                    const char* last_result, uint32_t bytes_downloaded, uint32_t bytes_total,
                    const char* history1, const char* history2, int8_t footer_focus);
bool book_sync_touch_item(uint16_t width, uint16_t height, uint16_t x, uint16_t y,
                          book_sync_item_t* item);
} // namespace ui
} // namespace xreader
