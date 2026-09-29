#include "book_info.hpp"

#include <stdio.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"

namespace xreader::ui
{

void draw_book_info(gfx::framebuffer_t* framebuffer, const epub::book_t* book, uint8_t spine_index,
                    uint8_t spine_count)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp{framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "BOOK INFO", "EPUB");
    auto body = layout::inset(layout::content(vp), m.margin);

    const char* title = book && book->title[0] ? book->title : "Unknown title";
    const char* author = book && book->author[0] ? book->author : "Unknown author";
    gfx::draw_text(framebuffer, body.x, body.y, title, 2, 0x00);
    gfx::draw_text(framebuffer, body.x, static_cast<uint16_t>(body.y + 42U), author, 1, 0x05);

    const uint16_t rule_y = static_cast<uint16_t>(body.y + 72U);
    gfx::fill_rect(framebuffer, body.x, rule_y, body.width, 1, 0x0b);

    char value[48] = {};
    snprintf(value, sizeof(value), "%u", static_cast<unsigned>(spine_count));
    const char* labels[] = {"FORMAT", "CHAPTERS", "CURRENT CHAPTER", "NAVIGATION"};
    char current[48] = {};
    snprintf(current, sizeof(current), "%u / %u", static_cast<unsigned>(spine_index + 1U),
             static_cast<unsigned>(spine_count));
    const char* values[] = {"EPUB", value, current, "TOC / BOOKMARKS"};
    const uint16_t list_y = static_cast<uint16_t>(rule_y + 18U);
    for (uint8_t i = 0; i < 4; ++i)
    {
        const uint16_t y = static_cast<uint16_t>(list_y + i * (m.row_height + 4U));
        gfx::draw_text(framebuffer, body.x, y, labels[i], 1, 0x06);
        const uint16_t w = gfx::measure_text(values[i], 1);
        gfx::draw_text(framebuffer, static_cast<uint16_t>(body.x + body.width - w), y, values[i], 1,
                       0x00);
        gfx::fill_rect(framebuffer, body.x, static_cast<uint16_t>(y + 25U), body.width, 1, 0x0d);
    }
    chrome::draw_indication_bar(framebuffer, "", "", "BACK");
}

bool book_info_back_hit(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y)
{
    const layout::viewport_t vp{display_width, display_height};
    const auto m = layout::metrics(vp);
    return y >= static_cast<uint16_t>(display_height - m.footer_height) &&
           x >= display_width * 2U / 3U;
}

} // namespace xreader::ui
