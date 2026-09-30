#include "chrome.hpp"

#include <stdio.h>
#include <time.h>

#include "gfx/font.hpp"

namespace xreader
{
namespace ui
{
namespace chrome
{

namespace
{
static constexpr uint8_t paper = 0x0f;
static constexpr uint8_t ink = 0x00;
static constexpr uint8_t mid = 0x08;
static constexpr uint8_t rule = 0x0b;
static bool battery_available = false;
static uint8_t battery_percent_value = 0;
static bool clock_visible = true;

static layout::metrics_t framebuffer_metrics(const gfx::framebuffer_t* framebuffer)
{
    return layout::metrics({framebuffer->width, framebuffer->height});
}

// The ESP32 RTC starts at the epoch and is only meaningful once something (SNTP,
// or the board RTC) has set it.  Anything before 2020 is an unset clock.
static bool local_clock(char* output, size_t capacity)
{
    const time_t now = time(nullptr);
    struct tm parts = {};
    localtime_r(&now, &parts);
    if (parts.tm_year + 1900 < 2020)
    {
        snprintf(output, capacity, "--:--");
        return false;
    }
    snprintf(output, capacity, "%02d:%02d", parts.tm_hour, parts.tm_min);
    return true;
}

// Drawn rather than taken from the icon table so the fill tracks the real charge
// instead of showing a single "full" glyph at every level.
static void draw_battery(gfx::framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint8_t percent)
{
    static constexpr uint16_t body_width = 22;
    static constexpr uint16_t body_height = 12;
    gfx::draw_rect(framebuffer, x, y, body_width, body_height, ink);
    gfx::fill_rect(framebuffer, static_cast<uint16_t>(x + body_width), static_cast<uint16_t>(y + 3),
                   2, 6, ink);
    const uint16_t inner = static_cast<uint16_t>(body_width - 4U);
    const uint16_t filled = static_cast<uint16_t>(inner * percent / 100U);
    if (filled != 0)
        gfx::fill_rect(framebuffer, static_cast<uint16_t>(x + 2), static_cast<uint16_t>(y + 2),
                       filled, static_cast<uint16_t>(body_height - 4U), ink);
}
} // namespace

uint16_t status_height(const gfx::framebuffer_t* framebuffer)
{
    return framebuffer == nullptr ? 0 : framebuffer_metrics(framebuffer).status_height;
}

uint16_t indication_height(const gfx::framebuffer_t* framebuffer)
{
    return framebuffer == nullptr ? 0 : framebuffer_metrics(framebuffer).footer_height;
}

uint16_t title_height(layout::viewport_t viewport)
{
    return layout::metrics(viewport).display_class == layout::display_compact ? 40U : 46U;
}

void set_battery_status(bool available, uint8_t percent)
{
    battery_available = available;
    battery_percent_value = percent > 100U ? 100U : percent;
}

void set_clock_visible(bool visible)
{
    clock_visible = visible;
}

void draw_status_bar(gfx::framebuffer_t* framebuffer, const char* title)
{
    if (framebuffer == nullptr)
        return;
    const layout::metrics_t m = framebuffer_metrics(framebuffer);
    const uint16_t padding = m.display_class == layout::display_compact ? 14 : 20;
    const uint16_t text_y = static_cast<uint16_t>((m.status_height - 24U) / 2U);
    gfx::fill_rect(framebuffer, 0, 0, framebuffer->width, m.status_height, paper);

    const layout::rect_t home_button =
        layout::status_home_bounds({framebuffer->width, framebuffer->height});
    const uint16_t home_glyph = gfx::icon_advance(1);
    gfx::draw_icon(framebuffer, static_cast<uint16_t>(home_button.x + (home_button.width - home_glyph) / 2U),
                   static_cast<uint16_t>(home_button.y + (home_button.height - home_glyph) / 2U),
                   gfx::icon_home, 1, mid);
    const uint16_t clock_x = static_cast<uint16_t>(home_button.width + 6U);

    if (clock_visible)
    {
        char clock[8] = {};
        (void)local_clock(clock, sizeof(clock));
        gfx::draw_text(framebuffer, clock_x, text_y, clock, 1, mid);
    }

    if (title != nullptr && title[0] != '\0')
    {
        const uint16_t width = gfx::measure_text(title, 1);
        const uint16_t x = framebuffer->width > width
                               ? static_cast<uint16_t>((framebuffer->width - width) / 2U)
                               : 0;
        gfx::draw_text(framebuffer, x, text_y, title, 1, ink);
    }

    if (battery_available)
    {
        char percent[8] = {};
        snprintf(percent, sizeof(percent), "%u%%", static_cast<unsigned>(battery_percent_value));
        const uint16_t percent_width = gfx::measure_text(percent, 1);
        const uint16_t percent_x =
            framebuffer->width > percent_width + padding
                ? static_cast<uint16_t>(framebuffer->width - percent_width - padding)
                : padding;
        gfx::draw_text(framebuffer, percent_x, text_y, percent, 1, ink);
        if (percent_x > 30U)
            draw_battery(framebuffer, static_cast<uint16_t>(percent_x - 30U),
                         static_cast<uint16_t>((m.status_height - 12U) / 2U),
                         battery_percent_value);
    }

    gfx::fill_rect(framebuffer, 0, static_cast<uint16_t>(m.status_height - 1U), framebuffer->width,
                   1, rule);
}

void draw_title_bar(gfx::framebuffer_t* framebuffer, const char* title, const char* trailing)
{
    if (framebuffer == nullptr || title == nullptr)
        return;
    const layout::metrics_t m = framebuffer_metrics(framebuffer);
    const uint16_t padding = m.display_class == layout::display_compact ? 14 : 20;
    const uint16_t top = m.status_height;
    const uint16_t height = title_height({framebuffer->width, framebuffer->height});
    gfx::fill_rect(framebuffer, 0, top, framebuffer->width, height, paper);
    gfx::draw_text(framebuffer, padding, static_cast<uint16_t>(top + (height - 24U) / 2U), title, 1,
                   ink);
    if (trailing != nullptr && trailing[0] != '\0')
    {
        const uint16_t width = gfx::measure_text(trailing, 1);
        const uint16_t x = framebuffer->width > width + padding
                               ? static_cast<uint16_t>(framebuffer->width - width - padding)
                               : padding;
        gfx::draw_text(framebuffer, x, static_cast<uint16_t>(top + (height - 24U) / 2U), trailing,
                       1, mid);
    }
    gfx::fill_rect(framebuffer, padding, static_cast<uint16_t>(top + height - 1U),
                   static_cast<uint16_t>(framebuffer->width - padding * 2U), 1, rule);
}

void draw_indication_bar(gfx::framebuffer_t* framebuffer, footer_cell_t left, footer_cell_t center,
                         footer_cell_t right)
{
    if (framebuffer == nullptr)
        return;
    const layout::metrics_t m = framebuffer_metrics(framebuffer);
    const uint16_t top = static_cast<uint16_t>(framebuffer->height - m.footer_height);
    const uint16_t third = static_cast<uint16_t>(framebuffer->width / 3U);
    gfx::fill_rect(framebuffer, 0, top, framebuffer->width, m.footer_height, paper);
    gfx::fill_rect(framebuffer, 0, top, framebuffer->width, 1, rule);

    const footer_cell_t cells[3] = {left, center, right};
    const uint16_t glyph = gfx::icon_advance(1);
    // Centre the icon+label stack as one block rather than pinning it near the
    // top: the extra height added for finger comfort was otherwise just blank
    // space below the label, which read as an unbalanced, "weird" footer.
    const uint16_t label_gap = 4U;
    const uint16_t label_height = 16U;
    const uint16_t block_height = static_cast<uint16_t>(glyph + label_gap + label_height);
    const uint16_t block_top = m.footer_height > block_height
                                  ? static_cast<uint16_t>(top + (m.footer_height - block_height) / 2U)
                                  : top;
    for (uint8_t index = 0; index < 3; ++index)
    {
        const footer_cell_t cell = cells[index];
        const uint16_t x = static_cast<uint16_t>(index * third);
        const uint16_t width = index == 2 ? static_cast<uint16_t>(framebuffer->width - x) : third;
        if (cell.label == nullptr && cell.icon == gfx::icon_none)
            continue;

        const uint8_t foreground = cell.disabled ? mid : ink;
        const bool has_label = cell.label != nullptr && cell.label[0] != '\0';
        const uint16_t label_width = has_label ? gfx::measure_text(cell.label, 1) : 0;
        if (cell.icon != gfx::icon_none)
        {
            const uint16_t icon_x =
                width > glyph ? static_cast<uint16_t>(x + (width - glyph) / 2U) : x;
            gfx::draw_icon(framebuffer, icon_x, block_top, cell.icon, 1, foreground);
        }
        if (has_label)
        {
            const uint16_t label_x =
                width > label_width ? static_cast<uint16_t>(x + (width - label_width) / 2U) : x;
            const uint16_t label_y =
                cell.icon == gfx::icon_none
                    ? static_cast<uint16_t>(top + (m.footer_height - label_height) / 2U)
                    : static_cast<uint16_t>(block_top + glyph + label_gap);
            gfx::draw_text(framebuffer, label_x, label_y, cell.label, 1, foreground);
        }
        if (index != 0)
            gfx::fill_rect(framebuffer, x, static_cast<uint16_t>(top + 8U), 1,
                           static_cast<uint16_t>(m.footer_height - 16U), rule);
    }
}

} // namespace chrome
} // namespace ui
} // namespace xreader
