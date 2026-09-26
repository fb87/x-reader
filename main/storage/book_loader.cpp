#include "book_loader.hpp"

#include <string.h>

#include "epub/book.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace xreader
{
namespace storage
{
namespace book_loader
{
namespace
{

static const char* const tag = "book_loader";

struct context_t
{
    char path[256];
    epub::book_t* book;
    volatile bool complete;
    esp_err_t result;
};

static context_t* active_context = nullptr;

static void task(void* argument)
{
    context_t* context = static_cast<context_t*>(argument);
    context->book = static_cast<epub::book_t*>(
        heap_caps_calloc(1, sizeof(epub::book_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (context->book == nullptr)
    {
        ESP_LOGE(tag, "book metadata allocation failed");
        context->result = ESP_ERR_NO_MEM;
    }
    else
    {
        context->result = epub::load_metadata(context->path, context->book);
        if (context->result == ESP_OK)
        {
            ESP_LOGI(tag, "loaded title='%s' author='%s' spine='%u'", context->book->title,
                     context->book->author, context->book->spine_count);
        }
        else
        {
            ESP_LOGW(tag, "metadata load failed for %s: %s", context->path,
                     esp_err_to_name(context->result));
        }
    }
    context->complete = true;
    vTaskDelete(nullptr);
}

} // namespace

esp_err_t start(const char* path)
{
    if (path == nullptr || strlen(path) >= 256)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (active_context != nullptr)
        return ESP_ERR_INVALID_STATE;
    context_t* context = static_cast<context_t*>(
        heap_caps_calloc(1, sizeof(context_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (context == nullptr)
        return ESP_ERR_NO_MEM;
    strcpy(context->path, path);
    context->result = ESP_ERR_INVALID_STATE;
    active_context = context;
    if (xTaskCreate(task, "xreader_book", 32768, context, 4, nullptr) != pdPASS)
    {
        heap_caps_free(context);
        active_context = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool poll(epub::book_t* book, esp_err_t* result)
{
    if (active_context == nullptr || !active_context->complete)
        return false;
    context_t* context = active_context;
    if (book != nullptr && context->book != nullptr)
        memcpy(book, context->book, sizeof(*book));
    if (result != nullptr)
        *result = context->result;
    heap_caps_free(context->book);
    heap_caps_free(context);
    active_context = nullptr;
    return true;
}

} // namespace book_loader
} // namespace storage
} // namespace xreader
