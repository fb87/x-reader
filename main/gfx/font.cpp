#include "font.hpp"

#include "unicode_font.hpp"

extern "C"
{
    extern const unsigned char widtbl_f16[96];
    extern const unsigned char* const chrtbl_f16[96];
}

namespace xreader
{
namespace gfx
{
namespace
{

static constexpr uint8_t font_height = 16;

static uint8_t glyph_width(char character)
{
    const uint8_t value = static_cast<uint8_t>(character);
    if (value < 32U || value > 127U)
    {
        character = '?';
    }
    return widtbl_f16[static_cast<uint8_t>(character) - 32];
}

static const unsigned char* glyph_data(char character)
{
    const uint8_t value = static_cast<uint8_t>(character);
    if (value < 32U || value > 127U)
    {
        character = '?';
    }
    return chrtbl_f16[static_cast<uint8_t>(character) - 32];
}

static size_t decode_utf8_impl(const char* text, uint32_t* codepoint)
{
    const uint8_t first = static_cast<uint8_t>(text[0]);
    if (first < 0x80U)
    {
        *codepoint = first;
        return 1;
    }
    if ((first & 0xe0U) == 0xc0U && (static_cast<uint8_t>(text[1]) & 0xc0U) == 0x80U)
    {
        *codepoint =
            (static_cast<uint32_t>(first & 0x1fU) << 6) | (static_cast<uint8_t>(text[1]) & 0x3fU);
        return 2;
    }
    if ((first & 0xf0U) == 0xe0U && (static_cast<uint8_t>(text[1]) & 0xc0U) == 0x80U &&
        (static_cast<uint8_t>(text[2]) & 0xc0U) == 0x80U)
    {
        *codepoint = (static_cast<uint32_t>(first & 0x0fU) << 12) |
                     (static_cast<uint32_t>(static_cast<uint8_t>(text[1]) & 0x3fU) << 6) |
                     (static_cast<uint8_t>(text[2]) & 0x3fU);
        return 3;
    }
    *codepoint = '?';
    return 1;
}

static int compare_composition(const unicode_composition_t* entry, uint32_t first, uint32_t second,
                               uint32_t third, uint8_t length)
{
    if (entry->first != first)
        return entry->first < first ? -1 : 1;
    if (entry->second != second)
        return entry->second < second ? -1 : 1;
    if (entry->third != third)
        return entry->third < third ? -1 : 1;
    if (entry->length != length)
        return entry->length < length ? -1 : 1;
    return 0;
}

static bool find_composition(uint32_t first, uint32_t second, uint32_t third, uint8_t length,
                             uint32_t* composed)
{
    size_t low = 0;
    size_t high = unicode_composition_count;
    while (low < high)
    {
        const size_t middle = low + (high - low) / 2;
        const int comparison =
            compare_composition(&unicode_compositions[middle], first, second, third, length);
        if (comparison < 0)
            low = middle + 1;
        else if (comparison > 0)
            high = middle;
        else
        {
            *composed = unicode_compositions[middle].composed;
            return true;
        }
    }
    return false;
}

static bool compose_unicode_impl(uint32_t first, uint32_t second, uint32_t third,
                                 uint32_t* composed, size_t* consumed_codepoints)
{
    if (composed == nullptr || consumed_codepoints == nullptr)
        return false;
    if (find_composition(first, second, third, 3, composed))
    {
        *consumed_codepoints = 3;
        return true;
    }
    if (find_composition(first, second, 0, 2, composed))
    {
        *consumed_codepoints = 2;
        return true;
    }
    return false;
}

static const unicode_glyph_t* unicode_glyph(uint32_t codepoint)
{
    size_t low = 0;
    size_t high = unicode_glyph_count;
    while (low < high)
    {
        const size_t middle = low + (high - low) / 2;
        const unicode_glyph_t* glyph = &unicode_glyphs[middle];
        if (glyph->codepoint < codepoint)
            low = middle + 1;
        else if (glyph->codepoint > codepoint)
            high = middle;
        else
            return glyph;
    }
    return nullptr;
}

static void draw_unicode_glyph(framebuffer_t* framebuffer, uint16_t x, uint16_t y,
                               const unicode_glyph_t* glyph, uint8_t scale, uint8_t value)
{
    for (uint8_t row = 0; row < unicode_glyph_height; ++row)
    {
        const uint8_t* packed = &glyph->bitmap[row * unicode_glyph_bytes_per_row];
        for (uint8_t column = 0; column < glyph->width; ++column)
        {
            if ((packed[column / 8U] & (0x80U >> (column % 8U))) == 0)
                continue;
            fill_rect(framebuffer, static_cast<uint16_t>(x + column * scale),
                      static_cast<uint16_t>(y + row * scale), scale, scale, value);
        }
    }
}

static void draw_glyph(framebuffer_t* framebuffer, uint16_t x, uint16_t y, char character,
                       uint8_t scale, uint8_t value)
{
    const uint8_t width = glyph_width(character);
    const uint8_t bytes_per_row = static_cast<uint8_t>((width + 7) / 8);
    const unsigned char* bitmap = glyph_data(character);
    for (uint8_t row = 0; row < font_height; ++row)
    {
        for (uint8_t column = 0; column < width; ++column)
        {
            const uint8_t byte = bitmap[row * bytes_per_row + column / 8];
            if ((byte & (1U << (7 - (column & 7)))) != 0)
            {
                fill_rect(framebuffer, static_cast<uint16_t>(x + column * scale),
                          static_cast<uint16_t>(y + row * scale), scale, scale, value);
            }
        }
    }
}

} // namespace

size_t decode_utf8(const char* text, uint32_t* codepoint)
{
    return decode_utf8_impl(text, codepoint);
}

bool compose_unicode(uint32_t first, uint32_t second, uint32_t third, uint32_t* composed,
                     size_t* consumed_codepoints)
{
    return compose_unicode_impl(first, second, third, composed, consumed_codepoints);
}

uint16_t glyph_advance(uint32_t codepoint, uint8_t scale)
{
    const unicode_glyph_t* glyph = unicode_glyph(codepoint);
    if (glyph != nullptr)
        return static_cast<uint16_t>(glyph->advance * scale);
    const char character = codepoint <= 0x7fU ? static_cast<char>(codepoint) : '?';
    return static_cast<uint16_t>((glyph_width(character) + 1U) * scale);
}

void draw_codepoint(framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint32_t codepoint,
                    uint8_t scale, uint8_t value)
{
    if (framebuffer == nullptr || scale == 0)
        return;
    const unicode_glyph_t* glyph = unicode_glyph(codepoint);
    if (glyph != nullptr)
    {
        draw_unicode_glyph(framebuffer, x, y, glyph, scale, value);
        return;
    }
    const char character = codepoint <= 0x7fU ? static_cast<char>(codepoint) : '?';
    draw_glyph(framebuffer, x, y, character, scale, value);
}

void draw_icon(framebuffer_t* framebuffer, uint16_t x, uint16_t y, icon_t icon, uint8_t scale,
               uint8_t value)
{
    if (framebuffer == nullptr || scale == 0 || icon >= icon_count)
        return;
    const icon_glyph_t* glyph = &icon_glyphs[icon];
    for (uint8_t row = 0; row < icon_size; ++row)
    {
        const uint8_t* packed = &glyph->bitmap[row * icon_stride];
        for (uint8_t column = 0; column < icon_size; ++column)
        {
            if ((packed[column / 8U] & (0x80U >> (column % 8U))) == 0)
                continue;
            fill_rect(framebuffer, static_cast<uint16_t>(x + column * scale),
                      static_cast<uint16_t>(y + row * scale), scale, scale, value);
        }
    }
}

uint16_t icon_advance(uint8_t scale)
{
    return static_cast<uint16_t>(icon_size * (scale == 0 ? 1 : scale));
}

uint16_t draw_text(framebuffer_t* framebuffer, uint16_t x, uint16_t y, const char* text,
                   uint8_t scale, uint8_t value)
{
    if (text == nullptr || scale == 0)
        return 0;
    uint16_t cursor = x;
    while (*text != '\0')
    {
        if (*text == '\n')
        {
            cursor = x;
            y = static_cast<uint16_t>(y + (font_height + 2) * scale);
        }
        else
        {
            uint32_t codepoint = 0;
            size_t consumed = decode_utf8(text, &codepoint);
            uint32_t second = 0;
            uint32_t third = 0;
            size_t second_bytes = 0;
            size_t third_bytes = 0;
            if (text[consumed] != '\0')
                second_bytes = decode_utf8(text + consumed, &second);
            if (second_bytes > 0 && second >= 0x0300U && second <= 0x036fU &&
                text[consumed + second_bytes] != '\0')
                third_bytes = decode_utf8(text + consumed + second_bytes, &third);
            uint32_t composed = 0;
            size_t consumed_codepoints = 0;
            if (second_bytes > 0 &&
                compose_unicode(codepoint, second, third, &composed, &consumed_codepoints))
            {
                codepoint = composed;
                consumed += second_bytes;
                if (consumed_codepoints == 3)
                    consumed += third_bytes;
            }
            if (framebuffer != nullptr)
                draw_codepoint(framebuffer, cursor, y, codepoint, scale, value);
            cursor = static_cast<uint16_t>(cursor + glyph_advance(codepoint, scale));
            text += consumed;
            continue;
        }
        ++text;
    }
    return cursor - x;
}

uint16_t measure_text(const char* text, uint8_t scale)
{
    return draw_text(nullptr, 0, 0, text, scale, 0);
}

} // namespace gfx
} // namespace xreader
