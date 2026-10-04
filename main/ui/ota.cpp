#include "ota.hpp"

#include <stdio.h>
#include <string.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"
#include "ui/navigation/focus.hpp"

namespace xreader
{
namespace ui
{
namespace
{
static layout::rect_t items_area(layout::viewport_t viewport)
{
    return layout::inset(layout::content(viewport), layout::metrics(viewport).margin);
}
} // namespace

void draw_ota(gfx::framebuffer_t* framebuffer, ota_item_t focus, const char* current_version,
              const char* available_version, const char* status, bool update_available,
              const char* release_notes, uint8_t progress_percent, const char* last_result,
              bool rollback_pending, int8_t footer_focus)
{
    if (framebuffer == nullptr)
        return;

    const layout::viewport_t viewport = {framebuffer->width, framebuffer->height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "System Update");

    const char* const labels[ota_item_count] = {"CHECK FOR UPDATE", "INSTALL UPDATE", "BACK"};
    char install_value[48] = {};
    if (status != nullptr && strcmp(status, "INSTALLING...") == 0)
        snprintf(install_value, sizeof(install_value), "%u%%",
                 static_cast<unsigned>(progress_percent));
    else if (update_available)
        snprintf(install_value, sizeof(install_value), "%s",
                 available_version != nullptr && available_version[0] != '\0' ? available_version
                                                                              : "READY");
    else
        snprintf(install_value, sizeof(install_value), "NONE");
    const char* const values[ota_item_count] = {
        current_version == nullptr ? "UNKNOWN" : current_version,
        install_value,
        "RETURN",
    };

    const layout::rect_t area = items_area(viewport);
    for (uint8_t index = 0; index < ota_item_count; ++index)
    {
        const layout::rect_t item =
            layout::row(area, index, ota_item_count, static_cast<uint16_t>(metrics.row_height + 4U),
                        metrics.gap);
        const bool selected = footer_focus < 0 && index == static_cast<uint8_t>(focus);
        // Focus-move redraws use the panel's fast 1-bit-only refresh mode,
        // which thresholds every pixel to pure black/white -- a subtle gray
        // wash is invisible under it. Inverting to a solid black row with
        // white content survives that threshold instead.
        gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height,
                       selected ? 0x00 : 0x0f);
        const uint8_t foreground = selected ? 0x0f : 0x00;
        const uint8_t secondary = selected ? 0x0f : 0x00;
        gfx::draw_text(framebuffer, static_cast<uint16_t>(item.x + 14U),
                       static_cast<uint16_t>(item.y + (item.height - 24U) / 2U), labels[index], 1,
                       foreground);
        const uint16_t value_width = gfx::measure_text(values[index], 1);
        const uint16_t value_x =
            item.width > value_width + 14U
                ? static_cast<uint16_t>(item.x + item.width - value_width - 14U)
                : item.x;
        gfx::draw_text(framebuffer, value_x,
                       static_cast<uint16_t>(item.y + (item.height - 24U) / 2U), values[index], 1,
                       secondary);
    }
    uint16_t note_y = viewport.height > metrics.footer_height + 52U
                          ? static_cast<uint16_t>(viewport.height - metrics.footer_height - 48U)
                          : 0U;
    if (release_notes != nullptr && release_notes[0] != '\0')
    {
        char note[96] = {};
        snprintf(note, sizeof(note), "NOTES: %.82s", release_notes);
        gfx::draw_text(framebuffer, metrics.margin, note_y, note, 1, 0x06);
        note_y = static_cast<uint16_t>(note_y + 26U);
    }
    if (last_result != nullptr && last_result[0] != '\0')
    {
        char result[112] = {};
        snprintf(result, sizeof(result), "%sLAST: %.86s",
                 rollback_pending ? "VERIFY PENDING  " : "", last_result);
        gfx::draw_text(framebuffer, metrics.margin, note_y, result, 1, 0x06);
    }
    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_list}, {"Select", gfx::icon_check},
                                {"Back", gfx::icon_arrow_back}, footer_focus);
}

bool ota_touch_item(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y,
                    ota_item_t* item)
{
    if (item == nullptr)
        return false;
    const layout::viewport_t viewport = {display_width, display_height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    uint8_t index = 0;
    if (!focus::hit_rows(items_area(viewport), ota_item_count,
                         static_cast<uint16_t>(metrics.row_height + 4U), metrics.gap, x, y, &index))
        return false;
    *item = static_cast<ota_item_t>(index);
    return true;
}

} // namespace ui
} // namespace xreader
