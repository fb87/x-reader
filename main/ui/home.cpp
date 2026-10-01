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
    "Continue reading", "Library", "Recent books", "Settings", "Sleep",
};

static layout::rect_t home_area(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    // The mockup uses a compact editorial list rather than oversized launcher cards.
    // Keep horizontal breathing room but use less vertical inset so all actions retain
    // consistent row heights on 800x480 and 960x540 displays.
    layout::rect_t content = layout::content(vp);
    const uint16_t horizontal = m.display_class == layout::display_compact ? 18U : 28U;
    const uint16_t vertical = m.display_class == layout::display_compact ? 10U : 14U;
    if (content.width > horizontal * 2U)
    {
        content.x = static_cast<uint16_t>(content.x + horizontal);
        content.width = static_cast<uint16_t>(content.width - horizontal * 2U);
    }
    if (content.height > vertical * 2U)
    {
        content.y = static_cast<uint16_t>(content.y + vertical);
        content.height = static_cast<uint16_t>(content.height - vertical * 2U);
    }
    return content;
}

static layout::rect_t home_row(layout::viewport_t vp, uint8_t index)
{
    const layout::metrics_t m = layout::metrics(vp);
    const layout::rect_t area = home_area(vp);
    // Rows must stay at least 44 px tall or draw_home_row drops the subtitle.
    // Portrait has height to spare, so grow the rows to fill instead of leaving
    // the list floating in the middle of the screen.
    const uint16_t gap = m.gap;
    const uint32_t gaps = static_cast<uint32_t>(home_action_count - 1U) * gap;
    const uint16_t fill =
        static_cast<uint16_t>((area.height > gaps ? area.height - gaps : 0) / home_action_count);
    uint16_t height = fill < 48U ? 48U : fill;
    if (height > 72U)
        height = 72U;
    return layout::stacked_row(area, index, home_action_count, height, gap);
}

static const char* subtitle_for(uint8_t index, const char* book_title)
{
    if (index == home_continue_reading)
        return book_title == nullptr || book_title[0] == '\0' ? "No book opened" : book_title;
    if (index == home_library)
        return "Browse books on SD card";
    if (index == home_recent_books)
        return "Recently opened books";
    if (index == home_settings)
        return "Display, reading, books and network";
    return "Suspend the device";
}

static void draw_home_row(gfx::framebuffer_t* framebuffer, layout::rect_t row, uint8_t index,
                          const char* subtitle, bool selected)
{
    // Focus-move redraws use the panel's fast 1-bit-only refresh mode, which
    // thresholds every pixel to pure black/white -- a subtle gray wash is
    // invisible under it. Inverting to a solid black row with white content
    // survives that threshold instead of relying on an intermediate gray.
    const uint8_t background = selected ? 0x00 : 0x0f;
    const uint8_t foreground = selected ? 0x0f : 0x00;
    const uint8_t secondary = selected ? 0x0f : 0x06;
    gfx::fill_rect(framebuffer, row.x, row.y, row.width, row.height, background);
    if (!selected)
        gfx::fill_rect(framebuffer, row.x, static_cast<uint16_t>(row.y + row.height - 1U),
                       row.width, 1U, 0x0d);

    const gfx::icon_t icons[home_action_count] = {
        gfx::icon_book, gfx::icon_list, gfx::icon_history, gfx::icon_settings, gfx::icon_bedtime,
    };
    const uint16_t glyph = gfx::icon_advance(1);
    const uint16_t icon_x = static_cast<uint16_t>(row.x + 10U);
    const uint16_t icon_y = static_cast<uint16_t>(row.y + (row.height - glyph) / 2U);
    gfx::draw_icon(framebuffer, icon_x, icon_y, icons[index], 1, foreground);

    // Two stacked 1x lines (title + subtitle) need 5 (top margin) + 24 (title
    // glyph height) + 1 (gap) + 24 (subtitle glyph height) = 54 px; below that,
    // fall back to a single centred title line rather than let the subtitle
    // spill past the row.
    const bool two_lines = row.height >= 54U;
    const uint16_t text_x = static_cast<uint16_t>(icon_x + glyph + 12U);
    const uint16_t title_y = two_lines ? static_cast<uint16_t>(row.y + 5U)
                                       : static_cast<uint16_t>(row.y + (row.height - 24U) / 2U);
    gfx::draw_text(framebuffer, text_x, title_y, menu_labels[index], 1, foreground);
    if (two_lines && subtitle != nullptr && subtitle[0] != '\0')
        gfx::draw_text(framebuffer, text_x, static_cast<uint16_t>(row.y + 30U), subtitle, 1,
                       secondary);

    if (index != home_sleep)
    {
        const uint16_t arrow_x =
            row.width > glyph + 8U ? static_cast<uint16_t>(row.x + row.width - glyph - 8U) : row.x;
        gfx::draw_icon(framebuffer, arrow_x,
                       static_cast<uint16_t>(row.y + (row.height - glyph) / 2U),
                       gfx::icon_chevron_right, 1, secondary);
    }
}
} // namespace

void draw_home(gfx::framebuffer_t* framebuffer, bool storage_mounted, const char* book_title,
               home_action_t focus)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, storage_mounted ? "X-Reader" : "X-Reader (No SD)");

    for (uint8_t index = 0; index < home_action_count; ++index)
        draw_home_row(framebuffer, home_row(vp, index), index, subtitle_for(index, book_title),
                      index == static_cast<uint8_t>(focus));

    chrome::draw_indication_bar(framebuffer, {"Library", gfx::icon_list}, {"Open", gfx::icon_book},
                                {"Settings", gfx::icon_settings});
}

bool home_touch_action(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y,
                       home_action_t* action)
{
    if (action == nullptr)
        return false;
    const layout::viewport_t vp = {display_width, display_height};
    for (uint8_t index = 0; index < home_action_count; ++index)
    {
        if (layout::contains(home_row(vp, index), x, y))
        {
            *action = static_cast<home_action_t>(index);
            return true;
        }
    }
    return false;
}

} // namespace ui
} // namespace xreader
