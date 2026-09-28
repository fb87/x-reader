#include "layout.hpp"

namespace xreader
{
namespace ui
{
namespace layout
{

viewport_t viewport(uint16_t width, uint16_t height)
{
    return {width, height};
}

display_class_t classify(viewport_t vp)
{
    if (vp.width <= 800 || vp.height <= 480)
        return display_compact;
    if (vp.width <= 1200 || vp.height <= 720)
        return display_medium;
    return display_large;
}

metrics_t metrics(viewport_t vp)
{
    metrics_t result = {};
    result.display_class = classify(vp);
    if (result.display_class == display_compact)
    {
        result.margin = 24;
        result.gap = 6;
        result.status_height = 32;
        result.footer_height = 34;
        result.row_height = 44;
        result.panel_padding = 18;
    }
    else if (result.display_class == display_medium)
    {
        result.margin = 40;
        result.gap = 8;
        result.status_height = 36;
        result.footer_height = 36;
        result.row_height = 52;
        result.panel_padding = 22;
    }
    else
    {
        result.margin = 52;
        result.gap = 10;
        result.status_height = 42;
        result.footer_height = 42;
        result.row_height = 60;
        result.panel_padding = 28;
    }
    return result;
}

bool contains(rect_t rect, uint16_t x, uint16_t y)
{
    return x >= rect.x && y >= rect.y && x < static_cast<uint32_t>(rect.x) + rect.width &&
           y < static_cast<uint32_t>(rect.y) + rect.height;
}

rect_t inset(rect_t rect, uint16_t amount)
{
    if (rect.width <= static_cast<uint32_t>(amount) * 2U ||
        rect.height <= static_cast<uint32_t>(amount) * 2U)
        return rect;
    rect.x = static_cast<uint16_t>(rect.x + amount);
    rect.y = static_cast<uint16_t>(rect.y + amount);
    rect.width = static_cast<uint16_t>(rect.width - amount * 2U);
    rect.height = static_cast<uint16_t>(rect.height - amount * 2U);
    return rect;
}

rect_t content(viewport_t vp)
{
    const metrics_t m = metrics(vp);
    const uint32_t chrome_height = static_cast<uint32_t>(m.status_height) + m.footer_height;
    const uint16_t height =
        vp.height > chrome_height ? static_cast<uint16_t>(vp.height - chrome_height) : 0;
    return {0, m.status_height, vp.width, height};
}

rect_t row(rect_t area, uint8_t index, uint8_t count, uint16_t preferred_height,
           uint16_t preferred_gap)
{
    if (count == 0 || index >= count)
        return {};
    const uint32_t gaps = static_cast<uint32_t>(count - 1U) * preferred_gap;
    uint16_t height = preferred_height;
    if (static_cast<uint32_t>(height) * count + gaps > area.height)
    {
        const uint16_t available =
            static_cast<uint16_t>(area.height > gaps ? area.height - gaps : 0);
        height = count > 0 ? static_cast<uint16_t>(available / count) : 0;
    }
    const uint32_t total = static_cast<uint32_t>(height) * count + gaps;
    const uint16_t top =
        total < area.height ? static_cast<uint16_t>((area.height - total) / 2U) : 0;
    return {area.x, static_cast<uint16_t>(area.y + top + index * (height + preferred_gap)),
            area.width, height};
}

rect_t centered_panel(viewport_t vp, uint8_t width_percent, uint8_t height_percent)
{
    if (width_percent > 100)
        width_percent = 100;
    if (height_percent > 100)
        height_percent = 100;
    const uint16_t width =
        static_cast<uint16_t>(static_cast<uint32_t>(vp.width) * width_percent / 100U);
    const uint16_t height =
        static_cast<uint16_t>(static_cast<uint32_t>(vp.height) * height_percent / 100U);
    return {static_cast<uint16_t>((vp.width - width) / 2U),
            static_cast<uint16_t>((vp.height - height) / 2U), width, height};
}

} // namespace layout
} // namespace ui
} // namespace xreader
