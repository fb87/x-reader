#include "ota.hpp"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "esp_app_desc.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "services/version.hpp"

namespace xreader
{
namespace services
{
namespace ota
{
namespace
{
static const char* const tag = "xreader_ota";
static state_t state = {};
static bool initialized = false;

static void copy_text(char* destination, size_t capacity, const char* source)
{
    if (destination == nullptr || capacity == 0)
        return;
    snprintf(destination, capacity, "%s", source == nullptr ? "" : source);
}

static void persist_result(const char* text)
{
    copy_text(state.last_result, sizeof(state.last_result), text);
    nvs_handle_t handle = 0;
    if (nvs_open("xreader_ota", NVS_READWRITE, &handle) == ESP_OK)
    {
        nvs_set_str(handle, "result", state.last_result);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static bool json_string(const char* json, const char* key, char* output, size_t output_size)
{
    if (json == nullptr || key == nullptr || output == nullptr || output_size == 0)
        return false;
    char needle[48] = {};
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* start = strstr(json, needle);
    if (start == nullptr)
        return false;
    start = strchr(start + strlen(needle), ':');
    if (start == nullptr)
        return false;
    start = strchr(start, '"');
    if (start == nullptr)
        return false;
    ++start;
    const char* end = strchr(start, '"');
    if (end == nullptr)
        return false;
    const size_t length = static_cast<size_t>(end - start);
    if (length >= output_size)
        return false;
    memcpy(output, start, length);
    output[length] = '\0';
    return true;
}

static esp_err_t ota_http_event(esp_http_client_event_t* event)
{
    if (event == nullptr)
        return ESP_OK;
    if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key != nullptr &&
        event->header_value != nullptr && strcasecmp(event->header_key, "Content-Length") == 0)
    {
        const unsigned long total = strtoul(event->header_value, nullptr, 10);
        if (total <= UINT32_MAX)
            state.bytes_total = static_cast<uint32_t>(total);
    }
    else if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0)
    {
        state.bytes_downloaded += static_cast<uint32_t>(event->data_len);
        if (state.bytes_total > 0U)
        {
            const uint32_t percent = (state.bytes_downloaded * 100U) / state.bytes_total;
            state.progress_percent = static_cast<uint8_t>(percent > 99U ? 99U : percent);
        }
    }
    return ESP_OK;
}

static esp_err_t load_manifest_url()
{
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open("xreader_ota", NVS_READONLY, &handle);
    if (error != ESP_OK)
        return error;
    size_t length = sizeof(state.manifest_url);
    error = nvs_get_str(handle, "manifest", state.manifest_url, &length);
    nvs_close(handle);
    return error;
}

static esp_err_t fetch_manifest()
{
    if (state.manifest_url[0] == '\0')
        return ESP_ERR_NOT_FOUND;

    esp_http_client_config_t config = {};
    config.url = state.manifest_url;
    config.timeout_ms = 8000;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr)
        return ESP_ERR_NO_MEM;
    esp_err_t error = esp_http_client_open(client, 0);
    if (error != ESP_OK)
    {
        esp_http_client_cleanup(client);
        return error;
    }
    const int64_t content_length = esp_http_client_fetch_headers(client);
    if (content_length > 4095)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_SIZE;
    }
    char buffer[4096] = {};
    int total = 0;
    while (total < static_cast<int>(sizeof(buffer) - 1U))
    {
        const int read = esp_http_client_read(client, buffer + total,
                                              static_cast<int>(sizeof(buffer) - 1U) - total);
        if (read < 0)
        {
            error = ESP_FAIL;
            break;
        }
        if (read == 0)
            break;
        total += read;
    }
    buffer[total] = '\0';
    const int status_code = esp_http_client_get_status_code(client);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (error != ESP_OK)
        return error;
    if (status_code < 200 || status_code >= 300)
        return ESP_FAIL;

