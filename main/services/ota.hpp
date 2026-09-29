#pragma once

#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace services
{
namespace ota
{

enum phase_t : uint8_t
{
    phase_idle,
    phase_checking,
    phase_ready,
    phase_installing,
    phase_error,
};

struct state_t
{
    phase_t phase;
    bool update_available;
    char current_version[32];
    char available_version[32];
    char manifest_url[192];
    char firmware_url[192];
    char release_notes[160];
    uint8_t progress_percent;
    uint32_t bytes_downloaded;
    uint32_t bytes_total;
    esp_err_t last_error;
    char last_result[96];
    bool rollback_pending;
};

esp_err_t init();
esp_err_t configure_manifest(const char* url);
esp_err_t request_check();
esp_err_t request_install();
esp_err_t confirm_running_image();
esp_err_t request_rollback();
state_t snapshot();

} // namespace ota
} // namespace services
} // namespace xreader
