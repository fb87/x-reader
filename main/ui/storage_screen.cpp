#include "storage_screen.hpp"

#include <stdio.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{
void draw_storage(gfx::framebuffer_t* framebuffer, bool mounted, uint64_t total_bytes,
                  uint64_t free_bytes, uint16_t books, const char* mount_path)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t viewport = {framebuffer->width, framebuffer->height};
    const auto metrics = layout::metrics(viewport);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "Storage");
    const auto area = layout::inset(layout::content(viewport), metrics.margin);

    char total_text[32] = {};
    char free_text[32] = {};
    char books_text[24] = {};
    snprintf(total_text, sizeof(total_text), "%.1f GB",
             static_cast<double>(total_bytes) / (1024.0 * 1024.0 * 1024.0));
    snprintf(free_text, sizeof(free_text), "%.1f GB",
             static_cast<double>(free_bytes) / (1024.0 * 1024.0 * 1024.0));
    snprintf(books_text, sizeof(books_text), "%u", static_cast<unsigned>(books));

    const char* const labels[] = {"STATUS", "MOUNT", "TOTAL", "FREE", "BOOKS"};
    const char* const values[] = {mounted ? "MOUNTED" : "UNAVAILABLE",
                                  mount_path == nullptr ? "-" : mount_path, total_text, free_text,
                                  books_text};
    for (uint8_t index = 0; index < 5; ++index)
    {
        const auto row = layout::row(area, index, 5, static_cast<uint16_t>(metrics.row_height + 4U),
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
