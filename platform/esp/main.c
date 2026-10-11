#include "m5paper_platform.h"
#include "runtime.h"

#include <esp_log.h>
#include <esp_spiffs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* const TAG = "x-reader";

void app_main(void) {
    const esp_vfs_spiffs_conf_t spiffs = {
        .base_path = "/lua",
        .partition_label = "lua",
        .max_files = 32,
        .format_if_mount_failed = false,
    };
    esp_err_t status = esp_vfs_spiffs_register(&spiffs);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Lua filesystem mount failed: %s", esp_err_to_name(status));
        return;
    }
    struct platform platform = {0};
    status = esp_platform_create(&platform);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "M5Paper platform unavailable: %s", esp_err_to_name(status));
        esp_vfs_spiffs_unregister("lua");
        return;
    }
    struct runtime* runtime = NULL;
    const struct runtime_config config = {
        .script_directory = "/lua",
        .library_directory = "/sdcard",
        .state_directory = "/state",
        .memory_limit = 512U * 1024U,
        .instruction_limit = 1000000U,
    };
    ESP_LOGI(TAG, "starting Lua runtime");
    if (runtime_create(&runtime, &config, &platform) != PLATFORM_OK) {
        ESP_LOGE(TAG, "Lua application failed to start: %s", runtime_error(runtime));
        esp_platform_destroy(&platform);
        esp_vfs_spiffs_unregister("lua");
        return;
    }
    ESP_LOGI(TAG, "Lua runtime started");
    if (runtime_update(runtime, platform.monotonic_ms(platform.context)) != PLATFORM_OK ||
        runtime_draw(runtime) != PLATFORM_OK) {
        ESP_LOGE(TAG, "Lua application initial frame failed: %s", runtime_error(runtime));
    } else {
        ESP_LOGI(TAG, "Lua application initial frame presented");
    }
    bool launcher_presented = false;
    uint64_t next_redraw = 250U;
    while (true) {
        struct platform_event event = {0};
        const uint64_t now = platform.monotonic_ms(platform.context);
        if (platform.next_event(platform.context, &event) != PLATFORM_OK ||
            (event.type != PLATFORM_EVENT_NONE && runtime_step(runtime, &event) != PLATFORM_OK) ||
            runtime_update(runtime, now) != PLATFORM_OK) {
            ESP_LOGE(TAG, "Lua application loop failed: %s", runtime_error(runtime));
            break;
        }
        if (now >= next_redraw || event.type != PLATFORM_EVENT_NONE) {
            if (runtime_draw(runtime) != PLATFORM_OK) {
                ESP_LOGE(TAG, "Lua application redraw failed: %s", runtime_error(runtime));
                break;
            }
            next_redraw = now + 250U;
            if (!launcher_presented && now >= 700U) {
                launcher_presented = true;
                ESP_LOGI(TAG, "Lua launcher frame presented");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    runtime_stop(runtime);
    runtime_destroy(runtime);
    esp_platform_destroy(&platform);
    esp_vfs_spiffs_unregister("lua");
}
