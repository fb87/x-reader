#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace book {

inline constexpr std::size_t title_size = 128;
inline constexpr std::size_t author_size = 64;
inline constexpr std::size_t path_size = 256;

inline bool valid_utf8(const unsigned char* value) {
    while (*value != 0) {
        if (*value < 0x80U) { ++value; continue; }
        int remaining = (*value & 0xE0U) == 0xC0U ? 1 : (*value & 0xF0U) == 0xE0U ? 2
                                                        : (*value & 0xF8U) == 0xF0U ? 3 : -1;
        if (remaining < 0) return false;
        ++value;
        while (remaining-- != 0) {
            if ((*value & 0xC0U) != 0x80U) return false;
            ++value;
        }
    }
    return true;
}

inline int windows_1252_byte(std::uint32_t cp) {
    struct mapping { std::uint32_t cp; unsigned char byte; };
    static constexpr mapping values[] = {
        {0x20AC, 0x80}, {0x201A, 0x82}, {0x0192, 0x83}, {0x201E, 0x84},
        {0x2026, 0x85}, {0x2020, 0x86}, {0x2021, 0x87}, {0x02C6, 0x88},
        {0x2030, 0x89}, {0x0160, 0x8A}, {0x2039, 0x8B}, {0x0152, 0x8C},
        {0x017D, 0x8E}, {0x2018, 0x91}, {0x2019, 0x92}, {0x201C, 0x93},
        {0x201D, 0x94}, {0x2022, 0x95}, {0x2013, 0x96}, {0x2014, 0x97},
        {0x02DC, 0x98}, {0x2122, 0x99}, {0x0161, 0x9A}, {0x203A, 0x9B},
        {0x0153, 0x9C}, {0x017E, 0x9E}, {0x0178, 0x9F},
    };
    for (const auto& value : values) if (value.cp == cp) return value.byte;
    return -1;
}

/** Repairs UTF-8 that was incorrectly decoded as Latin-1 and encoded again. */
inline void repair_mojibake(char* destination, std::size_t capacity, const char* source) {
    if (destination == nullptr || capacity == 0 || source == nullptr) return;
    unsigned char recovered[title_size]{};
    std::size_t used = 0;
    bool changed = false;
    const auto* cursor = reinterpret_cast<const unsigned char*>(source);
    while (*cursor != 0 && used + 1 < sizeof(recovered)) {
        std::uint32_t cp = 0;
        if (*cursor < 0x80U) {
            cp = *cursor++;
        } else {
            const unsigned char first = *cursor++;
            int continuation = (first & 0xE0U) == 0xC0U ? 1 : (first & 0xF0U) == 0xE0U ? 2
                                                              : (first & 0xF8U) == 0xF0U ? 3 : -1;
            if (continuation < 0) return;
            cp = first & (continuation == 1 ? 0x1FU : continuation == 2 ? 0x0FU : 0x07U);
            while (continuation-- != 0) {
                if ((*cursor & 0xC0U) != 0x80U) return;
                cp = (cp << 6U) | (*cursor++ & 0x3FU);
            }
        }
        if (cp > 0xFFU) {
            const int mapped = windows_1252_byte(cp);
            if (mapped < 0) return;
            cp = static_cast<std::uint32_t>(mapped);
        }
        recovered[used++] = static_cast<unsigned char>(cp);
        if (cp >= 0x80U) changed = true;
    }
    recovered[used] = 0;
    if (!changed || !valid_utf8(recovered)) return;
    std::snprintf(destination, capacity, "%s", reinterpret_cast<const char*>(recovered));
}

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
    char repaired[title_size]{};
    std::snprintf(repaired, sizeof(repaired), "%s", destination);
    repair_mojibake(destination, capacity, repaired);
}

} // namespace book
