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
static constexpr uint8_t paper = 0x0f;
static constexpr uint8_t ink = 0x00;
static constexpr uint8_t mid = 0x08;

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
    const uint16_t padding = m.display_class == layout::display_compact ? 18 : 26;
    gfx::fill_rect(framebuffer, 0, 0, framebuffer->width, m.status_height, paper);
    gfx::draw_text(framebuffer, padding, static_cast<uint16_t>((m.status_height - 16U) / 2U),
                   left == nullptr ? "" : left, 1, ink);
    const uint16_t right_width = gfx::measure_text(right == nullptr ? "" : right, 1);
    const uint16_t right_x = framebuffer->width > right_width + padding
                                 ? static_cast<uint16_t>(framebuffer->width - right_width - padding)
                                 : padding;
    gfx::draw_text(framebuffer, right_x, static_cast<uint16_t>((m.status_height - 16U) / 2U),
                   right == nullptr ? "" : right, 1, mid);
    gfx::fill_rect(framebuffer, padding, static_cast<uint16_t>(m.status_height - 2U),
                   static_cast<uint16_t>(framebuffer->width - padding * 2U), 1, ink);
}

void draw_indication_bar(gfx::framebuffer_t* framebuffer, const char* left, const char* center,
                         const char* right)
{
    if (framebuffer == nullptr)
        return;
    const layout::metrics_t m = framebuffer_metrics(framebuffer);
    const uint16_t top = static_cast<uint16_t>(framebuffer->height - m.footer_height);
    const uint16_t third = static_cast<uint16_t>(framebuffer->width / 3U);
    gfx::fill_rect(framebuffer, 0, top, framebuffer->width, m.footer_height, paper);
    gfx::fill_rect(framebuffer, 0, top, framebuffer->width, 1, ink);

    const char* labels[3] = {left == nullptr ? "" : left, center == nullptr ? "" : center,
                             right == nullptr ? "" : right};
    for (uint8_t index = 0; index < 3; ++index)
    {
        const uint16_t x = static_cast<uint16_t>(index * third);
        const uint16_t width = index == 2 ? static_cast<uint16_t>(framebuffer->width - x) : third;
        if (index == 1)
            gfx::fill_rect(framebuffer, x, static_cast<uint16_t>(top + 4U), width,
                           static_cast<uint16_t>(m.footer_height - 8U), ink);
        const uint16_t text_width = gfx::measure_text(labels[index], 1);
        const uint16_t text_x =
            width > text_width ? static_cast<uint16_t>(x + (width - text_width) / 2U) : x;
        const uint16_t text_y = static_cast<uint16_t>(top + (m.footer_height - 16U) / 2U);
        gfx::draw_text(framebuffer, text_x, text_y, labels[index], 1, index == 1 ? paper : ink);
        if (index != 0)
            gfx::fill_rect(framebuffer, x, static_cast<uint16_t>(top + 8U), 1,
                           static_cast<uint16_t>(m.footer_height - 16U), mid);
    }
}

} // namespace chrome
} // namespace ui
} // namespace xreader
