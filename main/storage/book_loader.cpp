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
};

static void task(void* argument)
{
    context_t* context = static_cast<context_t*>(argument);
    epub::book_t* book = static_cast<epub::book_t*>(
        heap_caps_calloc(1, sizeof(epub::book_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (book == nullptr)
    {
        ESP_LOGE(tag, "book metadata allocation failed");
    }
    else
    {
        const esp_err_t error = epub::load_metadata(context->path, book);
        if (error == ESP_OK)
        {
            ESP_LOGI(tag, "loaded title='%s' author='%s' spine='%u'", book->title, book->author,
                     book->spine_count);
        }
        else
        {
            ESP_LOGW(tag, "metadata load failed for %s: %s", context->path, esp_err_to_name(error));
        }
        heap_caps_free(book);
    }
    heap_caps_free(context);
    vTaskDelete(nullptr);
}

} // namespace

esp_err_t start(const char* path)
{
    if (path == nullptr || strlen(path) >= 256)
    {
        return ESP_ERR_INVALID_ARG;
    }
    context_t* context = static_cast<context_t*>(
        heap_caps_calloc(1, sizeof(context_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (context == nullptr)
        return ESP_ERR_NO_MEM;
    strcpy(context->path, path);
    if (xTaskCreate(task, "xreader_book", 32768, context, 4, nullptr) != pdPASS)
    {
        heap_caps_free(context);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

} // namespace book_loader
} // namespace storage
} // namespace xreader
