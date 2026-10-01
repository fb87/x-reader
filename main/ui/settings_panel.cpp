#include "settings_panel.hpp"

#include "ui/chrome.hpp"
#include "ui/widgets.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static constexpr uint8_t paper = 0x0f;
static constexpr uint8_t ink = 0x00;
static constexpr uint8_t secondary = 0x06;
static constexpr uint8_t rule = 0x0c;

static layout::rect_t panel_area(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    layout::rect_t area = layout::content(vp);
    const uint16_t inset = m.display_class == layout::display_compact ? 14U : 20U;
    if (area.width > inset * 2U)
    {
        area.x = static_cast<uint16_t>(area.x + inset);
        area.width = static_cast<uint16_t>(area.width - inset * 2U);
    }
    return area;
}

static uint16_t row_height(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    // Sliders and segmented cells need more room than a plain text row.
    return static_cast<uint16_t>(m.row_height + 16U);
}

static layout::rect_t panel_row(layout::viewport_t vp, uint8_t index, uint8_t count)
{
    const layout::metrics_t m = layout::metrics(vp);
    return layout::stacked_row(panel_area(vp), index, count, row_height(vp), m.gap);
}

// Controls occupy the right-hand half of the row so the label always has room.
static layout::rect_t control_area(layout::rect_t row)
{
    const uint16_t padding = 12;
    const uint16_t width = static_cast<uint16_t>(row.width / 2U);
    if (row.width <= width + padding)
        return row;
    return {static_cast<uint16_t>(row.x + row.width - width - padding),
            static_cast<uint16_t>(row.y + (row.height - 28U) / 2U), width, 28};
}
} // namespace

void draw_settings_panel(gfx::framebuffer_t* framebuffer, const char* title,
                         const settings_row_t* rows, uint8_t count, uint8_t focus)
{
    if (framebuffer == nullptr || rows == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    gfx::clear(framebuffer, paper);
    chrome::draw_status_bar(framebuffer, title);

    const uint16_t glyph = gfx::icon_advance(1);
    for (uint8_t index = 0; index < count; ++index)
    {
        const settings_row_t& spec = rows[index];
        const layout::rect_t row = panel_row(vp, index, count);
        if (row.height == 0)
            continue;
        const bool selected = index == focus;
        // Focus-move redraws use the panel's fast 1-bit-only refresh mode, which
        // thresholds every pixel to pure black/white -- a subtle gray wash is
        // invisible under it. Inverting to a solid black row with white content
        // survives that threshold instead of relying on an intermediate gray.
        const uint8_t row_ink = selected ? paper : ink;
        const uint8_t row_secondary = selected ? paper : secondary;
        gfx::fill_rect(framebuffer, row.x, row.y, row.width, row.height, selected ? ink : paper);
        gfx::fill_rect(framebuffer, row.x, static_cast<uint16_t>(row.y + row.height - 1U),
                       row.width, 1, rule);

        uint16_t text_x = static_cast<uint16_t>(row.x + 10U);
        if (spec.icon != gfx::icon_none)
        {
            gfx::draw_icon(framebuffer, text_x,
                           static_cast<uint16_t>(row.y + (row.height - glyph) / 2U), spec.icon, 1,
                           row_ink);
            text_x = static_cast<uint16_t>(text_x + glyph + 10U);
        }
        gfx::draw_text(framebuffer, text_x, static_cast<uint16_t>(row.y + (row.height - 24U) / 2U),
                       spec.label == nullptr ? "" : spec.label, 1, row_ink);

        const layout::rect_t control = control_area(row);
        switch (spec.control)
        {
        case settings_control_toggle:
            widgets::draw_toggle(framebuffer, widgets::toggle_bounds(row, 12U), spec.on);
            break;
        case settings_control_slider:
            widgets::draw_slider(framebuffer, control, spec.level, spec.max_level);
            break;
        case settings_control_segmented:
            widgets::draw_segmented(framebuffer, control, spec.segments, spec.segment_count,
                                    spec.segment_selected);
            break;
        case settings_control_link:
        {
            const uint16_t arrow_x = row.width > glyph + 10U
                                         ? static_cast<uint16_t>(row.x + row.width - glyph - 10U)
                                         : row.x;
            if (spec.value != nullptr && spec.value[0] != '\0')
            {
                const uint16_t width = gfx::measure_text(spec.value, 1);
                if (arrow_x > width + 8U)
                    gfx::draw_text(framebuffer, static_cast<uint16_t>(arrow_x - width - 8U),
                                   static_cast<uint16_t>(row.y + (row.height - 24U) / 2U),
                                   spec.value, 1, row_secondary);
            }
            gfx::draw_icon(framebuffer, arrow_x,
                           static_cast<uint16_t>(row.y + (row.height - glyph) / 2U),
                           gfx::icon_chevron_right, 1, row_secondary);
            break;
        }
        case settings_control_value:
        default:
        {
            const char* value = spec.value == nullptr ? "" : spec.value;
            const uint16_t width = gfx::measure_text(value, 1);
            const uint16_t x = row.width > width + 12U
                                   ? static_cast<uint16_t>(row.x + row.width - width - 12U)
                                   : row.x;
            gfx::draw_text(framebuffer, x, static_cast<uint16_t>(row.y + (row.height - 24U) / 2U),
                           value, 1, row_secondary);
            break;
        }
        }
    }

    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_list},
                                {"Change", gfx::icon_swap_horiz}, {"Back", gfx::icon_arrow_back});
}

bool settings_panel_hit(layout::viewport_t viewport, const settings_row_t* rows, uint8_t count,
                        uint16_t x, uint16_t y, settings_hit_t* hit)
{
    if (rows == nullptr || hit == nullptr)
        return false;
    const layout::viewport_t vp = viewport;
    for (uint8_t index = 0; index < count; ++index)
    {
        const layout::rect_t row = panel_row(vp, index, count);
        if (row.height == 0 || !layout::contains(row, x, y))
            continue;
        hit->index = index;
        hit->has_value = false;
        hit->value = 0;
        const layout::rect_t control = control_area(row);
        if (rows[index].control == settings_control_slider)
        {
            uint8_t value = 0;
            if (widgets::slider_value_at(control, x, y, rows[index].max_level, &value))
            {
                hit->has_value = true;
                hit->value = value;
            }
        }
        else if (rows[index].control == settings_control_segmented)
        {
            uint8_t selected = 0;
            if (widgets::segmented_index_at(control, rows[index].segment_count, x, y, &selected))
            {
                hit->has_value = true;
                hit->value = selected;
            }
        }
        return true;
    }
    return false;
}

} // namespace ui
} // namespace xreader
