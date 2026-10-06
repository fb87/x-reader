#include "app/cpp/init.hpp"
#include "core/state.hpp"
#include "selected_board.hpp"

#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <new>
#endif

#include <cstdio>
#include <cstdlib>

/** @brief Composition root wiring the build-selected board to the board-independent application. */
static int run_application() {
    static selected_board::runtime board{};
#ifdef ESP_PLATFORM
    if (!selected_board::init(board) || !selected_board::mount(board, "/sdcard")) return 1;
#else
    selected_board::init(board);
    if (const char* root = std::getenv("XREADER_SDCARD")) selected_board::mount(board, root);
#endif

#ifdef ESP_PLATFORM
    struct app_storage {
        state::store memory{};
        state::store persistent{};
        app::context application{};
    };
    static app_storage* storage = nullptr;
    if (storage == nullptr) {
        void* memory = heap_caps_calloc(1, sizeof(app_storage), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (memory == nullptr) return 1;
        storage = new (memory) app_storage{};
    }
    auto& memory = storage->memory;
    auto& persistent = storage->persistent;
    auto& application = storage->application;
#else
    static state::store memory{};
    static state::store persistent{};
    static app::context application{};
#endif
    (void)state::load(persistent, "build/state.db");

    if (!app::init(application, board.capabilities, memory, persistent)) {
        std::fprintf(stderr, "application initialization failed\n");
        return 1;
    }
#ifdef ESP_PLATFORM
    ESP_LOGI("xreader", "library scan found %u books",
             static_cast<unsigned>(application.reader.library.count));
#endif

#ifdef ESP_PLATFORM
    // Keep the board-owned application alive after the initial frame. The
    // simulator has its own event loop, while ESP-IDF invokes app_main only
    // once and otherwise leaves the splash frame on the panel forever.
    for (;;) {
        app::pump(application);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
#else
    app::pump(application);
    app::checkpoint(application);
    (void)state::save(persistent, "build/state.db");
#endif
    return 0;
}

#ifdef ESP_PLATFORM
extern "C" void app_main() { (void)run_application(); }
#else
int main() { return run_application(); }
#endif
