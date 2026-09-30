#include "widgets.hpp"

namespace xreader
{
namespace ui
{
namespace widgets
{

namespace
{
static constexpr uint8_t paper = 0x0f;
static constexpr uint8_t ink = 0x00;
static constexpr uint8_t mid = 0x08;
static constexpr uint8_t rule = 0x0b;
static constexpr uint8_t focus_wash = 0x0d;

static constexpr uint16_t slider_knob = 16;
static constexpr uint16_t track_thickness = 4;

// There is no circle primitive in gfx, and e-paper renders crisp rectangles
// better than dithered curves at this size, so knobs and pills are drawn as
// rectangles with their corner pixels trimmed.
static void rounded_fill(gfx::framebuffer_t* framebuffer, layout::rect_t bounds, uint8_t value)
{
    if (bounds.width < 3U || bounds.height < 3U)
    {
        gfx::fill_rect(framebuffer, bounds.x, bounds.y, bounds.width, bounds.height, value);
        return;
    }
    gfx::fill_rect(framebuffer, static_cast<uint16_t>(bounds.x + 1U), bounds.y,
                   static_cast<uint16_t>(bounds.width - 2U), bounds.height, value);
    gfx::fill_rect(framebuffer, bounds.x, static_cast<uint16_t>(bounds.y + 1U), 1,
                   static_cast<uint16_t>(bounds.height - 2U), value);
    gfx::fill_rect(framebuffer, static_cast<uint16_t>(bounds.x + bounds.width - 1U),
                   static_cast<uint16_t>(bounds.y + 1U), 1,
                   static_cast<uint16_t>(bounds.height - 2U), value);
}

static uint16_t clamp_u16(uint32_t value, uint16_t limit)
{
    return value > limit ? limit : static_cast<uint16_t>(value);
}
} // namespace

void draw_slider(gfx::framebuffer_t* framebuffer, layout::rect_t bounds, uint8_t value,
                 uint8_t max_value)
{
    if (framebuffer == nullptr || bounds.width <= slider_knob || max_value == 0)
        return;
    const uint16_t track_y =
        static_cast<uint16_t>(bounds.y + (bounds.height - track_thickness) / 2U);
    const uint16_t span = static_cast<uint16_t>(bounds.width - slider_knob);
    const uint16_t filled = clamp_u16(
        static_cast<uint32_t>(span) * (value > max_value ? max_value : value) / max_value, span);

    gfx::fill_rect(framebuffer, static_cast<uint16_t>(bounds.x + slider_knob / 2U), track_y, span,
                   track_thickness, rule);
    if (filled != 0)
        gfx::fill_rect(framebuffer, static_cast<uint16_t>(bounds.x + slider_knob / 2U), track_y,
                       filled, track_thickness, ink);

    const layout::rect_t knob = {
        static_cast<uint16_t>(bounds.x + filled),
        static_cast<uint16_t>(bounds.y + (bounds.height - slider_knob) / 2U), slider_knob,
        slider_knob};
    rounded_fill(framebuffer, knob, ink);
}

bool slider_value_at(layout::rect_t bounds, uint16_t x, uint16_t y, uint8_t max_value,
                     uint8_t* value)
{
    if (value == nullptr || max_value == 0 || bounds.width <= slider_knob)
        return false;
    if (!layout::contains(bounds, x, y))
        return false;
    const uint16_t span = static_cast<uint16_t>(bounds.width - slider_knob);
    const uint16_t origin = static_cast<uint16_t>(bounds.x + slider_knob / 2U);
    const uint16_t offset = x <= origin ? 0U : static_cast<uint16_t>(x - origin);
    const uint32_t scaled = static_cast<uint32_t>(offset > span ? span : offset) * max_value;
    *value = static_cast<uint8_t>((scaled + span / 2U) / span);
    return true;
}

layout::rect_t toggle_bounds(layout::rect_t row, uint16_t padding)
{
    static constexpr uint16_t width = 44;
    static constexpr uint16_t height = 24;
    if (row.width <= width + padding)
        return row;
    return {static_cast<uint16_t>(row.x + row.width - width - padding),
            static_cast<uint16_t>(row.y + (row.height - height) / 2U), width, height};
}

void draw_toggle(gfx::framebuffer_t* framebuffer, layout::rect_t bounds, bool on)
{
    if (framebuffer == nullptr || bounds.width < 8U || bounds.height < 8U)
        return;
    rounded_fill(framebuffer, bounds, on ? ink : paper);
    if (!on)
    {
        gfx::draw_rect(framebuffer, bounds.x, bounds.y, bounds.width, bounds.height, mid);
    }
    const uint16_t knob = static_cast<uint16_t>(bounds.height - 6U);
    const uint16_t knob_x = on ? static_cast<uint16_t>(bounds.x + bounds.width - knob - 3U)
                               : static_cast<uint16_t>(bounds.x + 3U);
    const layout::rect_t handle = {knob_x, static_cast<uint16_t>(bounds.y + 3U), knob, knob};
    rounded_fill(framebuffer, handle, on ? paper : mid);
}

void draw_segmented(gfx::framebuffer_t* framebuffer, layout::rect_t bounds,
                    const gfx::icon_t* icons, uint8_t count, uint8_t selected)
{
    if (framebuffer == nullptr || icons == nullptr || count == 0 || bounds.width < count)
        return;
    const uint16_t cell = static_cast<uint16_t>(bounds.width / count);
    const uint16_t glyph = gfx::icon_advance(1);
    for (uint8_t index = 0; index < count; ++index)
    {
        const uint16_t x = static_cast<uint16_t>(bounds.x + index * cell);
        const uint16_t width =
            index == count - 1U ? static_cast<uint16_t>(bounds.x + bounds.width - x) : cell;
        gfx::fill_rect(framebuffer, x, bounds.y, width, bounds.height,
                       index == selected ? focus_wash : paper);
        gfx::draw_rect(framebuffer, x, bounds.y, width, bounds.height,
                       index == selected ? ink : rule);
        if (width > glyph && bounds.height > glyph)
            gfx::draw_icon(framebuffer, static_cast<uint16_t>(x + (width - glyph) / 2U),
                           static_cast<uint16_t>(bounds.y + (bounds.height - glyph) / 2U),
                           icons[index], 1, ink);
    }
}

bool segmented_index_at(layout::rect_t bounds, uint8_t count, uint16_t x, uint16_t y,
                        uint8_t* index)
{
    if (index == nullptr || count == 0 || !layout::contains(bounds, x, y))
        return false;
    const uint16_t cell = static_cast<uint16_t>(bounds.width / count);
    if (cell == 0)
        return false;
    const uint16_t offset = static_cast<uint16_t>(x - bounds.x);
    const uint8_t resolved = static_cast<uint8_t>(offset / cell);
    *index = resolved >= count ? static_cast<uint8_t>(count - 1U) : resolved;
    return true;
}

void draw_progress(gfx::framebuffer_t* framebuffer, layout::rect_t bounds, uint32_t value,
                   uint32_t total)
{
    if (framebuffer == nullptr || bounds.width == 0 || bounds.height == 0)
        return;
    gfx::fill_rect(framebuffer, bounds.x, bounds.y, bounds.width, bounds.height, rule);
    if (total == 0)
        return;
    const uint32_t capped = value > total ? total : value;
    const uint16_t filled =
        clamp_u16(static_cast<uint32_t>(bounds.width) * capped / total, bounds.width);
    if (filled != 0)
        gfx::fill_rect(framebuffer, bounds.x, bounds.y, filled, bounds.height, ink);
}

uint16_t draw_breadcrumb(gfx::framebuffer_t* framebuffer, layout::rect_t bounds,
                         const char* const* parts, uint8_t count)
{
    if (framebuffer == nullptr || parts == nullptr || count == 0)
        return 0;
    const uint16_t glyph = gfx::icon_advance(1);
    const uint16_t text_y = static_cast<uint16_t>(bounds.y + (bounds.height - 24U) / 2U);
    const uint16_t icon_y = static_cast<uint16_t>(bounds.y + (bounds.height - glyph) / 2U);
    uint16_t cursor = bounds.x;
    for (uint8_t index = 0; index < count; ++index)
    {
        if (parts[index] == nullptr)
            continue;
        if (index != 0)
        {
            if (cursor + glyph > bounds.x + bounds.width)
                break;
            gfx::draw_icon(framebuffer, cursor, icon_y, gfx::icon_chevron_right, 1, mid);
            cursor = static_cast<uint16_t>(cursor + glyph);
        }
        const uint16_t width = gfx::measure_text(parts[index], 1);
        if (cursor + width > bounds.x + bounds.width)
            break;
        // The trailing element is the current location, so give it full contrast.
        gfx::draw_text(framebuffer, cursor, text_y, parts[index], 1,
                       index == count - 1U ? ink : mid);
        cursor = static_cast<uint16_t>(cursor + width);
    }
    return static_cast<uint16_t>(cursor - bounds.x);
}

void draw_button(gfx::framebuffer_t* framebuffer, layout::rect_t bounds, const char* label,
                 gfx::icon_t icon, bool focused)
{
    if (framebuffer == nullptr || bounds.width == 0 || bounds.height == 0)
        return;
    gfx::fill_rect(framebuffer, bounds.x, bounds.y, bounds.width, bounds.height,
                   focused ? ink : focus_wash);
    gfx::draw_rect(framebuffer, bounds.x, bounds.y, bounds.width, bounds.height, ink);
    const uint8_t foreground = focused ? paper : ink;

    const uint16_t glyph = icon == gfx::icon_none ? 0U : gfx::icon_advance(1);
    const uint16_t label_width = label == nullptr ? 0U : gfx::measure_text(label, 1);
    const uint16_t gap = (glyph != 0 && label_width != 0) ? 8U : 0U;
    const uint16_t total = static_cast<uint16_t>(glyph + gap + label_width);
    uint16_t cursor = bounds.width > total
                          ? static_cast<uint16_t>(bounds.x + (bounds.width - total) / 2U)
                          : bounds.x;
    if (glyph != 0)
    {
        gfx::draw_icon(framebuffer, cursor,
                       static_cast<uint16_t>(bounds.y + (bounds.height - glyph) / 2U), icon, 1,
                       foreground);
        cursor = static_cast<uint16_t>(cursor + glyph + gap);
    }
    if (label_width != 0)
        gfx::draw_text(framebuffer, cursor,
                       static_cast<uint16_t>(bounds.y + (bounds.height - 24U) / 2U), label, 1,
                       foreground);
}

bool button_contains(layout::rect_t bounds, uint16_t x, uint16_t y)
{
    return layout::contains(bounds, x, y);
}

} // namespace widgets
} // namespace ui
} // namespace xreader
