#pragma once

#include "core/state.hpp"
#include "reader/library.hpp"

#include <cstdint>

namespace reader {

/** @brief Runtime reader-domain context shared by application pages. */
struct context {
    library::index library{};
};

/** @brief Initializes reader defaults without overwriting restored values. */
inline void init(context&, state::store& shared)
{
    if (state::get(shared, "reader.settings.font_size", std::int64_t{-1}) < 0) {
        state::set(shared, "reader.settings.font_size", std::int64_t{1});
    }
    if (state::get(shared, "reader.settings.full_refresh_every", std::int64_t{-1}) < 0) {
        state::set(shared, "reader.settings.full_refresh_every", std::int64_t{6});
    }
    if (state::get(shared, "reader.settings.show_progress", false) == false &&
        state::find(shared, "reader.settings.show_progress") == nullptr) {
        state::set(shared, "reader.settings.show_progress", true);
    }
    if (state::get(shared, "reader.library.selected", std::int64_t{-1}) < 0) {
        state::set(shared, "reader.library.selected", std::int64_t{0});
    }
    if (state::find(shared, "reader.book.current") == nullptr) {
        state::set(shared, "reader.book.current", std::int64_t{-1});
    }
    if (state::find(shared, "reader.book.page") == nullptr) {
        state::set(shared, "reader.book.page", std::int64_t{0});
    }
}

/** @brief Opens the selected library book and resets reading position. */
inline bool open_selected(context& self, state::store& shared)
{
    auto* selected = library::selected(self.library, shared);
    if (selected == nullptr) {
        return false;
    }
    const auto id = state::get(shared, "reader.library.selected", std::int64_t{0});
    state::set(shared, "reader.book.current", id);
    state::set(shared, "reader.book.page", std::int64_t{0});
    state::set(shared, "reader.book.progress", static_cast<std::int64_t>(selected->progress));
    state::set(shared, "reader.book.title", selected->title.data());
    return true;
}

/** @brief Advances one page and updates shared progress state. */
inline void next_page(state::store& shared)
{
    auto page = state::get(shared, "reader.book.page", std::int64_t{0}) + 1;
    state::set(shared, "reader.book.page", page);
    state::set(shared, "reader.book.progress", page >= 100 ? std::int64_t{100} : page);
}

/** @brief Moves one page backward without underflow. */
inline void previous_page(state::store& shared)
{
    auto page = state::get(shared, "reader.book.page", std::int64_t{0});
    if (page > 0) {
        --page;
    }
    state::set(shared, "reader.book.page", page);
    state::set(shared, "reader.book.progress", page);
}

/** @brief Changes the reader font-size index in the supported range. */
inline void adjust_font(state::store& shared, int delta)
{
    auto value = state::get(shared, "reader.settings.font_size", std::int64_t{1}) + delta;
    if (value < 0) {
        value = 0;
    }
    if (value > 2) {
        value = 2;
    }
    state::set(shared, "reader.settings.font_size", value);
}

} // namespace reader
