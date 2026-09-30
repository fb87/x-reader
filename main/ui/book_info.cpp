#include "book_info.hpp"

#include <stdio.h>

#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"
#include "ui/widgets.hpp"

namespace xreader::ui
{

void draw_book_info(gfx::framebuffer_t* framebuffer, const epub::book_t* book, uint8_t spine_index,
                    uint8_t spine_count, const uint8_t* cover_pixels, uint16_t cover_pixel_width,
                    uint16_t cover_pixel_height)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp{framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "Book Information");
    auto body = layout::inset(layout::content(vp), m.margin);

    const char* title = book && book->title[0] ? book->title : "Unknown title";
    const char* author = book && book->author[0] ? book->author : "Unknown author";

    // Mockup 4 leads with a cover beside the title block.
    const uint16_t cover_w = static_cast<uint16_t>(body.width / 3U);
    const uint16_t cover_h = static_cast<uint16_t>(cover_w * 3U / 2U);
    gfx::draw_rect(framebuffer, body.x, body.y, cover_w, cover_h, 0x07);
    if (cover_pixels != nullptr && cover_pixel_width != 0U && cover_pixel_height != 0U)
    {
        // Fit the decoded cover inside the slot preserving aspect ratio
        // (blit_4bpp_scaled stretches to whatever rect it is given), letterboxed
        // rather than cropped so nothing the cover shows is cut off.
        uint16_t fit_w = cover_w;
        uint16_t fit_h = static_cast<uint16_t>(static_cast<uint32_t>(cover_pixel_height) * cover_w /
                                               cover_pixel_width);
        if (fit_h > cover_h)
        {
            fit_h = cover_h;
            fit_w = static_cast<uint16_t>(static_cast<uint32_t>(cover_pixel_width) * cover_h /
                                          cover_pixel_height);
        }
        const uint16_t fit_x = static_cast<uint16_t>(body.x + (cover_w - fit_w) / 2U);
        const uint16_t fit_y = static_cast<uint16_t>(body.y + (cover_h - fit_h) / 2U);
        gfx::blit_4bpp_scaled(framebuffer, fit_x, fit_y, fit_w, fit_h, cover_pixels,
                              cover_pixel_width, cover_pixel_height);
    }
    else
    {
        // No decoded cover (JPEG source, missing cache, or read/decode failure):
        // a labelled placeholder rather than an empty box.
        gfx::draw_icon(framebuffer,
                       static_cast<uint16_t>(body.x + (cover_w - gfx::icon_advance(1)) / 2U),
                       static_cast<uint16_t>(body.y + (cover_h - gfx::icon_advance(1)) / 2U),
                       gfx::icon_book, 1, 0x08);
    }

    const uint16_t text_x = static_cast<uint16_t>(body.x + cover_w + 16U);
    const uint16_t text_w =
        body.width > cover_w + 16U ? static_cast<uint16_t>(body.width - cover_w - 16U) : body.width;
    gfx::draw_text(framebuffer, text_x, body.y, title, 1, 0x00);
    gfx::draw_text(framebuffer, text_x, static_cast<uint16_t>(body.y + 22U), author, 1, 0x05);

    char chapters[16] = {};
    snprintf(chapters, sizeof(chapters), "%u", static_cast<unsigned>(spine_count));
    char current[24] = {};
    snprintf(current, sizeof(current), "%u / %u", static_cast<unsigned>(spine_index + 1U),
             static_cast<unsigned>(spine_count));
    const char* labels[] = {"Format", "Chapters", "Current chapter"};
    const char* values[] = {"EPUB", chapters, current};
    const uint16_t list_y = static_cast<uint16_t>(body.y + 52U);
    for (uint8_t i = 0; i < 3; ++i)
    {
        const uint16_t y = static_cast<uint16_t>(list_y + i * 24U);
        gfx::draw_text(framebuffer, text_x, y, labels[i], 1, 0x06);
        const uint16_t w = gfx::measure_text(values[i], 1);
        if (text_w > w)
            gfx::draw_text(framebuffer, static_cast<uint16_t>(text_x + text_w - w), y, values[i], 1,
                           0x00);
    }

    const uint16_t rule_y = static_cast<uint16_t>(body.y + cover_h + 18U);
    gfx::fill_rect(framebuffer, body.x, rule_y, body.width, 1, 0x0b);

    // Call to action, matching the mockup's filled "Open Book" button.
    const layout::rect_t open_button = book_info_open_bounds(vp);
    widgets::draw_button(framebuffer, open_button, "Open Book", gfx::icon_book, true);

    chrome::draw_indication_bar(framebuffer, {nullptr, gfx::icon_none}, {"Open", gfx::icon_book},
                                {"Back", gfx::icon_arrow_back});
}

layout::rect_t book_info_open_bounds(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    const layout::rect_t body = layout::inset(layout::content(vp), m.margin);
    const uint16_t height = 44;
    const uint16_t cover_h = static_cast<uint16_t>((body.width / 3U) * 3U / 2U);
    const uint16_t y = static_cast<uint16_t>(body.y + cover_h + 40U);
    return {body.x, y, body.width, height};
}

bool book_info_open_hit(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y)
{
    return widgets::button_contains(book_info_open_bounds({display_width, display_height}), x, y);
}

bool book_info_back_hit(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y)
{
    const layout::viewport_t vp{display_width, display_height};
    const auto m = layout::metrics(vp);
    return y >= static_cast<uint16_t>(display_height - m.footer_height) &&
           x >= display_width * 2U / 3U;
}

} // namespace xreader::ui
