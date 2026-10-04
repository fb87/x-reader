#include "book_title.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static bool utf8_codepoint(const unsigned char *text, size_t available,
                           unsigned *value, size_t *length)
{
    unsigned first = text[0];
    if (first < 0x80) {
        *value = first;
        *length = 1;
        return true;
    }

    size_t count;
    unsigned codepoint;
    unsigned minimum;
    if ((first & 0xe0) == 0xc0) {
        count = 2;
        codepoint = first & 0x1f;
        minimum = 0x80;
    } else if ((first & 0xf0) == 0xe0) {
        count = 3;
        codepoint = first & 0x0f;
        minimum = 0x800;
    } else if ((first & 0xf8) == 0xf0) {
        count = 4;
        codepoint = first & 0x07;
        minimum = 0x10000;
    } else {
        return false;
    }
    if (count > available) return false;
    for (size_t i = 1; i < count; ++i) {
        if ((text[i] & 0xc0) != 0x80) return false;
        codepoint = (codepoint << 6) | (text[i] & 0x3f);
    }
    if (codepoint < minimum || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
    *value = codepoint;
    *length = count;
    return true;
}

static bool valid_utf8(const unsigned char *text, size_t length)
{
    for (size_t offset = 0; offset < length;) {
        unsigned value;
        size_t count;
        if (!utf8_codepoint(text + offset, length - offset, &value, &count)) return false;
        offset += count;
    }
    return true;
}

static bool windows_1252_byte(unsigned codepoint, unsigned char *value)
{
    static const unsigned mapping[32] = {
        0x20ac, 0,      0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
        0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017d, 0,
        0,      0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0,      0x017e, 0x0178,
    };
    if (codepoint <= 0xff) {
        *value = (unsigned char)codepoint;
        return true;
    }
    for (unsigned i = 0; i < 32; ++i) {
        if (mapping[i] == codepoint) {
            *value = (unsigned char)(0x80 + i);
            return true;
        }
    }
    return false;
}

void app_book_display_title(char *output, size_t capacity, const char *input)
{
    if (!output || capacity == 0) return;
    if (!input) input = "";

    size_t input_length = strlen(input);
    size_t used = 0;
    bool changed = false;
    bool repairable = true;
    for (size_t offset = 0; offset < input_length;) {
        unsigned codepoint;
        unsigned char byte;
        size_t count;
        if (!utf8_codepoint((const unsigned char *)input + offset,
                            input_length - offset, &codepoint, &count) ||
            !windows_1252_byte(codepoint, &byte) || used + 1 >= capacity) {
            repairable = false;
            break;
        }
        output[used++] = (char)byte;
        changed |= count > 1;
        offset += count;
    }
    if (repairable && changed && valid_utf8((const unsigned char *)output, used)) {
        output[used] = '\0';
    } else {
        snprintf(output, capacity, "%s", input);
    }

    char *extension = strrchr(output, '.');
    if (extension && (!strcasecmp(extension, ".epub") || !strcasecmp(extension, ".epu")))
        *extension = '\0';
}
