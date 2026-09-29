#include "book_sync.hpp"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "sdkconfig.h"

namespace xreader
{
namespace services
{
namespace book_sync
{
namespace
{
static const char* const tag = "xreader_sync";
static state_t state = {};
static bool initialized = false;

struct request_t
{
    char book_path[256];
    uint32_t spine;
    uint32_t page;
};

static void copy_text(char* destination, size_t capacity, const char* source)
{
    snprintf(destination, capacity, "%s", source == nullptr ? "" : source);
}

static const char* basename_of(const char* path)
{
    if (path == nullptr)
        return "";
    const char* slash = strrchr(path, '/');
    return slash == nullptr ? path : slash + 1;
}

static esp_err_t post_progress(const request_t* request)
{
    if (request == nullptr || state.server[0] == '\0')
        return ESP_ERR_INVALID_STATE;
    char url[256] = {};
    snprintf(url, sizeof(url), "%s/v1/progress", state.server);
    char body[512] = {};
    snprintf(body, sizeof(body), "{\"book\":\"%s\",\"spine\":%lu,\"page\":%lu}",
             basename_of(request->book_path), static_cast<unsigned long>(request->spine),
             static_cast<unsigned long>(request->page));

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 8000;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr)
        return ESP_ERR_NO_MEM;
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, static_cast<int>(strlen(body)));
    const esp_err_t error = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (error != ESP_OK)
        return error;
    return status >= 200 && status < 300 ? ESP_OK : ESP_FAIL;
}

static void update_last_sync()
{
    const time_t now = time(nullptr);
    struct tm time_info = {};
    localtime_r(&now, &time_info);
    if (time_info.tm_year >= 120)
        strftime(state.last_sync, sizeof(state.last_sync), "%Y-%m-%d %H:%M", &time_info);
    else
        copy_text(state.last_sync, sizeof(state.last_sync), "JUST NOW");
}

static void sync_task(void* argument)
{
    request_t* request = static_cast<request_t*>(argument);
    esp_err_t error = ESP_OK;
    if (state.sync_progress)
        error = post_progress(request);
    if (error == ESP_OK)
    {
        update_last_sync();
        state.phase = phase_complete;
    }
    else
        state.phase = phase_error;
    state.last_error = error;
    delete request;
    ESP_LOGI(tag, "sync complete: %s", esp_err_to_name(error));
    vTaskDelete(nullptr);
}

static void load_state()
{
    nvs_handle_t handle = 0;
    if (nvs_open("xreader_sync", NVS_READONLY, &handle) != ESP_OK)
        return;
    size_t length = sizeof(state.server);
    nvs_get_str(handle, "server", state.server, &length);
    uint8_t value = 1;
    if (nvs_get_u8(handle, "books", &value) == ESP_OK)
        state.sync_books = value != 0;
    value = 1;
    if (nvs_get_u8(handle, "progress", &value) == ESP_OK)
        state.sync_progress = value != 0;
    length = sizeof(state.last_sync);
    nvs_get_str(handle, "last", state.last_sync, &length);
    nvs_close(handle);
}

static void persist_flags()
{
    nvs_handle_t handle = 0;
    if (nvs_open("xreader_sync", NVS_READWRITE, &handle) != ESP_OK)
        return;
    nvs_set_u8(handle, "books", state.sync_books ? 1U : 0U);
    nvs_set_u8(handle, "progress", state.sync_progress ? 1U : 0U);
    if (state.last_sync[0] != '\0')
        nvs_set_str(handle, "last", state.last_sync);
    nvs_commit(handle);
    nvs_close(handle);
}
} // namespace

esp_err_t init()
{
    if (initialized)
        return ESP_OK;
    state.phase = phase_idle;
    state.sync_books = true;
    state.sync_progress = true;
    state.last_error = ESP_OK;
    load_state();
    if (state.server[0] == '\0' && CONFIG_XREADER_SYNC_SERVER[0] != '\0')
        copy_text(state.server, sizeof(state.server), CONFIG_XREADER_SYNC_SERVER);
    initialized = true;
    return ESP_OK;
}

esp_err_t configure_server(const char* url)
{
    init();
    if (url == nullptr || strlen(url) >= sizeof(state.server))
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open("xreader_sync", NVS_READWRITE, &handle);
    if (error != ESP_OK)
        return error;
    error = nvs_set_str(handle, "server", url);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    if (error == ESP_OK)
        copy_text(state.server, sizeof(state.server), url);
    return error;
}

void set_sync_books(bool enabled)
{
    init();
    state.sync_books = enabled;
    persist_flags();
}

void set_sync_progress(bool enabled)
{
    init();
    state.sync_progress = enabled;
    persist_flags();
}

esp_err_t request_sync(const char* book_path, uint32_t spine, uint32_t page)
{
    init();
    if (state.server[0] == '\0')
        return ESP_ERR_NOT_FOUND;
    if (state.phase == phase_syncing)
        return ESP_ERR_INVALID_STATE;
    request_t* request = new request_t{};
    if (request == nullptr)
        return ESP_ERR_NO_MEM;
    copy_text(request->book_path, sizeof(request->book_path), book_path);
    request->spine = spine;
    request->page = page;
    state.phase = phase_syncing;
    state.last_error = ESP_OK;
    if (xTaskCreate(sync_task, "xreader_sync", 6144, request, 4, nullptr) != pdPASS)
    {
        delete request;
        state.phase = phase_error;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

state_t snapshot()
{
    init();
    return state;
}

} // namespace book_sync
} // namespace services
} // namespace xreader
