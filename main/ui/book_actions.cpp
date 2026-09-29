#include "book_actions.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"
#include "ui/navigation/focus.hpp"
namespace xreader { namespace ui {
namespace { static layout::rect_t area_for(layout::viewport_t vp) {
    layout::rect_t content = layout::content(vp);
    const uint16_t title_h = chrome::title_height(vp);
    if (content.height > title_h) { content.y = static_cast<uint16_t>(content.y + title_h); content.height = static_cast<uint16_t>(content.height - title_h); }
    return layout::inset(content, layout::metrics(vp).margin);
} }
void draw_book_actions(gfx::framebuffer_t* framebuffer, book_action_item_t focus, const char* title)
{
    if (framebuffer == nullptr) return;
    const layout::viewport_t vp{framebuffer->width, framebuffer->height};
    const auto m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "Book Actions");
    chrome::draw_title_bar(framebuffer, title == nullptr || title[0] == '\0' ? "Untitled" : title,
                           nullptr);
    const char* labels[book_action_item_count] = {"OPEN", "RENAME", "DELETE", "BACK"};
    const auto area = area_for(vp);
    for (uint8_t i = 0; i < book_action_item_count; ++i)
    {
        const auto r = layout::row(area, i, book_action_item_count, static_cast<uint16_t>(m.row_height + 4U), m.gap);
        const bool selected = i == static_cast<uint8_t>(focus);
        gfx::fill_rect(framebuffer, r.x, r.y, r.width, r.height, selected ? 0x0d : 0x0f);
        gfx::draw_text(framebuffer, static_cast<uint16_t>(r.x + 14U),
                       static_cast<uint16_t>(r.y + (r.height - 16U) / 2U), labels[i], 1,
                       0x00);
    }
    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_list},
                                {"Select", gfx::icon_check},
                                {"Back", gfx::icon_arrow_back});
}
bool book_actions_touch_item(uint16_t width, uint16_t height, uint16_t x, uint16_t y, book_action_item_t* item)
{
    if (item == nullptr) return false;
    const layout::viewport_t vp{width,height}; const auto m=layout::metrics(vp); uint8_t index=0;
    if (!focus::hit_rows(area_for(vp), book_action_item_count, static_cast<uint16_t>(m.row_height+4U), m.gap, x, y, &index)) return false;
    *item=static_cast<book_action_item_t>(index); return true;
}
} }
