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
    char path[512];
    epub::book_t* book;
    epub::document_t* document;
    uint8_t spine_index;
    bool load_metadata;
    volatile bool complete;
    esp_err_t result;
};

static context_t* active_context = nullptr;

static void task(void* argument)
{
    context_t* context = static_cast<context_t*>(argument);
    if (context->load_metadata)
        context->book = static_cast<epub::book_t*>(
            heap_caps_calloc(1, sizeof(epub::book_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (context->book == nullptr)
    {
        ESP_LOGE(tag, "book metadata allocation failed");
        context->result = ESP_ERR_NO_MEM;
    }
    else
    {
        context->result =
            context->load_metadata ? epub::load_metadata(context->path, context->book) : ESP_OK;
        if (context->result == ESP_OK)
        {
            if (context->load_metadata)
                ESP_LOGI(tag, "loaded title='%s' author='%s' spine='%d'", context->book->title,
                         context->book->author, static_cast<int>(context->book->spine_count));
            context->document = static_cast<epub::document_t*>(
                heap_caps_calloc(1, sizeof(epub::document_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (context->document == nullptr)
                context->result = ESP_ERR_NO_MEM;
            else
            {
                context->result = epub::load_document(context->path, context->book,
                                                      context->spine_index, context->document);
                if (context->result != ESP_OK)
                    ESP_LOGW(tag, "document load failed: spine=%u directory='%s' href='%s': %s",
                             static_cast<unsigned>(context->spine_index),
                             context->book->opf_directory,
                             context->book->spine[context->spine_index].href,
                             esp_err_to_name(context->result));
            }
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
    if (path == nullptr || strlen(path) >= 512)
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
    context->load_metadata = true;
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

esp_err_t start_document(const char* path, const epub::book_t* book, uint8_t spine_index)
{
    if (path == nullptr || strlen(path) >= 512 || book == nullptr ||
        spine_index >= book->spine_count)
        return ESP_ERR_INVALID_ARG;
    if (active_context != nullptr)
        return ESP_ERR_INVALID_STATE;
    context_t* context = static_cast<context_t*>(
        heap_caps_calloc(1, sizeof(context_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (context == nullptr)
        return ESP_ERR_NO_MEM;
    context->book = static_cast<epub::book_t*>(
        heap_caps_malloc(sizeof(epub::book_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (context->book == nullptr)
    {
        heap_caps_free(context);
        return ESP_ERR_NO_MEM;
    }
    strcpy(context->path, path);
    memcpy(context->book, book, sizeof(*book));
    context->spine_index = spine_index;
    context->load_metadata = false;
    context->result = ESP_ERR_INVALID_STATE;
    active_context = context;
    if (xTaskCreate(task, "xreader_doc", 32768, context, 4, nullptr) != pdPASS)
    {
        heap_caps_free(context->book);
        heap_caps_free(context);
        active_context = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool poll(epub::book_t* book, epub::document_t* document, esp_err_t* result)
{
    if (active_context == nullptr || !active_context->complete)
        return false;
    context_t* context = active_context;
    if (book != nullptr && context->book != nullptr)
        memcpy(book, context->book, sizeof(*book));
    if (document != nullptr && context->document != nullptr)
        memcpy(document, context->document, sizeof(*document));
    if (result != nullptr)
        *result = context->result;
    heap_caps_free(context->book);
    heap_caps_free(context->document);
    heap_caps_free(context);
    active_context = nullptr;
    return true;
}

} // namespace book_loader
} // namespace storage
} // namespace xreader
