#pragma once

#include <ctype.h>
#include <stdint.h>
#include <string.h>

namespace xreader
{
namespace services
{
namespace version
{

inline const char* skip_prefix(const char* text)
{
    if (text == nullptr)
        return "";
    return (*text == 'v' || *text == 'V') ? text + 1 : text;
}

inline uint32_t component(const char** cursor)
{
    const char* p = *cursor;
    uint32_t value = 0;
    while (*p >= '0' && *p <= '9')
    {
        const uint32_t digit = static_cast<uint32_t>(*p - '0');
        value = value > (UINT32_MAX - digit) / 10U ? UINT32_MAX : value * 10U + digit;
        ++p;
    }
    *cursor = p;
    return value;
}

// Returns <0 when lhs<rhs, 0 when equivalent, >0 when lhs>rhs.
// Supports the common vMAJOR.MINOR.PATCH[-prerelease][+build] form without
// allocating or pulling a semantic-version library into the firmware.
inline int compare(const char* lhs, const char* rhs)
{
    const char* a = skip_prefix(lhs);
    const char* b = skip_prefix(rhs);
    for (uint8_t index = 0; index < 3U; ++index)
    {
        const uint32_t av = component(&a);
        const uint32_t bv = component(&b);
        if (av < bv) return -1;
        if (av > bv) return 1;
        if (*a == '.') ++a;
        if (*b == '.') ++b;
    }

    // Build metadata does not affect precedence. Stable releases sort after a
    // prerelease with the same numeric core.
    const bool a_pre = *a == '-';
    const bool b_pre = *b == '-';
    if (a_pre != b_pre)
        return a_pre ? -1 : 1;
    if (!a_pre)
        return 0;
    ++a;
    ++b;
    while (*a != '\0' && *a != '+' && *b != '\0' && *b != '+')
    {
        if (*a != *b)
            return static_cast<unsigned char>(*a) < static_cast<unsigned char>(*b) ? -1 : 1;
        ++a;
        ++b;
    }
    const bool a_end = *a == '\0' || *a == '+';
    const bool b_end = *b == '\0' || *b == '+';
    if (a_end && b_end) return 0;
    return a_end ? -1 : 1;
}

} // namespace version
} // namespace services
} // namespace xreader
