#include "home.hpp"

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"
#include "ui/navigation/focus.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static const char* const menu_labels[home_action_count] = {
    "CONTINUE READING", "LIBRARY", "RECENT BOOKS", "SETTINGS", "SLEEP",
};

static layout::rect_t menu_area(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    layout::rect_t body = layout::inset(layout::content(vp), m.margin);
    const uint16_t heading = m.display_class == layout::display_compact ? 58 : 78;
    if (body.height > heading)
    {
        body.y = static_cast<uint16_t>(body.y + heading);
        body.height = static_cast<uint16_t>(body.height - heading);
    }
    return body;
}
} // namespace

void draw_home(gfx::framebuffer_t* framebuffer, bool storage_mounted, const char* book_title,
               home_action_t focus)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "HOME", storage_mounted ? "SD" : "NO SD");

    const layout::rect_t body = layout::inset(layout::content(vp), m.margin);
    const uint8_t heading_scale = m.display_class == layout::display_compact ? 1 : 2;
    gfx::draw_text(framebuffer, body.x, static_cast<uint16_t>(body.y + 8), "WELCOME TO XREADER",
                   heading_scale, 0x00);
    gfx::draw_text(
        framebuffer, body.x, static_cast<uint16_t>(body.y + (heading_scale == 1 ? 30 : 38)),
        book_title == nullptr || book_title[0] == '\0' ? "LOADING BOOK..." : book_title, 1, 0x04);

    const layout::rect_t menu = menu_area(vp);
    for (uint8_t index = 0; index < home_action_count; ++index)
    {
        const layout::rect_t item =
            layout::row(menu, index, home_action_count, m.row_height, m.gap);
        const bool selected = index == static_cast<uint8_t>(focus);
        if (selected)
            gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height, 0x00);
        else
            gfx::draw_rect(framebuffer, item.x, item.y, item.width, item.height, 0x04);
        const uint16_t text_y = static_cast<uint16_t>(item.y + (item.height - 16U) / 2U);
        gfx::draw_text(framebuffer, static_cast<uint16_t>(item.x + m.panel_padding), text_y,
                       menu_labels[index], 1, selected ? 0x0f : 0x00);
    }
    chrome::draw_indication_bar(framebuffer, "UP/DOWN", "SELECT", "BACK");
}

bool home_touch_action(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y,
                       home_action_t* action)
{
    if (action == nullptr)
        return false;
    const layout::viewport_t vp = {display_width, display_height};
    const layout::metrics_t m = layout::metrics(vp);
    uint8_t index = 0;
    if (!focus::hit_rows(menu_area(vp), home_action_count, m.row_height, m.gap, x, y, &index))
        return false;
    *action = static_cast<home_action_t>(index);
    return true;
}

} // namespace ui
} // namespace xreader
