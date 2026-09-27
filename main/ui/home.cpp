#include "home.hpp"

#include "gfx/font.hpp"
#include "ui/chrome.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static constexpr uint16_t menu_left = 40;
static constexpr uint16_t menu_top = 150;
static constexpr uint16_t menu_height = 48;
static constexpr uint16_t menu_gap = 8;
static const char* const menu_labels[home_action_count] = {
    "CONTINUE READING", "LIBRARY", "RECENT BOOKS", "SETTINGS", "SLEEP",
};
} // namespace

void draw_home(gfx::framebuffer_t* framebuffer, bool storage_mounted, const char* book_title,
               home_action_t focus)
{
    if (framebuffer == nullptr)
        return;
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "HOME", storage_mounted ? "SD" : "NO SD");
    gfx::draw_text(framebuffer, 40, 62, "WELCOME TO XREADER", 2, 0x00);
    gfx::draw_text(framebuffer, 40, 94,
                   book_title == nullptr || book_title[0] == '\0' ? "LOADING BOOK..." : book_title,
                   1, 0x00);
    for (uint8_t index = 0; index < home_action_count; ++index)
    {
        const uint16_t y = static_cast<uint16_t>(menu_top + index * (menu_height + menu_gap));
        const bool selected = index == static_cast<uint8_t>(focus);
        if (selected)
            gfx::fill_rect(framebuffer, menu_left, y, framebuffer->width - menu_left * 2,
                           menu_height, 0x00);
        gfx::draw_text(framebuffer, static_cast<uint16_t>(menu_left + 16),
                       static_cast<uint16_t>(y + 16), menu_labels[index], 1,
                       selected ? 0x0f : 0x00);
        if (!selected)
            gfx::draw_rect(framebuffer, menu_left, y, framebuffer->width - menu_left * 2,
                           menu_height, 0x04);
    }
    chrome::draw_indication_bar(framebuffer, "ROTARY", "SELECT", "PRESS");
}

bool home_touch_action(uint16_t y, home_action_t* action)
{
    if (action == nullptr || y < menu_top)
        return false;
    const uint16_t stride = menu_height + menu_gap;
    const uint16_t index = static_cast<uint16_t>((y - menu_top) / stride);
    if (index >= home_action_count || (y - menu_top) % stride >= menu_height)
        return false;
    *action = static_cast<home_action_t>(index);
    return true;
}

} // namespace ui
} // namespace xreader
