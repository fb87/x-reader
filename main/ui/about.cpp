#include "about.hpp"

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{
void draw_about(gfx::framebuffer_t* framebuffer, const char* version, const char* board,
                const char* idf_version, const char* build_date)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t viewport = {framebuffer->width, framebuffer->height};
    const auto metrics = layout::metrics(viewport);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "About");
    const auto area = layout::inset(layout::content(viewport), metrics.margin);
    const char* const labels[] = {"VERSION", "BOARD", "ESP-IDF", "BUILD"};
    const char* const values[] = {version == nullptr ? "UNKNOWN" : version,
                                  board == nullptr ? "UNKNOWN" : board,
                                  idf_version == nullptr ? "UNKNOWN" : idf_version,
                                  build_date == nullptr ? "UNKNOWN" : build_date};
    for (uint8_t index = 0; index < 4; ++index)
    {
        const auto row = layout::row(area, index, 4, static_cast<uint16_t>(metrics.row_height + 8U),
                                     metrics.gap);
        gfx::draw_text(framebuffer, static_cast<uint16_t>(row.x + 12U),
                       static_cast<uint16_t>(row.y + 12U), labels[index], 1, 0x00);
        const uint16_t width = gfx::measure_text(values[index], 1);
        gfx::draw_text(framebuffer, static_cast<uint16_t>(row.x + row.width - (width + 12U)),
                       static_cast<uint16_t>(row.y + 12U), values[index], 1, 0x05);
    }
    chrome::draw_indication_bar(framebuffer, {nullptr, gfx::icon_none}, {nullptr, gfx::icon_none},
                                {"Back", gfx::icon_arrow_back});
}
} // namespace ui
} // namespace xreader
