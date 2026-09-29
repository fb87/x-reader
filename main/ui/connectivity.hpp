#pragma once
#include "gfx/framebuffer.hpp"
#include <stdint.h>
namespace xreader
{
namespace ui
{
enum connectivity_item_t : uint8_t
{
    connectivity_wifi,
    connectivity_network,
    connectivity_forget_network,
    connectivity_sync_server,
    connectivity_back,
    connectivity_item_count
};
void draw_connectivity(gfx::framebuffer_t* framebuffer, connectivity_item_t focus,
                       bool wifi_enabled, bool connected, const char* ssid, const char* ip,
                       bool sync_server_configured, const char* status);
bool connectivity_touch_item(uint16_t width, uint16_t height, uint16_t x, uint16_t y,
                             connectivity_item_t* item);
} // namespace ui
} // namespace xreader
