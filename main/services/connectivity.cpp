#include "connectivity.hpp"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "sdkconfig.h"

namespace xreader
{
namespace services
{
namespace connectivity
{
namespace
{
static const char* const tag = "xreader_net";
static state_t state = {};
static esp_netif_t* station_netif = nullptr;
static bool wifi_started = false;
static bool handlers_registered = false;
static scan_result_t scans[max_scan_results] = {};
static int64_t connect_started_us = 0;
static constexpr int64_t connect_timeout_us = 15LL * 1000LL * 1000LL;
static constexpr uint8_t max_reconnect_attempts = 3;
static int64_t last_rssi_refresh_us = 0;
static constexpr int64_t rssi_refresh_us = 5LL * 1000LL * 1000LL;

static failure_t failure_for_reason(uint8_t reason)
{
    switch (reason)
    {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return failure_auth;
    case WIFI_REASON_NO_AP_FOUND:
        return failure_not_found;
    default:
        return failure_disconnected;
    }
}

static void begin_connect()
{
    state.phase = phase_connecting;
    state.failure = failure_none;
    state.disconnect_reason = 0;
    state.last_error = ESP_OK;
    connect_started_us = esp_timer_get_time();
}

static void copy_text(char* destination, size_t capacity, const char* source)
{
    if (destination == nullptr || capacity == 0)
        return;
    if (source == nullptr)
    {
        destination[0] = '\0';
        return;
    }
    snprintf(destination, capacity, "%s", source);
}

static esp_err_t read_credentials(char* ssid, size_t ssid_size, char* password,
                                  size_t password_size)
{
    if (ssid == nullptr || password == nullptr)
        return ESP_ERR_INVALID_ARG;
    ssid[0] = '\0';
    password[0] = '\0';
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open("xreader_net", NVS_READONLY, &handle);
    if (error != ESP_OK)
        return error;
    size_t length = ssid_size;
    error = nvs_get_str(handle, "ssid", ssid, &length);
    if (error == ESP_OK)
    {
        length = password_size;
        const esp_err_t password_error = nvs_get_str(handle, "password", password, &length);
        if (password_error != ESP_OK && password_error != ESP_ERR_NVS_NOT_FOUND)
            error = password_error;
    }
    nvs_close(handle);
    return error;
}

static void apply_credentials(const char* ssid, const char* password)
{
    wifi_config_t config = {};
    if (ssid != nullptr)
        snprintf(reinterpret_cast<char*>(config.sta.ssid), sizeof(config.sta.ssid), "%s", ssid);
    if (password != nullptr)
        snprintf(reinterpret_cast<char*>(config.sta.password), sizeof(config.sta.password), "%s",
                 password);
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    const esp_err_t error = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (error != ESP_OK)
    {
        state.phase = phase_error;
        state.failure = failure_driver;
        state.last_error = error;
        return;
    }
    copy_text(state.ssid, sizeof(state.ssid), ssid);
}

static void event_handler(void*, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT)
    {
        if (id == WIFI_EVENT_STA_START)
        {
            state.phase = state.enabled ? phase_idle : phase_off;
            if (state.enabled && state.ssid[0] != '\0')
            {
                begin_connect();
                const esp_err_t error = esp_wifi_connect();
                if (error != ESP_OK)
                {
                    state.phase = phase_error;
                    state.failure = failure_driver;
                    state.last_error = error;
                }
            }
        }
        else if (id == WIFI_EVENT_SCAN_DONE)
        {
            uint16_t count = max_scan_results;
            wifi_ap_record_t records[max_scan_results] = {};
            if (esp_wifi_scan_get_ap_records(&count, records) == ESP_OK)
            {
                state.scan_count =
                    static_cast<uint8_t>(count > max_scan_results ? max_scan_results : count);
                for (uint8_t index = 0; index < state.scan_count; ++index)
                {
                    snprintf(scans[index].ssid, sizeof(scans[index].ssid), "%s",
                             reinterpret_cast<const char*>(records[index].ssid));
                    scans[index].rssi = records[index].rssi;
                    scans[index].secured = records[index].authmode != WIFI_AUTH_OPEN;
                }
                ++state.scan_generation;
                state.phase = state.connected ? phase_connected : phase_idle;
                state.last_error = ESP_OK;
            }
            else
            {
                state.phase = phase_error;
            }
        }
        else if (id == WIFI_EVENT_STA_DISCONNECTED)
        {
            const auto* event = static_cast<const wifi_event_sta_disconnected_t*>(data);
            const uint8_t reason = event == nullptr ? 0U : event->reason;
            const bool was_connected = state.connected;
            state.connected = false;
            state.ip[0] = '\0';
            state.disconnect_reason = reason;

            if (!state.enabled)
            {
                state.phase = phase_off;
                state.failure = failure_none;
            }
            else if (state.phase == phase_error && state.failure == failure_timeout)
            {
                // poll() intentionally disconnected after the connection timeout.
            }
            else if (!was_connected && state.phase == phase_connecting)
            {
                state.phase = phase_error;
                state.failure = failure_for_reason(reason);
            }
            else if (state.ssid[0] != '\0' && state.reconnect_attempts < max_reconnect_attempts)
            {
                ++state.reconnect_attempts;
                begin_connect();
                const esp_err_t error = esp_wifi_connect();
                if (error != ESP_OK)
                {
                    state.phase = phase_error;
                    state.failure = failure_driver;
                    state.last_error = error;
                }
            }
            else
            {
                state.phase = phase_error;
                state.failure = failure_for_reason(reason);
            }
        }
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        const auto* event = static_cast<const ip_event_got_ip_t*>(data);
        state.connected = true;
        state.phase = phase_connected;
        state.failure = failure_none;
        state.reconnect_attempts = 0;
        state.last_error = ESP_OK;
        connect_started_us = 0;
        if (event != nullptr)
        {
            snprintf(state.ip, sizeof(state.ip), IPSTR, IP2STR(&event->ip_info.ip));
        }
        wifi_ap_record_t record = {};
        if (esp_wifi_sta_get_ap_info(&record) == ESP_OK)
            state.rssi = record.rssi;
    }
}
} // namespace

esp_err_t init()
{
    if (state.initialized)
        return ESP_OK;

    esp_err_t error = esp_netif_init();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
        return error;
    error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
        return error;

    station_netif = esp_netif_create_default_wifi_sta();
    if (station_netif == nullptr)
        return ESP_ERR_NO_MEM;

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    error = esp_wifi_init(&init_config);
    if (error != ESP_OK)
        return error;

    if (!handlers_registered)
    {
        error = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr);
        if (error != ESP_OK)
            return error;
        error = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr);
        if (error != ESP_OK)
            return error;
        handlers_registered = true;
    }

