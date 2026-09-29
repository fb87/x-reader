#pragma once

#include <stdint.h>

#include "esp_err.h"

namespace xreader
{
namespace services
{
namespace connectivity
{

enum phase_t : uint8_t
{
    phase_off,
    phase_idle,
    phase_scanning,
    phase_connecting,
    phase_connected,
    phase_error,
};

enum failure_t : uint8_t
{
    failure_none,
    failure_auth,
    failure_not_found,
    failure_timeout,
    failure_disconnected,
    failure_driver,
};

struct scan_result_t
{
    char ssid[33];
    int8_t rssi;
    bool secured;
};

static constexpr uint8_t max_scan_results = 12;

struct state_t
{
    bool initialized;
    bool enabled;
    bool connected;
    phase_t phase;
    int8_t rssi;
    char ssid[33];
    char ip[16];
    esp_err_t last_error;
    failure_t failure;
    uint8_t disconnect_reason;
    uint8_t reconnect_attempts;
    uint8_t scan_count;
    uint32_t scan_generation;
};

esp_err_t init();
esp_err_t set_enabled(bool enabled);
esp_err_t reconnect();
esp_err_t cancel_connect();
esp_err_t request_scan();
uint8_t scan_results(scan_result_t* results, uint8_t capacity);
esp_err_t forget_network();
esp_err_t configure(const char* ssid, const char* password);
void poll();
const char* status_text(const state_t& value);
state_t snapshot();

} // namespace connectivity
} // namespace services
} // namespace xreader
