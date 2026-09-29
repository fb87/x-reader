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
    esp_err_t last_error;
};

esp_err_t init();
esp_err_t configure_manifest(const char* url);
esp_err_t request_check();
esp_err_t request_install();
state_t snapshot();

} // namespace ota
} // namespace services
} // namespace xreader
