#include "m5paper_platform.h"
#include "runtime.h"

#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_timer.h>
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
    if (runtime_update(runtime, platform.monotonic_ms(platform.context), NULL) != PLATFORM_OK ||
        runtime_draw(runtime) != PLATFORM_OK) {
        ESP_LOGE(TAG, "Lua application initial frame failed: %s", runtime_error(runtime));
    } else {
        ESP_LOGI(TAG, "Lua application initial frame presented");
    }
    bool launcher_presented = false;
    while (true) {
        struct platform_event event = {0};
        const uint64_t now = platform.monotonic_ms(platform.context);
        bool needs_redraw = false;
        if (platform.next_event(platform.context, &event) != PLATFORM_OK ||
            (event.type != PLATFORM_EVENT_NONE && runtime_step(runtime, &event) != PLATFORM_OK) ||
            runtime_update(runtime, now, &needs_redraw) != PLATFORM_OK) {
            ESP_LOGE(TAG, "Lua application loop failed: %s", runtime_error(runtime));
            break;
        }
        /* Redraw only on a real input event or when Lua's update() reports
         * a state change (e.g. an internal, timer-driven scene transition
         * like the loader->launcher splash or auto-sleep). No unconditional
         * periodic redraw: clearing, redrawing, and diffing the full
         * framebuffer costs well over 100ms even when nothing visually
         * changed, and almost nothing in this app changes on a timer. */
        if (needs_redraw || event.type != PLATFORM_EVENT_NONE) {
            const int64_t draw_started_us = esp_timer_get_time();
            if (runtime_draw(runtime) != PLATFORM_OK) {
                ESP_LOGE(TAG, "Lua application redraw failed: %s", runtime_error(runtime));
                break;
            }
            ESP_LOGI(TAG, "frame: event=%d draw+present in %lldms", (int)event.type,
                     (long long)((esp_timer_get_time() - draw_started_us) / 1000));
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
