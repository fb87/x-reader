#pragma once

#include <stddef.h>
#include <stdint.h>

namespace xreader::epub::css
{

enum text_align_t : uint8_t
{
    align_inherit,
    align_left,
    align_center,
    align_right,
    align_justify,
};

struct style_t
{
    bool hidden;
    bool block;
    bool preformatted;
    bool page_break_before;
    bool page_break_after;
    text_align_t text_align;
};

// Parse the small inline-CSS subset that materially affects the text/e-paper
// reader. Unknown properties are intentionally ignored.
style_t parse_inline(const char* text);

} // namespace xreader::epub::css
