#include "css.hpp"

#include <ctype.h>
#include <string.h>

namespace xreader::epub::css
{
namespace
{

static bool equal_ci(const char* text, size_t length, const char* expected)
{
    const size_t expected_length = strlen(expected);
    if (length != expected_length)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (tolower(static_cast<unsigned char>(text[i])) !=
            tolower(static_cast<unsigned char>(expected[i])))
            return false;
    return true;
}

static void trim(const char** begin, const char** end)
{
    while (*begin < *end && isspace(static_cast<unsigned char>(**begin)))
        ++*begin;
    while (*end > *begin && isspace(static_cast<unsigned char>((*end)[-1])))
        --*end;
}

} // namespace

style_t parse_inline(const char* text)
{
    style_t result = {};
    result.text_align = align_inherit;
    if (text == nullptr)
        return result;

    const char* cursor = text;
    while (*cursor != '\0')
    {
        const char* declaration_end = strchr(cursor, ';');
        if (declaration_end == nullptr)
            declaration_end = cursor + strlen(cursor);
        const char* colon = static_cast<const char*>(
            memchr(cursor, ':', static_cast<size_t>(declaration_end - cursor)));
        if (colon != nullptr)
        {
            const char* name_begin = cursor;
            const char* name_end = colon;
            const char* value_begin = colon + 1;
            const char* value_end = declaration_end;
            trim(&name_begin, &name_end);
            trim(&value_begin, &value_end);
            const size_t name_length = static_cast<size_t>(name_end - name_begin);
            const size_t value_length = static_cast<size_t>(value_end - value_begin);

            if (equal_ci(name_begin, name_length, "display"))
            {
                result.hidden = equal_ci(value_begin, value_length, "none");
                result.block = equal_ci(value_begin, value_length, "block") ||
                               equal_ci(value_begin, value_length, "list-item");
            }
            else if (equal_ci(name_begin, name_length, "visibility"))
            {
                if (equal_ci(value_begin, value_length, "hidden"))
                    result.hidden = true;
            }
            else if (equal_ci(name_begin, name_length, "white-space"))
            {
                result.preformatted = equal_ci(value_begin, value_length, "pre") ||
                                      equal_ci(value_begin, value_length, "pre-wrap") ||
                                      equal_ci(value_begin, value_length, "break-spaces");
            }
            else if (equal_ci(name_begin, name_length, "text-align"))
            {
                if (equal_ci(value_begin, value_length, "left"))
                    result.text_align = align_left;
                else if (equal_ci(value_begin, value_length, "center"))
                    result.text_align = align_center;
                else if (equal_ci(value_begin, value_length, "right"))
                    result.text_align = align_right;
                else if (equal_ci(value_begin, value_length, "justify"))
                    result.text_align = align_justify;
            }
            else if (equal_ci(name_begin, name_length, "page-break-before") ||
                     equal_ci(name_begin, name_length, "break-before"))
            {
                result.page_break_before = equal_ci(value_begin, value_length, "always") ||
                                           equal_ci(value_begin, value_length, "page");
            }
            else if (equal_ci(name_begin, name_length, "page-break-after") ||
                     equal_ci(name_begin, name_length, "break-after"))
            {
                result.page_break_after = equal_ci(value_begin, value_length, "always") ||
                                          equal_ci(value_begin, value_length, "page");
            }
        }
        cursor = *declaration_end == ';' ? declaration_end + 1 : declaration_end;
    }
    return result;
}

} // namespace xreader::epub::css
