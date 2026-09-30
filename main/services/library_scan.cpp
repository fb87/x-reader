#include "library_scan.hpp"

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "services/book_manager.hpp"

namespace xreader
{
namespace services
{
namespace library_scan
{

namespace
{
static const char* const tag = "libscan";
static constexpr size_t mount_path_capacity = 64;

// `catalog_t` is large enough that the project's own callers allocate it from
// SPIRAM; a stack copy inside a task would blow the default stack.  One static
// working buffer is enough since request() refuses to start a second scan.
static library_index::catalog_t* working_catalog = nullptr;
static char mount_path_copy[mount_path_capacity] = {};
static action_t pending_action = action_rescan;
static volatile bool task_busy = false;
static volatile bool result_ready = false;
static result_t last_result = {};

static library_index::catalog_t* allocate_catalog()
{
    library_index::catalog_t* catalog = static_cast<library_index::catalog_t*>(heap_caps_calloc(
        1, sizeof(library_index::catalog_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (catalog == nullptr)
        catalog = static_cast<library_index::catalog_t*>(
            heap_caps_calloc(1, sizeof(library_index::catalog_t), MALLOC_CAP_8BIT));
    return catalog;
}

static void scan_task(void*)
{
    result_t result = {};
    if (pending_action == action_import)
    {
        const book_manager::result_t outcome = book_manager::import_books(mount_path_copy);
        result.imported = outcome.imported;
        result.error = outcome.error;
    }
    else if (pending_action == action_cleanup)
    {
        const book_manager::result_t outcome = book_manager::cleanup(mount_path_copy);
        result.removed = outcome.removed;
        result.error = outcome.error;
    }
    const esp_err_t rebuild_error = library_index::rebuild(mount_path_copy, working_catalog);
    if (result.error == ESP_OK)
        result.error = rebuild_error;
    ESP_LOGI(tag, "scan complete: action=%u imported=%u removed=%u books=%u error=%s",
             static_cast<unsigned>(pending_action), static_cast<unsigned>(result.imported),
             static_cast<unsigned>(result.removed), static_cast<unsigned>(working_catalog->count),
             esp_err_to_name(result.error));
    last_result = result;
    result_ready = true;
    task_busy = false;
    vTaskDelete(nullptr);
}
} // namespace

esp_err_t request(action_t action, const char* mount_path)
{
    if (task_busy)
        return ESP_ERR_INVALID_STATE;
    if (mount_path == nullptr || strlen(mount_path) >= mount_path_capacity)
        return ESP_ERR_INVALID_ARG;
    if (working_catalog == nullptr)
    {
        working_catalog = allocate_catalog();
        if (working_catalog == nullptr)
            return ESP_ERR_NO_MEM;
    }
    snprintf(mount_path_copy, sizeof(mount_path_copy), "%s", mount_path);
    pending_action = action;
    task_busy = true;
    result_ready = false;
    // library_index::rebuild() runs the same EPUB zip/XML metadata parser that
    // main()'s boot-time scan runs on the 64 KB main app task stack
    // (CONFIG_ESP_MAIN_TASK_STACK_SIZE).  An 8 KB stack here overflowed inside
    // zip::open() on real hardware: the corruption didn't crash immediately, it
    // wedged a newlib lock and took down the whole device with an interrupt
    // watchdog panic on the next fopen(), which looked like an SD/SPI
    // concurrency bug until decoded with addr2line.  Match the main task's
    // stack size since this is the identical code path.
    if (xTaskCreate(scan_task, "xreader_libscan", 65536, nullptr, 3, nullptr) != pdPASS)
    {
        task_busy = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool busy()
{
    return task_busy;
}

bool take_result(library_index::catalog_t* catalog, result_t* result)
{
    if (!result_ready || catalog == nullptr || working_catalog == nullptr)
        return false;
    memcpy(catalog, working_catalog, sizeof(library_index::catalog_t));
    if (result != nullptr)
        *result = last_result;
    result_ready = false;
    return true;
}

} // namespace library_scan
} // namespace services
} // namespace xreader
