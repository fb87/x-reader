#pragma once

#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace services
{
namespace book_sync
{

enum phase_t : uint8_t
{
    phase_idle,
    phase_syncing,
    phase_complete,
    phase_error,
};

struct state_t
{
    phase_t phase;
    bool sync_books;
    bool sync_progress;
    char server[192];
    char last_sync[32];
    esp_err_t last_error;
    uint16_t books_total;
    uint16_t books_completed;
    uint32_t bytes_downloaded;
    uint32_t bytes_total;
    char activity[64];
    bool pending_retry;
    uint8_t retry_count;
    char last_result[96];
    char history[4][80];
    uint8_t history_count;
};

esp_err_t init();
esp_err_t configure_server(const char* url);
void set_sync_books(bool enabled);
void set_sync_progress(bool enabled);
esp_err_t request_sync(const char* book_path, uint32_t spine, uint32_t page);
void poll(bool connected);
state_t snapshot();

} // namespace book_sync
} // namespace services
} // namespace xreader
