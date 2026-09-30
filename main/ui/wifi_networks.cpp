#include "wifi_networks.hpp"

#include <stdio.h>

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
static constexpr uint8_t extra_items = 2; // RESCAN + HIDDEN NETWORK

static layout::rect_t list_area(layout::viewport_t viewport)
{
    layout::rect_t content = layout::content(viewport);
    const uint16_t title_h = chrome::title_height(viewport);
    if (content.height > title_h)
    {
        content.y = static_cast<uint16_t>(content.y + title_h);
        content.height = static_cast<uint16_t>(content.height - title_h);
    }
    return layout::inset(content, layout::metrics(viewport).margin);
}
} // namespace

void draw_wifi_networks(gfx::framebuffer_t* framebuffer, const wifi_network_view_t* networks,
                        uint8_t count, uint8_t focus, const char* status)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t viewport = {framebuffer->width, framebuffer->height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "Wi-Fi Networks");
    chrome::draw_title_bar(framebuffer, "Networks", status);

    const uint8_t visible_networks = count > 6U ? 6U : count;
    const uint8_t total = static_cast<uint8_t>(visible_networks + extra_items);
    const layout::rect_t area = list_area(viewport);
    for (uint8_t index = 0; index < total; ++index)
    {
        const layout::rect_t row = layout::row(
            area, index, total, static_cast<uint16_t>(metrics.row_height + 2U), metrics.gap);
        const bool selected = index == focus;
        gfx::fill_rect(framebuffer, row.x, row.y, row.width, row.height, selected ? 0x0d : 0x0f);
        const uint8_t foreground = 0x00;
        if (index < visible_networks)
        {
            const wifi_network_view_t& network = networks[index];
            gfx::draw_text(framebuffer, static_cast<uint16_t>(row.x + 12U),
                           static_cast<uint16_t>(row.y + (row.height - 16U) / 2U), network.ssid, 1,
                           foreground);
            char info[24] = {};
            snprintf(info, sizeof(info), "%s %d dBm", network.secured ? "LOCK" : "OPEN",
                     static_cast<int>(network.rssi));
            const uint16_t info_width = gfx::measure_text(info, 1);
            const uint16_t info_x =
                row.width > info_width + 12U
                    ? static_cast<uint16_t>(row.x + row.width - info_width - 12U)
                    : row.x;
            gfx::draw_text(framebuffer, info_x,
                           static_cast<uint16_t>(row.y + (row.height - 16U) / 2U), info, 1,
                           selected ? 0x04 : 0x06);
        }
        else
        {
            const char* label = index == visible_networks ? "RESCAN" : "ADD HIDDEN NETWORK";
            gfx::draw_text(framebuffer, static_cast<uint16_t>(row.x + 12U),
                           static_cast<uint16_t>(row.y + (row.height - 16U) / 2U), label, 1,
                           foreground);
        }
    }
    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_list}, {"Select", gfx::icon_check},
                                {"Back", gfx::icon_arrow_back});
}

bool wifi_networks_touch_item(uint16_t width, uint16_t height, uint16_t x, uint16_t y,
                              uint8_t count, uint8_t* selected)
{
    if (selected == nullptr)
        return false;
    const layout::viewport_t viewport = {width, height};
    const layout::metrics_t metrics = layout::metrics(viewport);
    const uint8_t visible = count > 6U ? 6U : count;
    const uint8_t total = static_cast<uint8_t>(visible + extra_items);
    return focus::hit_rows(list_area(viewport), total,
                           static_cast<uint16_t>(metrics.row_height + 2U), metrics.gap, x, y,
                           selected);
}

} // namespace ui
} // namespace xreader
