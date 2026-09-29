#include "home.hpp"

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static const char* const menu_labels[home_action_count] = {
    "CONTINUE", "LIBRARY", "RECENT", "SETTINGS", "SLEEP",
};

static layout::rect_t continue_card(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    layout::rect_t body = layout::inset(layout::content(vp), m.margin);
    const uint16_t height = m.display_class == layout::display_compact ? 92 : 112;
    return {body.x, body.y, body.width, height};
}

static layout::rect_t action_card(layout::viewport_t vp, uint8_t index)
{
    const layout::metrics_t m = layout::metrics(vp);
    const layout::rect_t body = layout::inset(layout::content(vp), m.margin);
    const layout::rect_t hero = continue_card(vp);
    const uint16_t gap = m.gap;
    const uint16_t top = static_cast<uint16_t>(hero.y + hero.height + gap);
    const uint16_t column_gap = gap;
    const uint16_t row_gap = gap;
    const uint16_t card_w = body.width > column_gap
                                ? static_cast<uint16_t>((body.width - column_gap) / 2U)
                                : body.width;
    const uint8_t tile_count = static_cast<uint8_t>(home_action_count - 1U);
    const uint8_t row_count = static_cast<uint8_t>((tile_count + 1U) / 2U);

    // Do not stretch home tiles to consume all remaining vertical space.  The mockup
    // intentionally uses compact action tiles with breathing room below them.  A fixed
    // height derived from the standard row metric keeps the same visual proportions in
    // landscape and portrait layouts.
    uint16_t card_h = static_cast<uint16_t>(m.row_height + 20U);
    const uint16_t available_h =
        body.y + body.height > top ? static_cast<uint16_t>(body.y + body.height - top) : 0;
    const uint32_t gaps_h = row_count > 0U ? static_cast<uint32_t>(row_count - 1U) * row_gap : 0U;
    const uint32_t required_h = static_cast<uint32_t>(card_h) * row_count + gaps_h;
    if (available_h < required_h && row_count > 0U)
    {
        const uint16_t usable =
            available_h > gaps_h ? static_cast<uint16_t>(available_h - gaps_h) : 0;
        card_h = static_cast<uint16_t>(usable / row_count);
    }

    const uint8_t local = static_cast<uint8_t>(index - 1U);
    const uint8_t row = static_cast<uint8_t>(local / 2U);
    const uint8_t col = static_cast<uint8_t>(local % 2U);
    return {static_cast<uint16_t>(body.x + col * (card_w + column_gap)),
            static_cast<uint16_t>(top + row * (card_h + row_gap)), card_w, card_h};
}

static void draw_card(gfx::framebuffer_t* framebuffer, layout::rect_t card, const char* title,
                      const char* subtitle, bool selected)
{
    const uint8_t bg = selected ? 0x00 : 0x0f;
    const uint8_t fg = selected ? 0x0f : 0x00;
    gfx::fill_rect(framebuffer, card.x, card.y, card.width, card.height, bg);
    gfx::draw_rect(framebuffer, card.x, card.y, card.width, card.height, selected ? 0x00 : 0x07);
    const uint16_t pad = 16;
    gfx::draw_text(framebuffer, static_cast<uint16_t>(card.x + pad),
                   static_cast<uint16_t>(card.y + 14), title, 1, fg);
    if (subtitle != nullptr && subtitle[0] != '\0' && card.height >= 54)
        gfx::draw_text(framebuffer, static_cast<uint16_t>(card.x + pad),
                       static_cast<uint16_t>(card.y + 38), subtitle, 1, selected ? 0x0d : 0x06);
}
} // namespace

void draw_home(gfx::framebuffer_t* framebuffer, bool storage_mounted, const char* book_title,
               home_action_t focus)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "X-READER", storage_mounted ? "SD READY" : "NO SD");

    const layout::rect_t hero = continue_card(vp);
    draw_card(framebuffer, hero, "CONTINUE READING",
              book_title == nullptr || book_title[0] == '\0' ? "No book opened" : book_title,
              focus == home_continue_reading);

    for (uint8_t index = 1; index < home_action_count; ++index)
    {
        const layout::rect_t card = action_card(vp, index);
        const char* subtitle = index == home_library        ? "Browse your books"
                               : index == home_recent_books ? "Recently opened"
                               : index == home_book_manager ? "Import, remove, organize"
                               : index == home_book_sync    ? "Sync books and progress"
                               : index == home_settings     ? "Reader preferences"
                                                            : "Suspend device";
        draw_card(framebuffer, card, menu_labels[index], subtitle,
                  index == static_cast<uint8_t>(focus));
    }
    chrome::draw_indication_bar(framebuffer, "MOVE", "OPEN", "BACK");
}

bool home_touch_action(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y,
                       home_action_t* action)
{
    if (action == nullptr)
        return false;
    const layout::viewport_t vp = {display_width, display_height};
    if (layout::contains(continue_card(vp), x, y))
    {
        *action = home_continue_reading;
        return true;
    }
    for (uint8_t index = 1; index < home_action_count; ++index)
    {
        if (layout::contains(action_card(vp, index), x, y))
        {
            *action = static_cast<home_action_t>(index);
            return true;
        }
    }
    return false;
}

} // namespace ui
} // namespace xreader
