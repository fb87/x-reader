#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace book {

inline constexpr std::size_t title_size = 128;
inline constexpr std::size_t author_size = 64;
inline constexpr std::size_t path_size = 256;

/** @brief Ebook metadata retained by the in-memory library index. */
struct item {
    std::array<char, title_size> title{};
    std::array<char, author_size> author{};
    std::array<char, path_size> path{};
    std::uint8_t progress = 0;
    bool favorite = false;
};

/** @brief Produces a display title from a path or filename. */
inline void make_title(char* destination, std::size_t capacity, const char* source)
{
    if (capacity == 0) {
        return;
    }
    const char* name = source;
    for (const char* cursor = source; cursor != nullptr && *cursor != '\0'; ++cursor) {
        if (*cursor == '/') {
            name = cursor + 1;
        }
    }
    std::snprintf(destination, capacity, "%s", name == nullptr ? "" : name);
    for (std::size_t i = 0; destination[i] != '\0'; ++i) {
        if (destination[i] == '.' && destination[i + 1] != '\0') {
            const char* suffix = destination + i;
            if (suffix[1] == 'e' || suffix[1] == 'E') {
                destination[i] = '\0';
                break;
            }
        }
        if (destination[i] == '_' || destination[i] == '-') {
            destination[i] = ' ';
        }
    }
}

} // namespace book