    char version_text[sizeof(state.available_version)] = {};
    char firmware[sizeof(state.firmware_url)] = {};
    char notes[sizeof(state.release_notes)] = {};
    if (!json_string(buffer, "version", version_text, sizeof(version_text)) ||
        !json_string(buffer, "url", firmware, sizeof(firmware)))
        return ESP_ERR_INVALID_RESPONSE;
    (void)json_string(buffer, "notes", notes, sizeof(notes));
    copy_text(state.available_version, sizeof(state.available_version), version_text);
    copy_text(state.firmware_url, sizeof(state.firmware_url), firmware);
    copy_text(state.release_notes, sizeof(state.release_notes), notes);
    state.update_available = version::compare(state.current_version, state.available_version) < 0;
    return ESP_OK;
}

static void check_task(void*)
{
    const esp_err_t error = fetch_manifest();
    state.last_error = error;
    if (error == ESP_OK)
    {
        state.phase = state.update_available ? phase_ready : phase_idle;
        persist_result(state.update_available ? "UPDATE AVAILABLE" : "UP TO DATE");
    }
    else
    {
        state.phase = phase_error;
        char result[96] = {};
        snprintf(result, sizeof(result), "CHECK FAILED: %s", esp_err_to_name(error));
        persist_result(result);
    }
    ESP_LOGI(tag, "check complete: %s", esp_err_to_name(error));
    vTaskDelete(nullptr);
}

static void install_task(void*)
{
    if (state.firmware_url[0] == '\0')
    {
        state.last_error = ESP_ERR_NOT_FOUND;
        persist_result("INSTALL FAILED: NO FIRMWARE URL");
        state.phase = phase_error;
        vTaskDelete(nullptr);
        return;
    }

    esp_err_t error = ESP_FAIL;
    for (uint8_t attempt = 0; attempt < 3U; ++attempt)
    {
        state.bytes_downloaded = 0U;
        state.bytes_total = 0U;
        state.progress_percent = 0U;

        esp_http_client_config_t http_config = {};
        http_config.url = state.firmware_url;
        http_config.timeout_ms = 15000;
        http_config.event_handler = ota_http_event;
        esp_https_ota_config_t ota_config = {};
        ota_config.http_config = &http_config;
        error = esp_https_ota(&ota_config);
        if (error == ESP_OK)
            break;
        ESP_LOGW(tag, "OTA attempt %u failed: %s", static_cast<unsigned>(attempt + 1U),
                 esp_err_to_name(error));
        if (attempt < 2U)
            vTaskDelay(pdMS_TO_TICKS(1500U));
    }

    state.last_error = error;
    if (error == ESP_OK)
    {
        state.progress_percent = 100U;
        char result[96] = {};
        snprintf(result, sizeof(result), "INSTALLED %s",
                 state.available_version[0] == '\0' ? "UPDATE" : state.available_version);
        persist_result(result);
        ESP_LOGI(tag, "OTA complete, restarting");
        esp_restart();
    }

    char result[96] = {};
    snprintf(result, sizeof(result), "INSTALL FAILED: %s", esp_err_to_name(error));
    persist_result(result);
    state.phase = phase_error;
    vTaskDelete(nullptr);
}
} // namespace

esp_err_t init()
{
    if (initialized)
        return ESP_OK;
    const esp_app_desc_t* description = esp_app_get_description();
    copy_text(state.current_version, sizeof(state.current_version),
              description == nullptr ? "UNKNOWN" : description->version);
    state.phase = phase_idle;
    state.last_error = ESP_OK;
    if (load_manifest_url() != ESP_OK && CONFIG_XREADER_OTA_MANIFEST_URL[0] != '\0')
        copy_text(state.manifest_url, sizeof(state.manifest_url), CONFIG_XREADER_OTA_MANIFEST_URL);

    nvs_handle_t handle = 0;
    if (nvs_open("xreader_ota", NVS_READONLY, &handle) == ESP_OK)
    {
        size_t length = sizeof(state.last_result);
        nvs_get_str(handle, "result", state.last_result, &length);
        nvs_close(handle);
    }
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t image_state = ESP_OTA_IMG_UNDEFINED;
    if (running != nullptr && esp_ota_get_state_partition(running, &image_state) == ESP_OK)
        state.rollback_pending = image_state == ESP_OTA_IMG_PENDING_VERIFY;

    initialized = true;
    return ESP_OK;
}

esp_err_t configure_manifest(const char* url)
{
    if (url == nullptr || strlen(url) >= sizeof(state.manifest_url))
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open("xreader_ota", NVS_READWRITE, &handle);
    if (error != ESP_OK)
        return error;
    error = nvs_set_str(handle, "manifest", url);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    if (error == ESP_OK)
        copy_text(state.manifest_url, sizeof(state.manifest_url), url);
    return error;
}

esp_err_t request_check()
{
    init();
    if (state.phase == phase_checking || state.phase == phase_installing)
        return ESP_ERR_INVALID_STATE;
    if (state.manifest_url[0] == '\0')
        return ESP_ERR_NOT_FOUND;
    state.phase = phase_checking;
    state.last_error = ESP_OK;
    if (xTaskCreate(check_task, "xreader_ota_check", 6144, nullptr, 4, nullptr) != pdPASS)
    {
        state.phase = phase_error;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t request_install()
{
    init();
    if (!state.update_available || state.firmware_url[0] == '\0')
        return ESP_ERR_INVALID_STATE;
    if (state.phase == phase_checking || state.phase == phase_installing)
        return ESP_ERR_INVALID_STATE;
    state.phase = phase_installing;
    state.progress_percent = 0U;
    state.bytes_downloaded = 0U;
    state.bytes_total = 0U;
    if (xTaskCreate(install_task, "xreader_ota_install", 8192, nullptr, 4, nullptr) != pdPASS)
    {
        state.phase = phase_error;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t confirm_running_image()
{
    init();
    if (!state.rollback_pending)
        return ESP_OK;
    const esp_err_t error = esp_ota_mark_app_valid_cancel_rollback();
    if (error == ESP_OK)
    {
        state.rollback_pending = false;
        persist_result("BOOT VERIFIED");
    }
    return error;
}

esp_err_t request_rollback()
{
    init();
    persist_result("ROLLBACK REQUESTED");
    return esp_ota_mark_app_invalid_rollback_and_reboot();
}

state_t snapshot()
{
    init();
    return state;
}

} // namespace ota
} // namespace services
} // namespace xreader
