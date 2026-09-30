#include "book_sync.hpp"

#include "services/library_index.hpp"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "storage/persistence.hpp"

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
static constexpr uint8_t max_manifest_books = 32;
static constexpr size_t response_capacity = 16384;
static TickType_t retry_not_before = 0;
static constexpr TickType_t retry_delay = pdMS_TO_TICKS(30000U);

struct request_t
{
    char book_path[256];
    uint32_t spine;
    uint32_t page;
};

struct remote_book_t
{
    char name[128];
    char url[256];
    uint32_t size;
    char sha256[65];
};

static void copy_text(char* destination, size_t capacity, const char* source)
{
    if (destination == nullptr || capacity == 0)
        return;
    snprintf(destination, capacity, "%s", source == nullptr ? "" : source);
}

static const char* basename_of(const char* path)
{
    if (path == nullptr)
        return "";
    const char* slash = strrchr(path, '/');
    return slash == nullptr ? path : slash + 1;
}

static void directory_of(const char* path, char* output, size_t capacity)
{
    copy_text(output, capacity, path == nullptr || path[0] == '\0' ? "/sdcard/books" : path);
    char* slash = strrchr(output, '/');
    if (slash == nullptr)
    {
        copy_text(output, capacity, "/sdcard/books");
        return;
    }
    if (slash == output)
        slash[1] = '\0';
    else
        *slash = '\0';
}

static bool safe_filename(const char* name)
{
    return name != nullptr && name[0] != '\0' && strstr(name, "..") == nullptr &&
           strchr(name, '/') == nullptr && strchr(name, '\\') == nullptr;
}

static void url_encode(const char* source, char* output, size_t capacity)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0;
    if (capacity == 0)
        return;
    for (const unsigned char* cursor =
             reinterpret_cast<const unsigned char*>(source == nullptr ? "" : source);
         *cursor != '\0' && used + 1U < capacity; ++cursor)
    {
        const unsigned char value = *cursor;
        const bool unreserved = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                                (value >= '0' && value <= '9') || value == '-' || value == '_' ||
                                value == '.' || value == '~';
        if (unreserved)
            output[used++] = static_cast<char>(value);
        else if (used + 3U < capacity)
        {
            output[used++] = '%';
            output[used++] = hex[value >> 4U];
            output[used++] = hex[value & 0x0fU];
        }
        else
            break;
    }
    output[used] = '\0';
}

static void persist_pending(const request_t* request)
{
    nvs_handle_t handle = 0;
    if (nvs_open("xreader_sync", NVS_READWRITE, &handle) != ESP_OK)
        return;
    if (request == nullptr || request->book_path[0] == '\0')
    {
        nvs_erase_key(handle, "pbook");
        nvs_erase_key(handle, "pspine");
        nvs_erase_key(handle, "ppage");
    }
    else
    {
        nvs_set_str(handle, "pbook", request->book_path);
        nvs_set_u32(handle, "pspine", request->spine);
        nvs_set_u32(handle, "ppage", request->page);
    }
    nvs_commit(handle);
    nvs_close(handle);
}

static bool load_pending(request_t* request)
{
    if (request == nullptr)
        return false;
    nvs_handle_t handle = 0;
    if (nvs_open("xreader_sync", NVS_READONLY, &handle) != ESP_OK)
        return false;
    size_t length = sizeof(request->book_path);
    const esp_err_t error = nvs_get_str(handle, "pbook", request->book_path, &length);
    if (error == ESP_OK)
    {
        nvs_get_u32(handle, "pspine", &request->spine);
        nvs_get_u32(handle, "ppage", &request->page);
    }
    nvs_close(handle);
    return error == ESP_OK && request->book_path[0] != '\0';
}

