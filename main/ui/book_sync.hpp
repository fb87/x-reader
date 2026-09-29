#pragma once
#include <stdint.h>
#include "gfx/framebuffer.hpp"
namespace xreader { namespace ui {
enum book_sync_item_t : uint8_t { book_sync_now, book_sync_books, book_sync_progress, book_sync_server, book_sync_history, book_sync_back, book_sync_item_count };
void draw_book_sync(gfx::framebuffer_t* framebuffer, book_sync_item_t focus, bool connected,
                    bool sync_books, bool sync_progress, bool server_configured,
                    const char* last_sync, const char* activity = nullptr,
                    uint16_t completed = 0, uint16_t total = 0, bool pending_retry = false,
                    uint8_t retry_count = 0, const char* last_result = nullptr,
                    uint32_t bytes_downloaded = 0, uint32_t bytes_total = 0,
                    const char* history1 = nullptr, const char* history2 = nullptr);
bool book_sync_touch_item(uint16_t width,uint16_t height,uint16_t x,uint16_t y,book_sync_item_t* item);
} }