    char ssid[33] = {};
    char password[65] = {};
    if (read_credentials(ssid, sizeof(ssid), password, sizeof(password)) == ESP_OK &&
        ssid[0] != '\0')
        apply_credentials(ssid, password);
    else if (CONFIG_XREADER_WIFI_SSID[0] != '\0')
        apply_credentials(CONFIG_XREADER_WIFI_SSID, CONFIG_XREADER_WIFI_PASSWORD);

    state.initialized = true;
    state.enabled = false;
    state.connected = false;
    state.phase = phase_off;
    state.failure = failure_none;
    state.disconnect_reason = 0;
    state.reconnect_attempts = 0;
    state.last_error = ESP_OK;
    ESP_LOGI(tag, "initialized%s", state.ssid[0] != '\0' ? " with saved network" : "");
    return ESP_OK;
}

esp_err_t set_enabled(bool enabled)
{
    if (!state.initialized)
    {
        const esp_err_t init_error = init();
        if (init_error != ESP_OK)
            return init_error;
    }
    if (enabled == state.enabled)
        return ESP_OK;

    state.enabled = enabled;
    if (enabled)
    {
        esp_err_t error = esp_wifi_set_mode(WIFI_MODE_STA);
        if (error != ESP_OK)
            return error;
        if (!wifi_started)
        {
            error = esp_wifi_start();
            if (error != ESP_OK)
                return error;
            wifi_started = true;
        }
        state.phase = phase_idle;
        if (state.ssid[0] != '\0')
        {
            begin_connect();
            error = esp_wifi_connect();
            if (error != ESP_OK)
            {
                state.phase = phase_error;
                state.failure = failure_driver;
            }
        }
        state.last_error = error;
        return error;
    }

    state.connected = false;
    state.ip[0] = '\0';
    state.phase = phase_off;
    state.failure = failure_none;
    state.reconnect_attempts = 0;
    connect_started_us = 0;
    if (wifi_started)
    {
        esp_wifi_disconnect();
        const esp_err_t stop_error = esp_wifi_stop();
        if (stop_error != ESP_OK)
            return stop_error;
        wifi_started = false;
    }
    return ESP_OK;
}

esp_err_t cancel_connect()
{
    if (!state.initialized || !state.enabled)
        return ESP_ERR_INVALID_STATE;
    connect_started_us = 0;
    state.reconnect_attempts = 0;
    state.connected = false;
    state.ip[0] = '\0';
    state.phase = phase_idle;
    state.failure = failure_none;
    state.last_error = ESP_OK;
    return esp_wifi_disconnect();
}

