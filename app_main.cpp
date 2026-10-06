#include "app/cpp/init.hpp"
#include "core/state.hpp"
#include "selected_board.hpp"

#include <cstdio>

/** @brief Composition root wiring the build-selected board to the board-independent application. */
int main()
{
    selected_board::runtime board{};
    selected_board::init(board);

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
