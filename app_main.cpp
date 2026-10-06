#include "app/cpp/init.hpp"
#include "core/state.hpp"
#include "selected_board.hpp"

#include <cstdio>
#include <cstdlib>

/** @brief Composition root wiring the build-selected board to the board-independent application. */
static int run_application() {
    selected_board::runtime board{};
#ifdef ESP_PLATFORM
    if (!selected_board::init(board) || !selected_board::mount(board, "/sdcard")) return 1;
#else
    selected_board::init(board);
    if (const char* root = std::getenv("XREADER_SDCARD")) selected_board::mount(board, root);
#endif

    state::store memory{};
    state::store persistent{};
    (void)state::load(persistent, "build/state.db");

    app::context application{};
    if (!app::init(application, board.capabilities, memory, persistent)) {
        std::fprintf(stderr, "application initialization failed\n");
        return 1;
    }

    app::pump(application);
    app::checkpoint(application);
    (void)state::save(persistent, "build/state.db");
    return 0;
}

#ifdef ESP_PLATFORM
extern "C" void app_main() { (void)run_application(); }
#else
int main() { return run_application(); }
#endif