esp_err_t reconnect()
{
    if (!state.enabled)
        return ESP_ERR_INVALID_STATE;
    if (state.ssid[0] == '\0')
        return ESP_ERR_NOT_FOUND;
    begin_connect();
    const esp_err_t error = esp_wifi_connect();
    state.last_error = error;
    if (error != ESP_OK)
    {
        state.phase = phase_error;
        state.failure = failure_driver;
    }
    return error;
}

esp_err_t request_scan()
{
    if (!state.initialized)
    {
        const esp_err_t init_error = init();
        if (init_error != ESP_OK)
            return init_error;
    }
    if (!state.enabled)
    {
        const esp_err_t enable_error = set_enabled(true);
        if (enable_error != ESP_OK)
            return enable_error;
    }
    wifi_scan_config_t config = {};
    config.show_hidden = true;
    state.phase = phase_scanning;
    const esp_err_t error = esp_wifi_scan_start(&config, false);
    state.last_error = error;
    if (error != ESP_OK)
        state.phase = phase_error;
    return error;
}

uint8_t scan_results(scan_result_t* results, uint8_t capacity)
{
    if (results == nullptr || capacity == 0)
        return 0;
    const uint8_t count = state.scan_count < capacity ? state.scan_count : capacity;
    for (uint8_t index = 0; index < count; ++index)
        results[index] = scans[index];
    return count;
}

esp_err_t forget_network()
{
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open("xreader_net", NVS_READWRITE, &handle);
    if (error != ESP_OK)
        return error;
    error = nvs_erase_key(handle, "ssid");
    if (error == ESP_ERR_NVS_NOT_FOUND)
        error = ESP_OK;
    const esp_err_t password_error = nvs_erase_key(handle, "password");
    if (error == ESP_OK && password_error != ESP_OK && password_error != ESP_ERR_NVS_NOT_FOUND)
        error = password_error;
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    if (error != ESP_OK)
        return error;
    if (state.enabled)
        esp_wifi_disconnect();
    state.connected = false;
    state.ssid[0] = '\0';
    state.ip[0] = '\0';
    state.phase = state.enabled ? phase_idle : phase_off;
    state.failure = failure_none;
    state.reconnect_attempts = 0;
    connect_started_us = 0;
    return ESP_OK;
}

esp_err_t configure(const char* ssid, const char* password)
{
    if (ssid == nullptr || ssid[0] == '\0' || strlen(ssid) > 32 ||
        (password != nullptr && strlen(password) > 64))
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open("xreader_net", NVS_READWRITE, &handle);
    if (error != ESP_OK)
        return error;
    error = nvs_set_str(handle, "ssid", ssid);
    if (error == ESP_OK)
        error = nvs_set_str(handle, "password", password == nullptr ? "" : password);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    if (error != ESP_OK)
        return error;

    if (!state.initialized)
    {
        error = init();
        if (error != ESP_OK)
            return error;
    }
    apply_credentials(ssid, password == nullptr ? "" : password);
    if (state.enabled)
        return reconnect();
    return ESP_OK;
}

void poll()
{
    if (!state.initialized)
        return;
    const int64_t now = esp_timer_get_time();
    if (state.connected &&
        (last_rssi_refresh_us == 0 || now - last_rssi_refresh_us >= rssi_refresh_us))
    {
        wifi_ap_record_t record = {};
        if (esp_wifi_sta_get_ap_info(&record) == ESP_OK)
            state.rssi = record.rssi;
        last_rssi_refresh_us = now;
    }
    if (state.phase != phase_connecting || connect_started_us == 0)
        return;
    if (now - connect_started_us < connect_timeout_us)
        return;

    ESP_LOGW(tag, "Wi-Fi connection timed out for %s", state.ssid);
    state.connected = false;
    state.ip[0] = '\0';
    state.phase = phase_error;
    state.failure = failure_timeout;
    state.last_error = ESP_ERR_TIMEOUT;
    connect_started_us = 0;
    esp_wifi_disconnect();
}

const char* status_text(const state_t& value)
{
    switch (value.phase)
    {
    case phase_off:
        return "WI-FI OFF";
    case phase_scanning:
        return "SCANNING...";
    case phase_connecting:
        return "CONNECTING...";
    case phase_connected:
        return "CONNECTED";
    case phase_error:
        switch (value.failure)
        {
        case failure_auth:
            return "WRONG PASSWORD";
        case failure_not_found:
            return "NETWORK NOT FOUND";
        case failure_timeout:
            return "CONNECTION TIMEOUT";
        case failure_disconnected:
            return "DISCONNECTED";
        case failure_driver:
            return "WI-FI ERROR";
        default:
            return "CONNECTION ERROR";
        }
    case phase_idle:
    default:
        return "READY";
    }
}

state_t snapshot()
{
    return state;
}

} // namespace connectivity
} // namespace services
} // namespace xreader
