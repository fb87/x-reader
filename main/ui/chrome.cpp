#include "chrome.hpp"

#include "gfx/font.hpp"

namespace xreader
{
namespace ui
{
namespace chrome
{

namespace
{
static constexpr uint8_t bar_background = 0x00;
static constexpr uint8_t bar_foreground = 0x0f;
static constexpr uint8_t body_foreground = 0x00;

static layout::metrics_t framebuffer_metrics(const gfx::framebuffer_t* framebuffer)
{
    return layout::metrics({framebuffer->width, framebuffer->height});
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

void draw_status_bar(gfx::framebuffer_t* framebuffer, const char* left, const char* right)
{
    if (framebuffer == nullptr)
        return;
    const layout::metrics_t m = framebuffer_metrics(framebuffer);
    const uint16_t padding = m.margin > 24 ? static_cast<uint16_t>(m.margin / 2U) : 12;
    gfx::fill_rect(framebuffer, 0, 0, framebuffer->width, m.status_height, bar_background);
    gfx::draw_text(framebuffer, padding, static_cast<uint16_t>((m.status_height - 16U) / 2U),
                   left == nullptr ? "" : left, 1, bar_foreground);
    const uint16_t right_width = gfx::measure_text(right == nullptr ? "" : right, 1);
    const uint16_t right_x = framebuffer->width > right_width + padding
                                 ? static_cast<uint16_t>(framebuffer->width - right_width - padding)
                                 : padding;
    gfx::draw_text(framebuffer, right_x, static_cast<uint16_t>((m.status_height - 16U) / 2U),
                   right == nullptr ? "" : right, 1, bar_foreground);
    gfx::fill_rect(framebuffer, 0, static_cast<uint16_t>(m.status_height - 1U), framebuffer->width,
                   1, body_foreground);
}

void draw_indication_bar(gfx::framebuffer_t* framebuffer, const char* left, const char* center,
                         const char* right)
{
    if (framebuffer == nullptr)
        return;
    const layout::metrics_t m = framebuffer_metrics(framebuffer);
    const uint16_t padding = m.margin > 24 ? static_cast<uint16_t>(m.margin / 2U) : 12;
    const uint16_t top = static_cast<uint16_t>(framebuffer->height - m.footer_height);
    const uint16_t text_y = static_cast<uint16_t>(top + (m.footer_height - 16U) / 2U);
    gfx::fill_rect(framebuffer, 0, top, framebuffer->width, m.footer_height, bar_background);
    gfx::draw_text(framebuffer, padding, text_y, left == nullptr ? "" : left, 1, bar_foreground);

    const uint16_t center_width = gfx::measure_text(center == nullptr ? "" : center, 1);
    const uint16_t center_x = framebuffer->width > center_width
                                  ? static_cast<uint16_t>((framebuffer->width - center_width) / 2U)
                                  : 0;
    gfx::draw_text(framebuffer, center_x, text_y, center == nullptr ? "" : center, 1,
                   bar_foreground);

    const uint16_t right_width = gfx::measure_text(right == nullptr ? "" : right, 1);
    const uint16_t right_x = framebuffer->width > right_width + padding
                                 ? static_cast<uint16_t>(framebuffer->width - right_width - padding)
                                 : padding;
    gfx::draw_text(framebuffer, right_x, text_y, right == nullptr ? "" : right, 1, bar_foreground);
    gfx::fill_rect(framebuffer, 0, top, framebuffer->width, 1, body_foreground);
}

} // namespace chrome
} // namespace ui
} // namespace xreader
