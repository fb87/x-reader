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
static constexpr uint16_t bar_padding = 20;
} // namespace

void draw_status_bar(gfx::framebuffer_t* framebuffer, const char* left, const char* right)
{
    if (framebuffer == nullptr)
        return;
    gfx::fill_rect(framebuffer, 0, 0, framebuffer->width, status_height, bar_background);
    gfx::draw_text(framebuffer, bar_padding, 10, left == nullptr ? "" : left, 1, bar_foreground);
    const uint16_t right_x =
        framebuffer->width > 140 ? static_cast<uint16_t>(framebuffer->width - 140) : bar_padding;
    gfx::draw_text(framebuffer, right_x, 10, right == nullptr ? "" : right, 1, bar_foreground);
    gfx::fill_rect(framebuffer, 0, status_height - 1, framebuffer->width, 1, body_foreground);
}

void draw_indication_bar(gfx::framebuffer_t* framebuffer, const char* left, const char* center,
                         const char* right)
{
    if (framebuffer == nullptr)
        return;
    const uint16_t top = static_cast<uint16_t>(framebuffer->height - indication_height);
    gfx::fill_rect(framebuffer, 0, top, framebuffer->width, indication_height, bar_background);
    gfx::draw_text(framebuffer, bar_padding, static_cast<uint16_t>(top + 10),
                   left == nullptr ? "" : left, 1, bar_foreground);
    const uint16_t center_x =
        framebuffer->width > 100 ? static_cast<uint16_t>(framebuffer->width / 2 - 50) : bar_padding;
    gfx::draw_text(framebuffer, center_x, static_cast<uint16_t>(top + 10),
                   center == nullptr ? "" : center, 1, bar_foreground);
    const uint16_t right_x =
        framebuffer->width > 140 ? static_cast<uint16_t>(framebuffer->width - 140) : bar_padding;
    gfx::draw_text(framebuffer, right_x, static_cast<uint16_t>(top + 10),
                   right == nullptr ? "" : right, 1, bar_foreground);
    gfx::fill_rect(framebuffer, 0, top, framebuffer->width, 1, body_foreground);
}

} // namespace chrome
} // namespace ui
} // namespace xreader
