#include "splash.hpp"

#include "gfx/font.hpp"
#include "ui/layout/layout.hpp"
#include "ui/widgets.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static constexpr uint8_t paper = 0x0f;
static constexpr uint8_t ink = 0x00;
static constexpr uint8_t mid = 0x07;
} // namespace

void draw_splash(gfx::framebuffer_t* framebuffer, const char* status)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    gfx::clear(framebuffer, paper);

    const uint8_t icon_scale = m.display_class == layout::display_compact ? 4U : 5U;
    const uint16_t icon_side = gfx::icon_advance(icon_scale);
    const char* const title = "X-Reader";
    const char* const subtitle = "EPUB Reader for ESP32";
    const uint16_t title_width = gfx::measure_text(title, 2);
    const uint16_t subtitle_width = gfx::measure_text(subtitle, 1);

    // Stack icon, title, and subtitle as one block, centred as a unit rather
    // than each line centred independently, so the block sits in the same
    // visual centre regardless of which lines are present.
    const uint16_t block_height = static_cast<uint16_t>(icon_side + 16U + 28U + 10U + 16U);
    uint16_t y =
        static_cast<uint16_t>(vp.height > block_height ? (vp.height - block_height) / 2U : 0U);

    gfx::draw_icon(framebuffer, static_cast<uint16_t>((vp.width - icon_side) / 2U), y,
                   gfx::icon_book, icon_scale, ink);
    y = static_cast<uint16_t>(y + icon_side + 16U);

    gfx::draw_text(framebuffer, static_cast<uint16_t>((vp.width - title_width) / 2U), y, title, 2,
                   ink);
    y = static_cast<uint16_t>(y + 28U);

    gfx::draw_text(framebuffer, static_cast<uint16_t>((vp.width - subtitle_width) / 2U), y,
                   subtitle, 1, mid);
    y = static_cast<uint16_t>(y + 10U);

    // A plain rule rather than an animated progress bar: boot has no real
    // percentage to report, and an unfilled/looping bar on e-paper just means
    // extra partial refreshes for no information.
    const uint16_t rule_width = static_cast<uint16_t>(vp.width / 3U);
    widgets::draw_progress(
        framebuffer, {static_cast<uint16_t>((vp.width - rule_width) / 2U), y, rule_width, 3}, 1, 1);

    if (status != nullptr && status[0] != '\0')
    {
        const uint16_t status_width = gfx::measure_text(status, 1);
        const uint16_t status_y = static_cast<uint16_t>(vp.height > 32U ? vp.height - 32U : 0U);
        gfx::draw_text(framebuffer, static_cast<uint16_t>((vp.width - status_width) / 2U), status_y,
                       status, 1, mid);
    }
}

} // namespace ui
} // namespace xreader
