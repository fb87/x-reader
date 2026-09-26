#include "font.hpp"

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
            draw_glyph(framebuffer, cursor, y, *text, scale, value);
            cursor = static_cast<uint16_t>(cursor + (glyph_width(*text) + 1) * scale);
        }
        ++text;
    }
    return cursor - x;
}

} // namespace gfx
} // namespace xreader