static void append_history(const char* text)
{
    if (text == nullptr || text[0] == '\0')
        return;
    for (int index = 3; index > 0; --index)
        copy_text(state.history[index], sizeof(state.history[index]), state.history[index - 1]);
    copy_text(state.history[0], sizeof(state.history[0]), text);
    if (state.history_count < 4U)
        ++state.history_count;
    nvs_handle_t handle = 0;
    if (nvs_open("xreader_sync", NVS_READWRITE, &handle) == ESP_OK)
    {
        nvs_set_blob(handle, "history", state.history, sizeof(state.history));
        nvs_set_u8(handle, "histcnt", state.history_count);
        nvs_set_str(handle, "result", state.last_result);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static esp_err_t http_read_json(const char* url, char* buffer, size_t capacity, int* status)
{
    if (url == nullptr || buffer == nullptr || capacity < 2U)
        return ESP_ERR_INVALID_ARG;
    buffer[0] = '\0';
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 10000;
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
    if (content_length >= static_cast<int64_t>(capacity))
        error = ESP_ERR_INVALID_SIZE;
    size_t total = 0;
    while (error == ESP_OK && total + 1U < capacity)
    {
        const int read =
            esp_http_client_read(client, buffer + total, static_cast<int>(capacity - total - 1U));
        if (read < 0)
        {
            error = ESP_FAIL;
            break;
        }
        if (read == 0)
            break;
        total += static_cast<size_t>(read);
    }
    buffer[total] = '\0';
    if (status != nullptr)
        *status = esp_http_client_get_status_code(client);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return error;
}

static esp_err_t http_post_json(const char* url, const char* body)
{
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 10000;
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

static esp_err_t post_progress(const request_t* request)
{
    if (request == nullptr || request->book_path[0] == '\0' || state.server[0] == '\0')
        return ESP_OK;
    char url[256] = {};
    snprintf(url, sizeof(url), "%s/v1/progress", state.server);
    char body[512] = {};
    snprintf(body, sizeof(body), "{\"book\":\"%s\",\"spine\":%lu,\"page\":%lu}",
             basename_of(request->book_path), static_cast<unsigned long>(request->spine),
             static_cast<unsigned long>(request->page));
    return http_post_json(url, body);
}

static esp_err_t reconcile_progress(const request_t* request)
{
    if (request == nullptr || request->book_path[0] == '\0')
        return ESP_OK;
    char encoded[384] = {};
    url_encode(basename_of(request->book_path), encoded, sizeof(encoded));
    char url[640] = {};
    snprintf(url, sizeof(url), "%s/v1/progress?book=%s", state.server, encoded);
    char* response = new char[4096];
    if (response == nullptr)
        return ESP_ERR_NO_MEM;
    int status = 0;
    esp_err_t error = http_read_json(url, response, 4096, &status);
    if (error != ESP_OK)
    {
        delete[] response;
        return error;
    }
    if (status == 404)
    {
        delete[] response;
        return post_progress(request);
    }
    if (status < 200 || status >= 300)
    {
        delete[] response;
        return ESP_FAIL;
    }
    cJSON* root = cJSON_Parse(response);
    delete[] response;
    if (root == nullptr)
        return ESP_ERR_INVALID_RESPONSE;
    const cJSON* spine = cJSON_GetObjectItemCaseSensitive(root, "spine");
    const cJSON* page = cJSON_GetObjectItemCaseSensitive(root, "page");
    if (!cJSON_IsNumber(spine) || !cJSON_IsNumber(page))
    {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    const uint32_t remote_spine = static_cast<uint32_t>(spine->valuedouble);
    const uint32_t remote_page = static_cast<uint32_t>(page->valuedouble);
    cJSON_Delete(root);

    // Deterministic offline-friendly conflict policy: keep the furthest reading position.
    const bool remote_ahead = remote_spine > request->spine ||
                              (remote_spine == request->spine && remote_page > request->page);
    if (remote_ahead)
        return storage::persistence::save_position_for_book(request->book_path, remote_spine,
                                                            remote_page);
    return post_progress(request);
}

static bool bookmark_equal(const storage::persistence::bookmark_t& left,
                           const storage::persistence::bookmark_t& right)
{
    return left.spine == right.spine && left.page == right.page;
}

static esp_err_t sync_bookmarks(const request_t* request)
{
    if (request == nullptr || request->book_path[0] == '\0')
        return ESP_OK;
    storage::persistence::bookmark_t merged[storage::persistence::max_bookmarks_per_book] = {};
    uint8_t merged_count = 0;
    esp_err_t local_error = storage::persistence::load_bookmarks_for_book(
        request->book_path, merged, storage::persistence::max_bookmarks_per_book, &merged_count);
    if (local_error == ESP_ERR_NVS_NOT_FOUND)
        local_error = ESP_OK;
    if (local_error != ESP_OK)
        return local_error;

    char encoded[384] = {};
    url_encode(basename_of(request->book_path), encoded, sizeof(encoded));
    char url[640] = {};
    snprintf(url, sizeof(url), "%s/v1/bookmarks?book=%s", state.server, encoded);
    char* response = new char[4096];
    if (response == nullptr)
        return ESP_ERR_NO_MEM;
    int status = 0;
    esp_err_t error = http_read_json(url, response, 4096, &status);
    if (error == ESP_OK && status >= 200 && status < 300)
    {
        cJSON* root = cJSON_Parse(response);
        const cJSON* list =
            root == nullptr ? nullptr : cJSON_GetObjectItemCaseSensitive(root, "bookmarks");
        if (cJSON_IsArray(list))
        {
            const cJSON* item = nullptr;
            cJSON_ArrayForEach(item, list)
            {
                const cJSON* spine = cJSON_GetObjectItemCaseSensitive(item, "spine");
                const cJSON* page = cJSON_GetObjectItemCaseSensitive(item, "page");
                if (!cJSON_IsNumber(spine) || !cJSON_IsNumber(page))
                    continue;
                storage::persistence::bookmark_t candidate = {
                    .spine = static_cast<uint32_t>(spine->valuedouble),
                    .page = static_cast<uint32_t>(page->valuedouble),
                };
                bool duplicate = false;
                for (uint8_t index = 0; index < merged_count; ++index)
                    duplicate = duplicate || bookmark_equal(merged[index], candidate);
                if (!duplicate && merged_count < storage::persistence::max_bookmarks_per_book)
                    merged[merged_count++] = candidate;
            }
        }
        if (root != nullptr)
            cJSON_Delete(root);
    }
    delete[] response;
    if (error != ESP_OK && status != 404)
        return error;

    error = storage::persistence::save_bookmarks_for_book(request->book_path, merged, merged_count);
    if (error != ESP_OK)
        return error;

    cJSON* root = cJSON_CreateObject();
    cJSON* list = cJSON_AddArrayToObject(root, "bookmarks");
    cJSON_AddStringToObject(root, "book", basename_of(request->book_path));
    for (uint8_t index = 0; index < merged_count; ++index)
    {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "spine", merged[index].spine);
        cJSON_AddNumberToObject(item, "page", merged[index].page);
        cJSON_AddItemToArray(list, item);
    }
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (body == nullptr)
        return ESP_ERR_NO_MEM;
    snprintf(url, sizeof(url), "%s/v1/bookmarks", state.server);
    error = http_post_json(url, body);
    cJSON_free(body);
    return error;
}

static esp_err_t sha256_file(const char* path, char* output, size_t capacity)
{
    if (capacity < 65U)
        return ESP_ERR_INVALID_SIZE;
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
        return ESP_ERR_NOT_FOUND;
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    int result = mbedtls_sha256_starts(&context, 0);
    uint8_t buffer[2048] = {};
    while (result == 0)
    {
        const size_t count = fread(buffer, 1, sizeof(buffer), file);
        if (count > 0)
            result = mbedtls_sha256_update(&context, buffer, count);
        if (count < sizeof(buffer))
            break;
    }
    uint8_t digest[32] = {};
    if (result == 0)
        result = mbedtls_sha256_finish(&context, digest);
    mbedtls_sha256_free(&context);
    fclose(file);
    if (result != 0)
        return ESP_FAIL;
    for (size_t index = 0; index < sizeof(digest); ++index)
        snprintf(output + index * 2U, capacity - index * 2U, "%02x", digest[index]);
    output[64] = '\0';
    return ESP_OK;
}

static esp_err_t download_book(const remote_book_t& book, const char* directory)
{
    if (!safe_filename(book.name) || book.url[0] == '\0')
        return ESP_ERR_INVALID_ARG;
    char final_path[512] = {};
    char part_path[520] = {};
    snprintf(final_path, sizeof(final_path), "%s/%s", directory, book.name);
    snprintf(part_path, sizeof(part_path), "%s.part", final_path);

    struct stat info = {};
    if (stat(final_path, &info) == 0 && book.size != 0U &&
        static_cast<uint32_t>(info.st_size) == book.size)
    {
        if (book.sha256[0] == '\0')
            return ESP_OK;
        char digest[65] = {};
        if (sha256_file(final_path, digest, sizeof(digest)) == ESP_OK &&
            strcasecmp(digest, book.sha256) == 0)
            return ESP_OK;
    }

    uint32_t offset = 0;
    if (stat(part_path, &info) == 0 && info.st_size > 0)
        offset = static_cast<uint32_t>(info.st_size);
    if (book.size != 0U && offset > book.size)
    {
        unlink(part_path);
        offset = 0;
    }

    esp_http_client_config_t config = {};
    config.url = book.url;
    config.timeout_ms = 15000;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr)
        return ESP_ERR_NO_MEM;
    if (offset > 0)
    {
        char range[48] = {};
        snprintf(range, sizeof(range), "bytes=%lu-", static_cast<unsigned long>(offset));
        esp_http_client_set_header(client, "Range", range);
    }
    esp_err_t error = esp_http_client_open(client, 0);
    if (error != ESP_OK)
    {
        esp_http_client_cleanup(client);
        return error;
    }
    esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    if (status != 200 && status != 206)
        error = ESP_FAIL;
    if (error == ESP_OK && offset > 0 && status == 200)
    {
        // Server ignored Range; restart cleanly instead of appending duplicate bytes.
        offset = 0;
        unlink(part_path);
    }
    FILE* file = error == ESP_OK ? fopen(part_path, offset > 0 ? "ab" : "wb") : nullptr;
    if (error == ESP_OK && file == nullptr)
        error = ESP_FAIL;
    uint8_t buffer[4096] = {};
    while (error == ESP_OK)
    {
        const int count =
            esp_http_client_read(client, reinterpret_cast<char*>(buffer), sizeof(buffer));
        if (count < 0)
        {
            error = ESP_FAIL;
            break;
        }
        if (count == 0)
            break;
        if (fwrite(buffer, 1, static_cast<size_t>(count), file) != static_cast<size_t>(count))
        {
            error = ESP_FAIL;
            break;
        }
        state.bytes_downloaded += static_cast<uint32_t>(count);
    }
    if (file != nullptr)
        fclose(file);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (error != ESP_OK)
        return error;

    if (book.size != 0U)
    {
        if (stat(part_path, &info) != 0 || static_cast<uint32_t>(info.st_size) != book.size)
            return ESP_ERR_INVALID_SIZE;
    }
    if (book.sha256[0] != '\0')
    {
        char digest[65] = {};
        error = sha256_file(part_path, digest, sizeof(digest));
        if (error != ESP_OK || strcasecmp(digest, book.sha256) != 0)
            return ESP_ERR_INVALID_CRC;
    }
    if (rename(part_path, final_path) != 0)
        return ESP_FAIL;
    return ESP_OK;
}

static esp_err_t fetch_and_sync_library(const request_t* request)
{
    char url[256] = {};
    snprintf(url, sizeof(url), "%s/v1/library", state.server);
    char* response = new char[response_capacity];
    if (response == nullptr)
        return ESP_ERR_NO_MEM;
    int status = 0;
    esp_err_t error = http_read_json(url, response, response_capacity, &status);
    if (error != ESP_OK || status < 200 || status >= 300)
    {
        delete[] response;
        return error == ESP_OK ? ESP_FAIL : error;
    }
    cJSON* root = cJSON_Parse(response);
    delete[] response;
    if (root == nullptr)
        return ESP_ERR_INVALID_RESPONSE;
    const cJSON* books = cJSON_GetObjectItemCaseSensitive(root, "books");
    if (!cJSON_IsArray(books))
    {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    remote_book_t manifest[max_manifest_books] = {};
    uint8_t count = 0;
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, books)
    {
        if (count >= max_manifest_books)
            break;
        const cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
        const cJSON* remote_url = cJSON_GetObjectItemCaseSensitive(item, "url");
        const cJSON* size = cJSON_GetObjectItemCaseSensitive(item, "size");
        const cJSON* sha = cJSON_GetObjectItemCaseSensitive(item, "sha256");
        if (!cJSON_IsString(name) || !cJSON_IsString(remote_url) ||
            !safe_filename(name->valuestring))
            continue;
        copy_text(manifest[count].name, sizeof(manifest[count].name), name->valuestring);
        copy_text(manifest[count].url, sizeof(manifest[count].url), remote_url->valuestring);
        if (cJSON_IsNumber(size) && size->valuedouble > 0)
            manifest[count].size = static_cast<uint32_t>(size->valuedouble);
        if (cJSON_IsString(sha))
            copy_text(manifest[count].sha256, sizeof(manifest[count].sha256), sha->valuestring);
        ++count;
    }
    cJSON_Delete(root);

    state.books_total = count;
    state.books_completed = 0;
    state.bytes_downloaded = 0;
    state.bytes_total = 0;
    for (uint8_t index = 0; index < count; ++index)
        state.bytes_total += manifest[index].size;

    char directory[256] = {};
    directory_of(request == nullptr ? nullptr : request->book_path, directory, sizeof(directory));
    for (uint8_t index = 0; index < count; ++index)
    {
        copy_text(state.activity, sizeof(state.activity), manifest[index].name);
        error = download_book(manifest[index], directory);
        if (error != ESP_OK)
            return error;
        ++state.books_completed;
    }
    if (count > 0U)
    {
        char mount_path[256] = {};
        copy_text(mount_path, sizeof(mount_path), directory);
        char* slash = strrchr(mount_path, '/');
        if (slash != nullptr && strcmp(slash + 1, "books") == 0)
            *slash = '\0';
        services::library_index::invalidate(mount_path);
    }
    return ESP_OK;
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

    nvs_handle_t handle = 0;
    if (nvs_open("xreader_sync", NVS_READWRITE, &handle) == ESP_OK)
    {
        nvs_set_str(handle, "last", state.last_sync);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static void sync_task(void* argument)
{
    request_t* request = static_cast<request_t*>(argument);
    esp_err_t error = ESP_OK;
    if (state.sync_books)
    {
        copy_text(state.activity, sizeof(state.activity), "LIBRARY");
        error = fetch_and_sync_library(request);
    }
    if (error == ESP_OK && state.sync_progress)
    {
        copy_text(state.activity, sizeof(state.activity), "READING PROGRESS");
        error = reconcile_progress(request);
    }
    if (error == ESP_OK && state.sync_progress)
    {
        copy_text(state.activity, sizeof(state.activity), "BOOKMARKS");
        error = sync_bookmarks(request);
    }
    if (error == ESP_OK)
    {
        update_last_sync();
        copy_text(state.activity, sizeof(state.activity), "COMPLETE");
        copy_text(state.last_result, sizeof(state.last_result), "SYNC COMPLETE");
        append_history(state.last_result);
        persist_pending(nullptr);
        state.pending_retry = false;
        state.retry_count = 0;
        state.phase = phase_complete;
    }
    else
    {
        snprintf(state.activity, sizeof(state.activity), "ERROR: %s", esp_err_to_name(error));
        snprintf(state.last_result, sizeof(state.last_result), "FAILED: %s",
                 esp_err_to_name(error));
        append_history(state.last_result);
        persist_pending(request);
        state.pending_retry = true;
        if (state.retry_count < 255U)
            ++state.retry_count;
        retry_not_before = xTaskGetTickCount() + retry_delay;
        state.phase = phase_error;
    }
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
    length = sizeof(state.last_result);
    nvs_get_str(handle, "result", state.last_result, &length);
    size_t history_size = sizeof(state.history);
    if (nvs_get_blob(handle, "history", state.history, &history_size) == ESP_OK)
        nvs_get_u8(handle, "histcnt", &state.history_count);
    nvs_close(handle);
    request_t pending = {};
    state.pending_retry = load_pending(&pending);
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
    if (state.last_result[0] != '\0')
        nvs_set_str(handle, "result", state.last_result);
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
    copy_text(state.activity, sizeof(state.activity), "IDLE");
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
    state.books_total = 0;
    state.books_completed = 0;
    state.bytes_downloaded = 0;
    state.bytes_total = 0;
    copy_text(state.activity, sizeof(state.activity), "STARTING");
    persist_pending(request);
    state.pending_retry = true;
    if (xTaskCreate(sync_task, "xreader_sync", 12288, request, 4, nullptr) != pdPASS)
    {
        delete request;
        state.phase = phase_error;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void poll(bool connected)
{
    init();
    if (!connected || !state.pending_retry || state.phase == phase_syncing)
        return;
    const TickType_t now = xTaskGetTickCount();
    if (retry_not_before != 0 && static_cast<int32_t>(now - retry_not_before) < 0)
        return;
    request_t pending = {};
    if (!load_pending(&pending))
    {
        state.pending_retry = false;
        return;
    }
    (void)request_sync(pending.book_path, pending.spine, pending.page);
}

state_t snapshot()
{
    init();
    return state;
}

} // namespace book_sync
} // namespace services
} // namespace xreader
