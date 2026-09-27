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
    if (character < 32 || character > 127)
    {
        character = '?';
    }
    return widtbl_f16[static_cast<uint8_t>(character) - 32];
}

static const unsigned char* glyph_data(char character)
{
    if (character < 32 || character > 127)
    {
        character = '?';
    }
    return chrtbl_f16[static_cast<uint8_t>(character) - 32];
}

static size_t decode_utf8(const char* text, uint32_t* codepoint)
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

static const unicode_glyph_t* unicode_glyph(uint32_t codepoint)
{
    for (size_t index = 0; index < unicode_glyph_count; ++index)
    {
        if (unicode_glyphs[index].codepoint == codepoint)
            return &unicode_glyphs[index];
    }
    return nullptr;
}

static void draw_unicode_glyph(framebuffer_t* framebuffer, uint16_t x, uint16_t y,
                               const unicode_glyph_t* glyph, uint8_t scale, uint8_t value)
{
    for (uint8_t row = 0; row < 16; ++row)
    {
        const uint16_t bits =
            static_cast<uint16_t>(glyph->bitmap[row * 2] << 8) | glyph->bitmap[row * 2 + 1];
        for (uint8_t column = 0; column < glyph->width; ++column)
        {
            if ((bits & (1U << (15 - column))) != 0)
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

uint16_t draw_text(framebuffer_t* framebuffer, uint16_t x, uint16_t y, const char* text,
                   uint8_t scale, uint8_t value)
{
    if (framebuffer == nullptr || text == nullptr || scale == 0)
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
            const size_t consumed = decode_utf8(text, &codepoint);
            const unicode_glyph_t* glyph = codepoint > 0x7fU ? unicode_glyph(codepoint) : nullptr;
            if (glyph != nullptr)
            {
                draw_unicode_glyph(framebuffer, cursor, y, glyph, scale, value);
                cursor = static_cast<uint16_t>(cursor + (glyph->width + 1) * scale);
            }
            else
            {
                const char character = codepoint <= 0x7fU ? static_cast<char>(codepoint) : '?';
                draw_glyph(framebuffer, cursor, y, character, scale, value);
                cursor = static_cast<uint16_t>(cursor + (glyph_width(character) + 1) * scale);
            }
            text += consumed;
            continue;
        }
        ++text;
    }
    return cursor - x;
}

} // namespace gfx
} // namespace xreader
